#ifndef UGV_CRC32_H
#define UGV_CRC32_H

#include <stddef.h>
#include <stdint.h>

/* IEEE CRC-32 (polynomial 0xEDB88320). The streaming form starts with
 * ugv_crc32_init(), accepts any number of chunks, then finishes with
 * ugv_crc32_finalize(). It matches Python's binascii.crc32(). */
uint32_t ugv_crc32_init(void);
uint32_t ugv_crc32_update(uint32_t state, const uint8_t *data, size_t size);
uint32_t ugv_crc32_finalize(uint32_t state);
uint32_t ugv_crc32(const uint8_t *data, size_t size);

#endif /* UGV_CRC32_H */
