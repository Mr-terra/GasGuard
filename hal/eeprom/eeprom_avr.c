/**
 * @file eeprom_avr.c
 * @brief Byte-addressed EEPROM access for the ATmega328P (1 KB).
 *
 * How it works
 * ------------
 * The avr-libc EEPROM helpers perform the register sequence (wait for the previous
 * write, set EEAR and EEDR, then EEMPE and EEPE). Writes use eeprom_update_block(),
 * which reads each byte first and writes only the bytes that differ. An unchanged
 * byte is never rewritten, so saving the same configuration again does not wear
 * the cells (REQ-LOG-003). The address range is checked before any access: a
 * write past the end would corrupt the next block without any error.
 *
 * @req REQ-CFG-001 REQ-LOG-001
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <avr/eeprom.h>

#include "common/gg_types/gg_types.h"
#include "hal/eeprom/eeprom.h"

/** @brief True if the block [addr, addr + len) lies inside the EEPROM. */
static bool eeprom_block_ok(uint16_t addr, uint16_t len)
{
    return ((uint32_t)addr + (uint32_t)len) <= (uint32_t)HAL_EEPROM_SIZE;
}

/**
 * @brief Read a block of bytes.
 *
 * The full contract is in the header.
 * @req REQ-CFG-001
 */
gg_status_t hal_eeprom_read(uint16_t addr, uint8_t *data, uint16_t len)
{
    gg_status_t status = GG_OK;

    if ((data == NULL) && (len > 0u)) {
        status = GG_ERR_NULL;
    } else if (!eeprom_block_ok(addr, len)) {
        status = GG_ERR_RANGE;
    } else if (len > 0u) {
        /* DEV-06: avr-libc takes the EEPROM address as a pointer; the cast is the API. */
        // cppcheck-suppress misra-c2012-11.6
        eeprom_read_block(data, (const void *)addr, len);
    } else {
        /* Empty block: nothing to read. */
    }
    return status;
}

/**
 * @brief Write a block of bytes. Blocks until the write has completed.
 *
 * The full contract is in the header.
 * @req REQ-LOG-001
 */
gg_status_t hal_eeprom_write(uint16_t addr, const uint8_t *data, uint16_t len)
{
    gg_status_t status = GG_OK;

    if ((data == NULL) && (len > 0u)) {
        status = GG_ERR_NULL;
    } else if (!eeprom_block_ok(addr, len)) {
        status = GG_ERR_RANGE;
    } else if (len > 0u) {
        /* DEV-06: avr-libc takes the EEPROM address as a pointer; the cast is the API. */
        // cppcheck-suppress misra-c2012-11.6
        eeprom_update_block(data, (void *)addr, len);
    } else {
        /* Empty block: nothing to write. */
    }
    return status;
}
