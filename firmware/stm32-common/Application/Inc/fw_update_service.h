#ifndef APPLICATION_FW_UPDATE_SERVICE_H
#define APPLICATION_FW_UPDATE_SERVICE_H

#include <stdbool.h>

/* Until the full operational FDCAN transport is added, this service owns one
 * exact-match filter for UGV_FW_CAN_ID_COMMAND and consumes only firmware
 * update commands. It is a no-op while CubeMX FDCAN remains disabled. */
bool fw_update_service_init(void);
void fw_update_service_poll(void);

#endif /* APPLICATION_FW_UPDATE_SERVICE_H */
