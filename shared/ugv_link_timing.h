#ifndef UGV_LINK_TIMING_H
#define UGV_LINK_TIMING_H

/* Shared across UART and CAN transports so the two link-supervision
 * timeouts can't silently drift apart between protocol headers. */
#define UGV_LINK_RC_TIMEOUT_MS      100u
#define UGV_LINK_COMMAND_TIMEOUT_MS 300u

#endif /* UGV_LINK_TIMING_H */
