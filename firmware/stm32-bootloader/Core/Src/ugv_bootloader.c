#include "ugv_bootloader.h"

#include <string.h>

#include "ugv_crc32.h"

static void make_status(const ugv_bootloader_t *bootloader,
                        ugv_fw_status_t *status, uint8_t code,
                        uint8_t detail, uint32_t value)
{
    status->code = code;
    status->session = bootloader->session;
    status->detail = detail;
    status->protocol_version = UGV_FW_PROTOCOL_VERSION;
    status->value = value;
}

static void make_error(ugv_bootloader_t *bootloader,
                       ugv_fw_status_t *status, uint8_t error,
                       uint32_t value, bool fatal)
{
    bootloader->last_error = error;
    if (fatal) {
        bootloader->state = UGV_BOOTLOADER_FAILED;
    }
    make_status(bootloader, status, UGV_FW_STATUS_ERROR, error, value);
}

static void make_current_status(const ugv_bootloader_t *bootloader,
                                ugv_fw_status_t *status)
{
    switch (bootloader->state) {
        case UGV_BOOTLOADER_IDLE:
            make_status(bootloader, status, UGV_FW_STATUS_IDLE,
                        UGV_FW_ERROR_NONE,
                        bootloader->platform->max_image_size);
            break;
        case UGV_BOOTLOADER_RECEIVING:
            make_status(bootloader, status, UGV_FW_STATUS_ACK,
                        UGV_FW_ERROR_NONE, bootloader->next_sequence);
            break;
        case UGV_BOOTLOADER_VERIFIED:
            make_status(bootloader, status, UGV_FW_STATUS_VERIFIED,
                        UGV_FW_ERROR_NONE,
                        ugv_crc32_finalize(bootloader->crc_state));
            break;
        case UGV_BOOTLOADER_FAILED:
        default:
            make_status(bootloader, status, UGV_FW_STATUS_ERROR,
                        bootloader->last_error, bootloader->next_sequence);
            break;
    }
}

void ugv_bootloader_init(ugv_bootloader_t *bootloader, uint8_t node_id,
                         const ugv_bootloader_platform_t *platform)
{
    if (bootloader == NULL) {
        return;
    }
    memset(bootloader, 0, sizeof(*bootloader));
    bootloader->node_id = node_id;
    bootloader->state = UGV_BOOTLOADER_IDLE;
    bootloader->platform = platform;
}

static bool platform_valid(const ugv_bootloader_t *bootloader)
{
    return bootloader->platform != NULL &&
           bootloader->platform->prepare != NULL &&
           bootloader->platform->write != NULL &&
           bootloader->platform->finalize != NULL &&
           bootloader->platform->abort != NULL &&
           bootloader->platform->activate != NULL;
}

static bool exact_target(const ugv_bootloader_t *bootloader,
                         const ugv_fw_command_t *command)
{
    return command->target_node == bootloader->node_id;
}

