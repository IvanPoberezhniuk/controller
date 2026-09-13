#ifndef APPLICATION_CAN_CONTROL_SERVICE_H
#define APPLICATION_CAN_CONTROL_SERVICE_H

#include <stdbool.h>

/* Owns FDCAN1 at runtime. It accepts only the final ESP32 motion/enable
 * frames and the maintenance bootloader-entry command. */
bool can_control_service_init(void);
void can_control_service_poll(void);
void can_control_service_publish_telemetry(void);

#endif /* APPLICATION_CAN_CONTROL_SERVICE_H */
