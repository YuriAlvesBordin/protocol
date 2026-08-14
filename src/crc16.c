#include "crc16.h"

uint16_t crc16_ccitt_false(const uint8_t *data, uint16_t length)
{
    uint16_t crc = 0xFFFF;
    uint16_t i;

    for (i = 0; i < length; i++)
    {
        crc ^= (uint16_t)data[i] << 0;
        uint8_t j;
        for (j = 0; j < 8; j++)
        {
            if (crc & 0x0001)
            {
                crc >>= 1;
                crc ^= 0x1021;
            }
            else
            {
                crc >>= 1;
            }
        }
    }

    return crc;
}