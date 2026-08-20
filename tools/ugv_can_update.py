#!/usr/bin/env python3
"""Upload a bootloader-linked STM32 application over Linux SocketCAN."""

from __future__ import annotations

import argparse
import binascii
import secrets
import socket
import struct
import sys
import time
from dataclasses import dataclass
from pathlib import Path

CAN_FRAME = struct.Struct("=IB3x8s")
COMMAND = struct.Struct("<BBBBI")
STATUS = struct.Struct("<BBBBI")
DATA = struct.Struct("<H6s")

CAN_ID_COMMAND = 0x600
CAN_ID_DATA = {"left": 0x610, "right": 0x611}
CAN_ID_STATUS = {"left": 0x680, "right": 0x681}
NODE_ID = {"left": 0x10, "right": 0x11}

CMD_ENTER = 0x01
CMD_QUERY = 0x02
CMD_BEGIN = 0x10
CMD_FINISH = 0x11
CMD_ACTIVATE = 0x12
CMD_ABORT = 0x13

STATUS_IDLE = 0x00
STATUS_READY = 0x01
STATUS_ACK = 0x02
STATUS_VERIFIED = 0x03
STATUS_ERROR = 0x80

PROTOCOL_VERSION = 1
DATA_BYTES = 6
ACK_INTERVAL = 32
APP_ADDRESS = 0x08006000
METADATA_ADDRESS = 0x0801F800
APP_MAX_SIZE = 102 * 1024
SRAM_BASE = 0x20000000
SRAM_END = 0x20007FF8

ERROR_NAMES = {
    0: "none",
    1: "bad command",
    2: "bad session",
    3: "bad state",
    4: "invalid image size",
    5: "unexpected sequence",
    6: "flash erase failed",
    7: "flash write failed",
    8: "incomplete image",
    9: "CRC mismatch",
    10: "metadata write failed",
}


class UpdateError(RuntimeError):
    pass


@dataclass(frozen=True)
class Status:
    code: int
    session: int
    detail: int
    protocol_version: int
    value: int


class SocketCan:
    def __init__(self, interface: str):
        if not hasattr(socket, "AF_CAN"):
            raise UpdateError("this Python build does not support SocketCAN")
        self._socket = socket.socket(socket.AF_CAN, socket.SOCK_RAW, socket.CAN_RAW)
        self._socket.bind((interface,))

    def close(self) -> None:
        self._socket.close()

    def send(self, identifier: int, payload: bytes) -> None:
        if len(payload) != 8:
            raise ValueError("firmware-update CAN frames must have DLC 8")
        self._socket.send(CAN_FRAME.pack(identifier, len(payload), payload))

    def receive(self, timeout: float) -> tuple[int, bytes] | None:
        self._socket.settimeout(timeout)
        try:
            raw = self._socket.recv(CAN_FRAME.size)
        except TimeoutError:
            return None
        identifier, dlc, payload = CAN_FRAME.unpack(raw)
        return identifier & 0x7FF, payload[:dlc]


def build_command(opcode: int, node_id: int, session: int, value: int = 0) -> bytes:
    return COMMAND.pack(opcode, node_id, session, 0, value)


def parse_status(payload: bytes) -> Status:
    if len(payload) != 8:
        raise UpdateError(f"invalid status DLC {len(payload)}")
    return Status(*STATUS.unpack(payload))


def validate_image(image: bytes) -> None:
    if len(image) < 8:
        raise UpdateError("image is too small to contain a vector table")
    if len(image) > APP_MAX_SIZE:
        raise UpdateError(
            f"image is {len(image)} bytes; OTA application limit is {APP_MAX_SIZE}"
        )

    initial_sp, reset_handler = struct.unpack_from("<II", image)
    reset_address = reset_handler & ~1
    if initial_sp < SRAM_BASE or initial_sp > SRAM_END or initial_sp % 8:
        raise UpdateError(
            f"invalid initial stack pointer 0x{initial_sp:08X}; "
            "use an stm32-*-ota-release .bin"
        )
    if not (reset_handler & 1) or not (APP_ADDRESS <= reset_address < METADATA_ADDRESS):
        raise UpdateError(
            f"invalid reset vector 0x{reset_handler:08X}; "
            "the image is probably linked for 0x08000000 instead of 0x08006000"
        )


