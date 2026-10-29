/**
 * @file hal_eeprom.h
 * @brief Hardware interface: non-volatile EEPROM (1 KB on the ATmega328P).
 *
 * Used by services/config (two CRC-protected copies) and services/eventlog
 * (ring buffer). Callers own the layout; see docs/02-architecture.md §5.
 *
 * @req REQ-CFG-001 REQ-LOG-001
 */
#ifndef HAL_EEPROM_H
#define HAL_EEPROM_H

#include <stdint.h>

#include "common/gg_types.h"

/** @brief Size of the EEPROM in bytes on the ATmega328P. */
#define HAL_EEPROM_SIZE 1024u

/**
 * @brief Read a block of bytes.
 *
 * @param addr  Start address, 0 .. HAL_EEPROM_SIZE - 1.
 * @param data  Destination buffer, at least len bytes.
 * @param len   Number of bytes; addr + len must not exceed HAL_EEPROM_SIZE.
 * @return GG_OK, GG_ERR_NULL, or GG_ERR_RANGE for a bad address or length.
 * @req REQ-CFG-001
 */
gg_status_t hal_eeprom_read(uint16_t addr, uint8_t *data, uint16_t len);

/**
 * @brief Write a block of bytes. Blocks until the write has completed.
 *
 * Writes cost EEPROM wear: callers must write only on events (REQ-LOG-003).
 *
 * @param addr  Start address, 0 .. HAL_EEPROM_SIZE - 1.
 * @param data  Source buffer, at least len bytes.
 * @param len   Number of bytes; addr + len must not exceed HAL_EEPROM_SIZE.
 * @return GG_OK, GG_ERR_NULL, GG_ERR_RANGE, or GG_ERR_HW if the write failed.
 * @req REQ-LOG-001
 */
gg_status_t hal_eeprom_write(uint16_t addr, const uint8_t *data, uint16_t len);

#endif /* HAL_EEPROM_H */
