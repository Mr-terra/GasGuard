/**
 * @file hal_eeprom_fake.c
 * @brief EEPROM kept in RAM, with the same size and address rules as the chip.
 *
 * How it works
 * ------------
 * The fake is a static array of HAL_EEPROM_SIZE bytes. A blank EEPROM reads
 * 0xFF, so the fake erases to 0xFF too. Each read and write checks the
 * pointer and the address range first, so a block that runs past the end of
 * the chip is refused instead of corrupting memory. Keeping these rules in the
 * fake lets the configuration and event-log code be tested on the PC against
 * the same limits as on the board.
 *
 * @req REQ-CFG-001 REQ-LOG-001
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "common/gg_types.h"
#include "hal/hal_eeprom.h"
#include "hal/host/hal_eeprom_fake.h"

/** @brief Value of an erased EEPROM cell. */
#define EEPROM_FAKE_ERASED 0xFFu

static uint8_t s_eeprom_fake[HAL_EEPROM_SIZE];

/**
 * @brief Check that a block lies inside the EEPROM.
 * @param addr  Start address.
 * @param len   Block length.
 * @return true if addr + len does not exceed HAL_EEPROM_SIZE.
 */
static bool block_in_range(uint16_t addr, uint16_t len)
{
    return ((uint32_t)addr + (uint32_t)len) <= HAL_EEPROM_SIZE;
}

void hal_eeprom_fake_reset(void)
{
    uint16_t addr;

    for (addr = 0u; addr < HAL_EEPROM_SIZE; addr++) {
        s_eeprom_fake[addr] = EEPROM_FAKE_ERASED;
    }
}

gg_status_t hal_eeprom_read(uint16_t addr, uint8_t *data, uint16_t len)
{
    gg_status_t status = GG_OK;
    uint16_t i;

    if (data == NULL) {
        status = GG_ERR_NULL;
    } else if (!block_in_range(addr, len)) {
        status = GG_ERR_RANGE;
    } else {
        for (i = 0u; i < len; i++) {
            data[i] = s_eeprom_fake[addr + i];
        }
    }
    return status;
}

gg_status_t hal_eeprom_write(uint16_t addr, const uint8_t *data, uint16_t len)
{
    gg_status_t status = GG_OK;
    uint16_t i;

    if (data == NULL) {
        status = GG_ERR_NULL;
    } else if (!block_in_range(addr, len)) {
        status = GG_ERR_RANGE;
    } else {
        for (i = 0u; i < len; i++) {
            s_eeprom_fake[addr + i] = data[i];
        }
    }
    return status;
}
