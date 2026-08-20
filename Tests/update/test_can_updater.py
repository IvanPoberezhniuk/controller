import importlib.util
import struct
import sys
import unittest
from pathlib import Path


MODULE_PATH = Path(__file__).parents[2] / "tools" / "ugv_can_update.py"
SPEC = importlib.util.spec_from_file_location("ugv_can_update", MODULE_PATH)
updater = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
sys.modules[SPEC.name] = updater
SPEC.loader.exec_module(updater)


class UpdaterFormatTests(unittest.TestCase):
    def test_command_layout(self):
        payload = updater.build_command(
            updater.CMD_BEGIN, updater.NODE_ID["left"], 7, 0x12345678
        )
        self.assertEqual(payload, bytes.fromhex("10 10 07 00 78 56 34 12"))

    def test_status_layout(self):
        status = updater.parse_status(bytes.fromhex("02 07 00 01 20 00 00 00"))
        self.assertEqual(status.code, updater.STATUS_ACK)
        self.assertEqual(status.session, 7)
        self.assertEqual(status.value, 32)

    def test_ota_image_validation(self):
        valid = bytearray(64)
        struct.pack_into("<II", valid, 0, updater.SRAM_END,
                         updater.APP_ADDRESS + 0x101)
        updater.validate_image(valid)

        standalone = bytearray(valid)
        struct.pack_into("<I", standalone, 4, 0x08000101)
        with self.assertRaises(updater.UpdateError):
            updater.validate_image(standalone)

    def test_can_frame_layout_is_linux_can_frame_size(self):
        packed = updater.CAN_FRAME.pack(0x600, 8, bytes(8))
        self.assertEqual(len(packed), 16)


if __name__ == "__main__":
    unittest.main()
