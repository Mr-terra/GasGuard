/**
 * @file hal_i2c_fake.h
 * @brief Host-only control API of the I2C fake (PC tests only, never on AVR).
 */
#ifndef HAL_I2C_FAKE_H
// DEV-02: include guard, not an unused macro
// cppcheck-suppress misra-c2012-2.5
#define HAL_I2C_FAKE_H

#include <stdbool.h>
#include <stdint.h>

#include "common/gg_types.h"

/**
 * @brief Forget every recorded transfer and mark every device absent.
 * @req REQ-HMI-001
 */
// DEV-04: HAL interface or test control API, one definition on the PC
// cppcheck-suppress misra-c2012-8.7
void hal_i2c_fake_reset(void);

/**
 * @brief Declare whether a device acknowledges on the bus.
 * @param addr7    7-bit device address.
 * @param present  true if the device answers.
 * @return GG_OK, or GG_ERR_RANGE if addr7 is above 0x7F.
 * @req REQ-HMI-001
 */
// DEV-04: HAL interface or test control API, one definition on the PC
// cppcheck-suppress misra-c2012-8.7
gg_status_t hal_i2c_fake_set_present(uint8_t addr7, bool present);

/**
 * @brief Address of the last successful write.
 * @req REQ-HMI-002
 */
// DEV-04: HAL interface or test control API, one definition on the PC
// cppcheck-suppress misra-c2012-8.7
uint8_t hal_i2c_fake_last_addr(void);

/**
 * @brief Length of the last successful write.
 * @req REQ-HMI-002
 */
// DEV-04: HAL interface or test control API, one definition on the PC
// cppcheck-suppress misra-c2012-8.7
uint8_t hal_i2c_fake_last_len(void);

/**
 * @brief First byte of the last successful write (the LCD control byte).
 * @req REQ-HMI-002
 */
// DEV-04: HAL interface or test control API, one definition on the PC
// cppcheck-suppress misra-c2012-8.7
uint8_t hal_i2c_fake_last_first_byte(void);

#endif /* HAL_I2C_FAKE_H */
