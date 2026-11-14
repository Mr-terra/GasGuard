/**
 * @file adc_fake.c
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
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "common/gg_types/gg_types.h"
#include "hal/adc/adc.h"
#include "hal/adc/adc_fake.h"

/** @brief Nominal bandgap raw value, as DIAG_BANDGAP_NOMINAL: a reset starts in tolerance. */
#define ADC_FAKE_BANDGAP_NOMINAL 225u

/** @brief Raw value of every channel, as scripted by the test. */
typedef struct {
    uint16_t raw[HAL_ADC_CHANNEL_COUNT];
    uint16_t bandgap_raw;
    bool bandgap_fails;   /**< hal_adc_read_bandgap() returns GG_ERR_TIMEOUT, as a stuck ADC would. */
} adc_fake_state_t;

static adc_fake_state_t s_adc_fake;

void hal_adc_fake_reset(void)
{
    uint8_t channel;

    for (channel = 0u; channel < HAL_ADC_CHANNEL_COUNT; channel++) {
        s_adc_fake.raw[channel] = 0u;
    }
    s_adc_fake.bandgap_raw = ADC_FAKE_BANDGAP_NOMINAL;
    s_adc_fake.bandgap_fails = false;
}

void hal_adc_fake_fail_bandgap(bool fail)
{
    s_adc_fake.bandgap_fails = fail;
}

gg_status_t hal_adc_fake_set_bandgap(uint16_t raw)
{
    gg_status_t status = GG_OK;

    if (raw > HAL_ADC_RAW_MAX) {
        status = GG_ERR_RANGE;
    } else {
        s_adc_fake.bandgap_raw = raw;
    }
    return status;
}

gg_status_t hal_adc_read_bandgap(uint16_t *out_raw)
{
    gg_status_t status = GG_OK;

    if (out_raw == NULL) {
        status = GG_ERR_NULL;
    } else if (s_adc_fake.bandgap_fails) {
        status = GG_ERR_TIMEOUT;
    } else {
        *out_raw = s_adc_fake.bandgap_raw;
    }
    return status;
}

gg_status_t hal_adc_fake_set_value(uint8_t channel, uint16_t raw)
{
    gg_status_t status = GG_OK;

    if ((channel >= HAL_ADC_CHANNEL_COUNT) || (raw > HAL_ADC_RAW_MAX)) {
        status = GG_ERR_RANGE;
    } else {
        s_adc_fake.raw[channel] = raw;
    }
    return status;
}

gg_status_t hal_adc_read(uint8_t channel, uint16_t *out_raw)
{
    gg_status_t status = GG_OK;

    if (out_raw == NULL) {
        status = GG_ERR_NULL;
    } else if (channel >= HAL_ADC_CHANNEL_COUNT) {
        status = GG_ERR_RANGE;
    } else {
        *out_raw = s_adc_fake.raw[channel];
    }
    return status;
}
