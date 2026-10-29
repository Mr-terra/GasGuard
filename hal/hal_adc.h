/**
 * @file hal_adc.h
 * @brief Hardware interface: analog-to-digital converter.
 *
 * Implemented by hal/avr (ATmega328P ADC) on the board and by hal/host fakes
 * on the PC. Application code depends only on this header (ADR-3).
 *
 * @req REQ-FUN-001
 */
#ifndef HAL_ADC_H
// DEV-02: include guard, not an unused macro
// cppcheck-suppress misra-c2012-2.5
#define HAL_ADC_H

#include <stdint.h>

#include "common/gg_types.h"

/** @brief Number of analog input channels exposed by the interface. */
#define HAL_ADC_CHANNEL_COUNT 8u

/** @brief Largest raw value of the 10-bit converter. */
#define HAL_ADC_RAW_MAX 1023u

/**
 * @brief Read one analog channel as a raw 10-bit value.
 *
 * @param channel  Channel index, 0 .. HAL_ADC_CHANNEL_COUNT - 1.
 * @param out_raw  Receives the raw value, 0 .. HAL_ADC_RAW_MAX.
 * @return GG_OK on success, GG_ERR_NULL if out_raw is NULL,
 *         GG_ERR_RANGE if channel is out of range.
 * @req REQ-FUN-001
 */
// DEV-04: one implementation per target, fake on PC
// cppcheck-suppress misra-c2012-8.7
gg_status_t hal_adc_read(uint8_t channel, uint16_t *out_raw);

#endif /* HAL_ADC_H */
