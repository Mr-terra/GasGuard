/**
 * @file hal_adc_fake.c
 * @brief Scriptable ADC for host tests: each channel returns a value set by the test.
 *
 * How it works
 * ------------
 * The fake keeps one raw value per channel in a static struct. A test sets the
 * value with hal_adc_fake_set_value(); the code under test calls hal_adc_read()
 * exactly as it would on the board, and gets the scripted value back. Both
 * functions check their arguments, so the fake enforces the same contract as
 * the real driver: NULL pointers and out-of-range channels return an error code.
 * Because the code under test only sees hal_adc.h, the same logic runs
 * unchanged on the PC and on the ATmega328P.
 *
 * @req REQ-FUN-001
 */
#include <stddef.h>
#include <stdint.h>

#include "common/gg_types.h"
#include "hal/hal_adc.h"
#include "hal/host/hal_adc_fake.h"

/** @brief Raw value of every channel, as scripted by the test. */
typedef struct {
    uint16_t raw[HAL_ADC_CHANNEL_COUNT];
} adc_fake_state_t;

static adc_fake_state_t s_adc_fake;

void hal_adc_fake_reset(void)
{
    uint8_t channel;

    for (channel = 0u; channel < HAL_ADC_CHANNEL_COUNT; channel++) {
        s_adc_fake.raw[channel] = 0u;
    }
}

gg_status_t hal_adc_fake_set_value(uint8_t channel, uint16_t raw)
{
    if (channel >= HAL_ADC_CHANNEL_COUNT) {
        return GG_ERR_RANGE;
    }
    if (raw > HAL_ADC_RAW_MAX) {
        return GG_ERR_RANGE;
    }
    s_adc_fake.raw[channel] = raw;
    return GG_OK;
}

gg_status_t hal_adc_read(uint8_t channel, uint16_t *out_raw)
{
    if (out_raw == NULL) {
        return GG_ERR_NULL;
    }
    if (channel >= HAL_ADC_CHANNEL_COUNT) {
        return GG_ERR_RANGE;
    }
    *out_raw = s_adc_fake.raw[channel];
    return GG_OK;
}
