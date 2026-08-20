#include "ugv_crc32.h"

uint32_t ugv_crc32_init(void)
{
    return 0xffffffffu;
}

uint32_t ugv_crc32_update(uint32_t state, const uint8_t *data, size_t size)
{
    if (data == NULL && size != 0u) {
        return state;
    }

    for (size_t index = 0; index < size; ++index) {
        state ^= data[index];
        for (uint8_t bit = 0; bit < 8u; ++bit) {
            const uint32_t mask = (uint32_t)-(int32_t)(state & 1u);
            state = (state >> 1u) ^ (0xedb88320u & mask);
        }
    }
    return state;
}

uint32_t ugv_crc32_finalize(uint32_t state)
{
    return state ^ 0xffffffffu;
}

uint32_t ugv_crc32(const uint8_t *data, size_t size)
{
    return ugv_crc32_finalize(ugv_crc32_update(ugv_crc32_init(), data, size));
}
