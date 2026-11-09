/**
 * @file eeprom_fake.h
 * @brief Host-only control API of the EEPROM fake (PC tests only, never on AVR).
 */
#ifndef HAL_EEPROM_FAKE_H
// DEV-02: include guard, not an unused macro
// cppcheck-suppress misra-c2012-2.5
#define HAL_EEPROM_FAKE_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Erase the fake EEPROM: every cell reads 0xFF, as on a blank chip.
 * @req REQ-CFG-001
 */
// DEV-04: HAL interface or test control API, one definition on the PC
// cppcheck-suppress misra-c2012-8.7
void hal_eeprom_fake_reset(void);

/**
 * @brief Number of successful writes since the last reset (REQ-LOG-003: writes happen on events only).
 * @return The count.
 */
// DEV-04: HAL interface or test control API, one definition on the PC
// cppcheck-suppress misra-c2012-8.7
uint32_t hal_eeprom_fake_write_count(void);

/**
 * @brief Make every write fail with GG_ERR_HW until switched off (test of the error paths).
 * @param fail  true to fail the writes.
 */
// DEV-04: HAL interface or test control API, one definition on the PC
// cppcheck-suppress misra-c2012-8.7
void hal_eeprom_fake_fail_writes(bool fail);

#endif /* HAL_EEPROM_FAKE_H */
