/**
 * @file crc16.h
 * @brief CRC-16/CCITT-FALSE, used to protect each configuration copy in EEPROM.
 *
 * Parameters: polynomial 0x1021, initial value 0xFFFF, no bit reflection, no
 * final XOR. The check value of the ASCII string "123456789" is 0x29B1.
 *
 * @req REQ-CFG-001
 */
#ifndef CRC16_H
// DEV-02: include guard, not an unused macro
// cppcheck-suppress misra-c2012-2.5
#define CRC16_H

#include <stdint.h>

#include "common/gg_types/gg_types.h"

/**
 * @brief Compute the CRC-16/CCITT-FALSE of a byte buffer.
 * @param data   Bytes to check. May be NULL only when len is 0.
 * @param len    Number of bytes.
 * @param out_crc  Receives the CRC. Must not be NULL.
 * @return GG_OK, GG_ERR_NULL if out_crc is NULL, or if data is NULL with len > 0.
 * @req REQ-CFG-001
 */
gg_status_t crc16_compute(const uint8_t *data, uint16_t len, uint16_t *out_crc);

#endif /* CRC16_H */
