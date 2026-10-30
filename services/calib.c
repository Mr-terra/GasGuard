/**
 * @file calib.c
 * @brief Linear calibration in Q8 fixed point, with a linear temperature term.
 *
 * How it works
 * ------------
 * Concentration = (raw - zero) x gain. The gain is stored as Q8, so 256 means
 * one unit per LSB; the product is computed in 32 bits and divided by 256, which
 * keeps the fractional part without any floating point. Readings at or below the
 * zero offset are reported as 0. The temperature term shifts the raw value by
 * (temp - reference) x coefficient, and is applied only when the temperature
 * sensor is valid: a faulty thermometer must never move the gas reading.
 *
 * Division is used instead of a right shift for the signed temperature term,
 * because the C99 result of shifting a negative value is implementation-defined.
 *
 * @req REQ-FUN-003 REQ-FUN-004
 */
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "common/gg_types.h"
#include "hal/hal_adc.h"
#include "services/calib.h"

gg_status_t calib_init(calib_t *calib, const calib_config_t *config)
{
    gg_status_t status = GG_OK;

    if ((calib == NULL) || (config == NULL)) {
        status = GG_ERR_NULL;
    } else if (config->gain_q8 == 0u) {
        status = GG_ERR_RANGE;
    } else {
        calib->config = *config;
    }
    return status;
}

gg_status_t calib_to_lel(const calib_t *calib, uint16_t raw, gg_conc_t *out)
{
    gg_status_t status = GG_OK;
    uint32_t units;

    if ((calib == NULL) || (out == NULL)) {
        status = GG_ERR_NULL;
    } else if (raw > HAL_ADC_RAW_MAX) {
        status = GG_ERR_RANGE;
    } else if (raw <= calib->config.zero_raw) {
        *out = 0;
    } else {
        units = ((uint32_t)(raw - calib->config.zero_raw) * calib->config.gain_q8)
                / (uint32_t)CALIB_GAIN_ONE;
        if (units > (uint32_t)INT16_MAX) {
            units = (uint32_t)INT16_MAX;
        }
        *out = (gg_conc_t)units;
    }
    return status;
}

gg_status_t calib_temp_correct(const calib_t *calib, const calib_reading_t *reading, uint16_t *out_raw)
{
    gg_status_t status = GG_OK;
    int32_t delta;
    int32_t corrected;

    if ((calib == NULL) || (reading == NULL) || (out_raw == NULL)) {
        status = GG_ERR_NULL;
    } else if (reading->raw > HAL_ADC_RAW_MAX) {
        status = GG_ERR_RANGE;
    } else if (!reading->temp_valid) {
        *out_raw = reading->raw;
    } else {
        delta = (int32_t)reading->temp_raw - (int32_t)calib->config.temp_ref_raw;
        corrected = (int32_t)reading->raw
                    + ((delta * (int32_t)calib->config.temp_coef_q8) / (int32_t)CALIB_GAIN_ONE);
        if (corrected < 0) {
            corrected = 0;
        }
        if (corrected > (int32_t)HAL_ADC_RAW_MAX) {
            corrected = (int32_t)HAL_ADC_RAW_MAX;
        }
        *out_raw = (uint16_t)corrected;
    }
    return status;
}
