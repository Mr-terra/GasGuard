/**
 * @file hal_adc_fake.h
 * @brief Host-only control API of the ADC fake (PC tests only, never on AVR).
 */
#ifndef HAL_ADC_FAKE_H
// DEV-02: include guard, not an unused macro
// cppcheck-suppress misra-c2012-2.5
#define HAL_ADC_FAKE_H

#include <stdint.h>

#include "common/gg_types.h"

/**
 * @brief Set every channel back to 0 (open circuit).
 * @req REQ-FUN-001
 */
// DEV-04: test-only control API, used from tests
// cppcheck-suppress misra-c2012-8.7
void hal_adc_fake_reset(void);

/**
 * @brief Script the raw value that the next hal_adc_read() returns for a channel.
 *
 * @param channel  Channel index, 0 .. HAL_ADC_CHANNEL_COUNT - 1.
 * @param raw      Raw value, 0 .. HAL_ADC_RAW_MAX.
 * @return GG_OK, or GG_ERR_RANGE if channel or raw is out of range.
 * @req REQ-FUN-001
 */
// DEV-04: test-only control API, used from tests
// cppcheck-suppress misra-c2012-8.7
gg_status_t hal_adc_fake_set_value(uint8_t channel, uint16_t raw);

#endif /* HAL_ADC_FAKE_H */
