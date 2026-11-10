/**
 * @file crc16.c
 * @brief CRC-16/CCITT-FALSE, bit by bit.
 *
 * How it works
 * ------------
 * The CRC is a register of 16 bits, started at 0xFFFF. For each byte, the byte
 * is XORed into the top 8 bits of the register. Then the register is shifted
 * left 8 times; each time the bit that falls out of the top is 1, the
 * polynomial 0x1021 is XORed in. The result is the checksum. A single changed
 * bit always changes the result, which is why each configuration copy stores
 * one (REQ-CFG-001). Lookup tables would be faster but cost 512 bytes of flash;
 * the bit loop is small and fast enough for a 100 ms cycle.
 *
 * @req REQ-CFG-001
 */
#include <stddef.h>
#include <stdint.h>

#include "common/gg_types/gg_types.h"
#include "services/crc16/crc16.h"

/** @brief Generator polynomial of CRC-16/CCITT. */
#define CRC16_POLY 0x1021u
/** @brief Initial register value of CRC-16/CCITT-FALSE. */
#define CRC16_INIT 0xFFFFu
/** @brief Bit that is shifted out of the register on each step. */
#define CRC16_TOP_BIT 0x8000u
/** @brief Number of bits in a byte, and the shift that places a byte in the top half. */
#define CRC16_BITS_PER_BYTE 8u
#define CRC16_BYTE_SHIFT    8u

/**
 * @brief Compute the CRC-16/CCITT-FALSE of a byte buffer.
 * @req REQ-CFG-001
 */
gg_status_t crc16_compute(const uint8_t *data, uint16_t len, uint16_t *out_crc)
{
    gg_status_t status = GG_OK;

    if ((out_crc == NULL) || ((data == NULL) && (len > 0u))) {
        status = GG_ERR_NULL;
    } else {
        uint16_t crc = CRC16_INIT;
        uint16_t i;
        uint8_t bit;

        for (i = 0u; i < len; i++) {
            crc ^= (uint16_t)((uint16_t)data[i] << CRC16_BYTE_SHIFT);
            for (bit = 0u; bit < CRC16_BITS_PER_BYTE; bit++) {
                if ((crc & CRC16_TOP_BIT) != 0u) {
                    crc = (uint16_t)((uint16_t)(crc << 1u) ^ CRC16_POLY);
                } else {
                    crc = (uint16_t)(crc << 1u);
                }
            }
        }
        *out_crc = crc;
    }
    return status;
}
