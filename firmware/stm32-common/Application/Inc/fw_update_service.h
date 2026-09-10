#ifndef APPLICATION_FW_UPDATE_SERVICE_H
#define APPLICATION_FW_UPDATE_SERVICE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Called by the operational FDCAN dispatcher after it validates the standard
 * data frame and DLC. Only a node-targeted ENTER command is acted on. */
void fw_update_service_handle_frame(uint16_t identifier,
                                    const uint8_t *payload,
                                    size_t size);

#endif /* APPLICATION_FW_UPDATE_SERVICE_H */
