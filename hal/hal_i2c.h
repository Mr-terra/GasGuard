/**
 * @file hal_i2c.h
 * @brief Hardware interface: I2C master (TWI) for the Grove LCD RGB backlight.
 *
 * The LCD text controller is at 0x3E and the RGB backlight at 0x62 on v4.0
 * boards (0x30 on v5.0): verify with an I2C scan (docs/02-architecture.md §2).
 *
 * @req REQ-HMI-001 REQ-HMI-002
 */
#ifndef HAL_I2C_H
#define HAL_I2C_H

#include <stdint.h>

#include "common/gg_types.h"

/**
 * @brief Enable the TWI peripheral at 100 kHz.
 * @return GG_OK on success.
 * @req REQ-HMI-001
 */
gg_status_t hal_i2c_init(void);

/**
 * @brief Write bytes to a 7-bit device address, with a STOP at the end.
 *
 * @param addr7  7-bit device address (no read/write bit).
 * @param data   Bytes to send.
 * @param len    Number of bytes.
 * @return GG_OK, GG_ERR_NULL, GG_ERR_TIMEOUT if the bus does not respond,
 *         or GG_ERR_HW on a bus error.
 * @req REQ-HMI-002
 */
gg_status_t hal_i2c_write(uint8_t addr7, const uint8_t *data, uint8_t len);

#endif /* HAL_I2C_H */