class Updater:
    def __init__(self, bus: SocketCan, node: str, timeout: float,
                 inter_frame_delay: float):
        self.bus = bus
        self.node = node
        self.node_id = NODE_ID[node]
        self.data_id = CAN_ID_DATA[node]
        self.status_id = CAN_ID_STATUS[node]
        self.timeout = timeout
        self.inter_frame_delay = inter_frame_delay
        self.session = secrets.randbelow(255) + 1

    def send_command(self, opcode: int, value: int = 0) -> None:
        self.bus.send(
            CAN_ID_COMMAND,
            build_command(opcode, self.node_id, self.session, value),
        )

    def wait_status(self, timeout: float | None = None,
                    require_session: bool = True) -> Status:
        deadline = time.monotonic() + (self.timeout if timeout is None else timeout)
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise UpdateError("timed out waiting for bootloader status")
            frame = self.bus.receive(remaining)
            if frame is None:
                raise UpdateError("timed out waiting for bootloader status")
            identifier, payload = frame
            if identifier != self.status_id:
                continue
            status = parse_status(payload)
            if status.protocol_version != PROTOCOL_VERSION:
                raise UpdateError(
                    f"bootloader protocol {status.protocol_version}, "
                    f"updater protocol {PROTOCOL_VERSION}"
                )
            if require_session and status.session != self.session:
                continue
            return status

    @staticmethod
    def check_error(status: Status, allow_sequence: bool = False) -> None:
        if status.code != STATUS_ERROR:
            return
        if allow_sequence and status.detail == 5:
            return
        name = ERROR_NAMES.get(status.detail, f"unknown error {status.detail}")
        raise UpdateError(f"bootloader rejected update: {name} (value={status.value})")

    def enter(self) -> None:
        # A running application resets immediately and cannot acknowledge ENTER.
        for _ in range(3):
            self.send_command(CMD_ENTER)
            time.sleep(0.05)

        deadline = time.monotonic() + 5.0
        while time.monotonic() < deadline:
            self.send_command(CMD_QUERY)
            try:
                status = self.wait_status(timeout=0.4, require_session=False)
            except UpdateError:
                continue
            self.check_error(status)
            return
        raise UpdateError(
            "node did not enter bootloader; the application FDCAN ENTER handler "
            "may not be enabled yet"
        )

    def begin(self, image_size: int) -> int:
        self.send_command(CMD_BEGIN, image_size)
        status = self.wait_status(timeout=10.0)
        self.check_error(status)
        if status.code not in (STATUS_READY, STATUS_ACK):
            raise UpdateError(f"unexpected BEGIN status 0x{status.code:02X}")
        return status.value if status.code == STATUS_ACK else 0

    def query_progress(self) -> int:
        self.send_command(CMD_QUERY)
        status = self.wait_status()
        self.check_error(status, allow_sequence=True)
        if status.code not in (STATUS_ACK, STATUS_READY):
            raise UpdateError(f"node is no longer receiving (status 0x{status.code:02X})")
        return status.value if status.code == STATUS_ACK else 0

    def send_image(self, image: bytes, start_sequence: int = 0) -> None:
        frame_count = (len(image) + DATA_BYTES - 1) // DATA_BYTES
        sequence = start_sequence
        while sequence < frame_count:
            window_end = min(sequence + ACK_INTERVAL, frame_count)
            for current in range(sequence, window_end):
                offset = current * DATA_BYTES
                chunk = image[offset:offset + DATA_BYTES].ljust(DATA_BYTES, b"\xFF")
                self.bus.send(self.data_id, DATA.pack(current, chunk))
                if self.inter_frame_delay:
                    time.sleep(self.inter_frame_delay)

            try:
                status = self.wait_status()
                self.check_error(status, allow_sequence=True)
                if status.code not in (STATUS_ACK, STATUS_ERROR):
                    raise UpdateError(f"unexpected data status 0x{status.code:02X}")
                sequence = status.value
            except UpdateError as error:
                if "timed out" not in str(error):
                    raise
                sequence = self.query_progress()

            percent = min(100.0, sequence * DATA_BYTES * 100.0 / len(image))
            print(f"\r{self.node}: {percent:5.1f}%", end="", flush=True)
        print()

    def finish(self, expected_crc: int) -> None:
        self.send_command(CMD_FINISH, expected_crc)
        status = self.wait_status(timeout=5.0)
        self.check_error(status)
        if status.code != STATUS_VERIFIED or status.value != expected_crc:
            raise UpdateError("node did not verify the programmed image")

    def activate(self) -> None:
        self.send_command(CMD_ACTIVATE)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Update one UGV STM32 node through Raspberry Pi SocketCAN"
    )
    parser.add_argument("--interface", default="can0", help="SocketCAN interface")
    parser.add_argument("--node", choices=("left", "right"), required=True)
    parser.add_argument("--image", type=Path, required=True,
                        help="bootloader-linked raw .bin application")
    parser.add_argument("--no-enter", action="store_true",
                        help="node is already waiting in the bootloader")
    parser.add_argument("--no-activate", action="store_true",
                        help="verify but leave the node in bootloader")
    parser.add_argument("--timeout", type=float, default=1.0)
    parser.add_argument("--inter-frame-delay", type=float, default=0.0003,
                        help="seconds between data frames (default: 0.0003)")
    parser.add_argument("--dry-run", action="store_true",
                        help="validate the binary without opening CAN")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    image = args.image.read_bytes()
    validate_image(image)
    crc = binascii.crc32(image) & 0xFFFFFFFF
    print(f"image={args.image} size={len(image)} crc32=0x{crc:08X}")
    if args.dry_run:
        return 0

    bus = SocketCan(args.interface)
    try:
        updater = Updater(bus, args.node, args.timeout, args.inter_frame_delay)
        if not args.no_enter:
            print(f"requesting {args.node} bootloader...")
            updater.enter()
        next_sequence = updater.begin(len(image))
        updater.send_image(image, next_sequence)
        updater.finish(crc)
        print(f"{args.node}: image verified")
        if not args.no_activate:
            updater.activate()
            print(f"{args.node}: activation requested")
    finally:
        bus.close()
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, UpdateError) as error:
        print(f"update failed: {error}", file=sys.stderr)
        raise SystemExit(1)