bool ugv_bootloader_handle_command(ugv_bootloader_t *bootloader,
                                   const ugv_fw_command_t *command,
                                   ugv_fw_status_t *status)
{
    if (bootloader == NULL || command == NULL || status == NULL ||
        !platform_valid(bootloader)) {
        return false;
    }

    const bool broadcast_query =
        command->opcode == UGV_FW_COMMAND_QUERY &&
        command->target_node == UGV_FW_BROADCAST_NODE;
    if (!exact_target(bootloader, command) && !broadcast_query) {
        return false;
    }

    if (command->opcode == UGV_FW_COMMAND_QUERY) {
        make_current_status(bootloader, status);
        return true;
    }

    switch ((ugv_fw_command_opcode_t)command->opcode) {
        case UGV_FW_COMMAND_BEGIN:
            if (command->value == 0u ||
                command->value > bootloader->platform->max_image_size) {
                bootloader->session = command->session;
                make_error(bootloader, status, UGV_FW_ERROR_IMAGE_SIZE,
                           bootloader->platform->max_image_size, false);
                return true;
            }

            /* BEGIN is idempotent within a session so a lost READY/ACK frame
             * cannot cause a second erase after data has already arrived. */
            if (bootloader->state == UGV_BOOTLOADER_RECEIVING &&
                bootloader->session == command->session &&
                bootloader->image_size == command->value) {
                make_status(bootloader, status, UGV_FW_STATUS_ACK,
                            UGV_FW_ERROR_NONE, bootloader->next_sequence);
                return true;
            }

            bootloader->session = command->session;
            if (!bootloader->platform->prepare(bootloader->platform->context,
                                               command->value)) {
                make_error(bootloader, status, UGV_FW_ERROR_FLASH_ERASE,
                           0u, true);
                return true;
            }
            bootloader->state = UGV_BOOTLOADER_RECEIVING;
            bootloader->image_size = command->value;
            bootloader->received_size = 0u;
            bootloader->next_sequence = 0u;
            bootloader->crc_state = ugv_crc32_init();
            bootloader->last_error = UGV_FW_ERROR_NONE;
            make_status(bootloader, status, UGV_FW_STATUS_READY,
                        UGV_FW_ERROR_NONE,
                        bootloader->platform->max_image_size);
            return true;

        case UGV_FW_COMMAND_FINISH: {
            if (bootloader->session != command->session) {
                make_error(bootloader, status, UGV_FW_ERROR_BAD_SESSION,
                           bootloader->next_sequence, false);
                return true;
            }

            const uint32_t computed_crc =
                ugv_crc32_finalize(bootloader->crc_state);
            if (bootloader->state == UGV_BOOTLOADER_VERIFIED &&
                command->value == computed_crc) {
                make_status(bootloader, status, UGV_FW_STATUS_VERIFIED,
                            UGV_FW_ERROR_NONE, computed_crc);
                return true;
            }
            if (bootloader->state != UGV_BOOTLOADER_RECEIVING) {
                make_error(bootloader, status, UGV_FW_ERROR_BAD_STATE,
                           bootloader->next_sequence, false);
                return true;
            }
            if (bootloader->received_size != bootloader->image_size) {
                make_error(bootloader, status, UGV_FW_ERROR_INCOMPLETE_IMAGE,
                           bootloader->next_sequence, false);
                return true;
            }
            if (computed_crc != command->value) {
                make_error(bootloader, status, UGV_FW_ERROR_CRC_MISMATCH,
                           computed_crc, false);
                return true;
            }
            if (!bootloader->platform->finalize(
                    bootloader->platform->context, bootloader->image_size,
                    computed_crc)) {
                make_error(bootloader, status,
                           UGV_FW_ERROR_METADATA_WRITE, computed_crc, true);
                return true;
            }
            bootloader->state = UGV_BOOTLOADER_VERIFIED;
            make_status(bootloader, status, UGV_FW_STATUS_VERIFIED,
                        UGV_FW_ERROR_NONE, computed_crc);
            return true;
        }

        case UGV_FW_COMMAND_ACTIVATE:
            if (bootloader->session != command->session) {
                make_error(bootloader, status, UGV_FW_ERROR_BAD_SESSION,
                           bootloader->next_sequence, false);
                return true;
            }
            if (bootloader->state != UGV_BOOTLOADER_VERIFIED) {
                make_error(bootloader, status, UGV_FW_ERROR_BAD_STATE,
                           bootloader->next_sequence, false);
                return true;
            }
            bootloader->platform->activate(bootloader->platform->context);
            return false;

        case UGV_FW_COMMAND_ABORT:
            if (bootloader->state != UGV_BOOTLOADER_IDLE &&
                bootloader->session != command->session) {
                make_error(bootloader, status, UGV_FW_ERROR_BAD_SESSION,
                           bootloader->next_sequence, false);
                return true;
            }
            bootloader->platform->abort(bootloader->platform->context);
            bootloader->state = UGV_BOOTLOADER_IDLE;
            bootloader->session = command->session;
            bootloader->image_size = 0u;
            bootloader->received_size = 0u;
            bootloader->next_sequence = 0u;
            bootloader->last_error = UGV_FW_ERROR_NONE;
            make_current_status(bootloader, status);
            return true;

        case UGV_FW_COMMAND_ENTER:
        default:
            make_error(bootloader, status, UGV_FW_ERROR_BAD_COMMAND,
                       bootloader->next_sequence, false);
            return true;
    }
}

bool ugv_bootloader_handle_data(ugv_bootloader_t *bootloader,
                                const ugv_fw_data_t *data,
                                ugv_fw_status_t *status)
{
    if (bootloader == NULL || data == NULL || status == NULL ||
        !platform_valid(bootloader)) {
        return false;
    }
    if (bootloader->state != UGV_BOOTLOADER_RECEIVING) {
        make_error(bootloader, status, UGV_FW_ERROR_BAD_STATE,
                   bootloader->next_sequence, false);
        return true;
    }
    if (data->sequence < bootloader->next_sequence) {
        make_status(bootloader, status, UGV_FW_STATUS_ACK,
                    UGV_FW_ERROR_NONE, bootloader->next_sequence);
        return true;
    }
    if (data->sequence > bootloader->next_sequence) {
        make_error(bootloader, status, UGV_FW_ERROR_SEQUENCE,
                   bootloader->next_sequence, false);
        return true;
    }

    const uint32_t remaining =
        bootloader->image_size - bootloader->received_size;
    const size_t write_size = remaining < UGV_FW_DATA_BYTES_PER_FRAME
                                  ? (size_t)remaining
                                  : UGV_FW_DATA_BYTES_PER_FRAME;
    if (!bootloader->platform->write(bootloader->platform->context,
                                     bootloader->received_size,
                                     data->bytes, write_size)) {
        make_error(bootloader, status, UGV_FW_ERROR_FLASH_WRITE,
                   bootloader->next_sequence, true);
        return true;
    }

    bootloader->crc_state = ugv_crc32_update(
        bootloader->crc_state, data->bytes, write_size);
    bootloader->received_size += (uint32_t)write_size;
    ++bootloader->next_sequence;

    const bool image_complete =
        bootloader->received_size == bootloader->image_size;
    const bool ack_boundary =
        (bootloader->next_sequence % UGV_FW_ACK_INTERVAL_FRAMES) == 0u;
    if (image_complete || ack_boundary) {
        make_status(bootloader, status, UGV_FW_STATUS_ACK,
                    UGV_FW_ERROR_NONE, bootloader->next_sequence);
        return true;
    }
    return false;
}
