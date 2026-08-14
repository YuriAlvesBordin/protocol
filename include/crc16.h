#ifndef CRC16_H
#define CRC16_H

#include <stdint.h>

/**
 * @brief Calculate CRC-16-CCITT-FALSE
 * 
 * @param data Pointer to data buffer
 * @param length Length of data in bytes
 * @return uint16_t CRC value
 */
uint16_t crc16_ccitt_false(const uint8_t *data, uint16_t length);

#endif // CRC16_H