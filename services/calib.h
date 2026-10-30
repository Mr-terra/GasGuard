/**
 * @file calib.h
 * @brief Conversion of a filtered raw ADC value to %LEL, with temperature compensation.
 *
 * The gain is a Q8 fixed-point number: CALIB_GAIN_ONE (256) means exactly
 * 1 unit (0.1 %LEL) per raw LSB, 2560 means 10 units per LSB. No floating point
 * is used (ADR-6).
 *
 * Parameters are passed in named structures rather than as a list of integers:
 * two adjacent parameters of the same type could be swapped without any
 * compiler warning (clang-tidy bugprone-easily-swappable-parameters).
 *
 * @req REQ-FUN-003 REQ-FUN-004
 */
#ifndef CALIB_H
// DEV-02: include guard, not an unused macro
// cppcheck-suppress misra-c2012-2.5
#define CALIB_H

#include <stdbool.h>
#include <stdint.h>

#include "common/gg_types.h"

/** @brief Fixed-point representation of 1.0 (Q8). */
#define CALIB_GAIN_ONE 256

/** @brief Calibration constants of one channel. Stored in configuration (REQ-CFG-001). */
typedef struct {
    uint16_t zero_raw;      /**< Raw value that means clean air (zero offset). */
    uint16_t gain_q8;       /**< Units (0.1 %LEL) per raw LSB, Q8. Must be non-zero. */
    uint16_t temp_ref_raw;  /**< Temperature reading at which no correction applies. */
    int16_t temp_coef_q8;   /**< Raw LSB corrected per temperature LSB, Q8. */
} calib_config_t;

/** @brief Live calibration of one channel: a validated copy of its configuration. */
typedef struct {
    calib_config_t config;  /**< Configuration in use. */
} calib_t;

/** @brief One sample to correct for temperature. */
typedef struct {
    uint16_t raw;          /**< Filtered raw value, 0 .. HAL_ADC_RAW_MAX. */
    uint16_t temp_raw;     /**< Current temperature reading (ADC counts). */
    bool temp_valid;       /**< True if the temperature sensor passed its diagnostics. */
} calib_reading_t;

/**
 * @brief Load a configuration into a channel after validating it.
 *
 * @param calib   Channel to initialise. Must not be NULL.
 * @param config  Configuration to load. Must not be NULL.
 * @return GG_OK, GG_ERR_NULL, or GG_ERR_RANGE if gain_q8 is zero.
 * @req REQ-FUN-003
 */
// DEV-05: service API, called by app/ and tests; cppcheck sees one caller at this stage
// cppcheck-suppress misra-c2012-8.7
gg_status_t calib_init(calib_t *calib, const calib_config_t *config);

/**
 * @brief Convert a filtered raw value to concentration in 0.1 %LEL.
 *
 * Readings at or below the zero offset are reported as 0: a negative
 * concentration has no physical meaning and only comes from drift.
 *
 * @param calib  Calibration of the channel. Must not be NULL.
 * @param raw    Filtered raw value, 0 .. HAL_ADC_RAW_MAX.
 * @param out    Receives the concentration. Must not be NULL.
 * @return GG_OK, GG_ERR_NULL, or GG_ERR_RANGE if raw is out of range.
 * @req REQ-FUN-003
 */
// DEV-05: service API, called by app/ and tests; cppcheck sees one caller at this stage
// cppcheck-suppress misra-c2012-8.7
gg_status_t calib_to_lel(const calib_t *calib, uint16_t raw, gg_conc_t *out);

/**
 * @brief Correct a raw value for temperature, when the temperature sensor is valid.
 *
 * corrected = raw + (temp_raw - temp_ref) * temp_coef, clamped to the ADC range.
 * When the temperature sensor is not valid, the raw value passes through unchanged:
 * a wrong temperature must never move the reading (REQ-FUN-004).
 *
 * @param calib     Calibration with the temperature term. Must not be NULL.
 * @param reading   Sample to correct. Must not be NULL.
 * @param out_raw   Receives the corrected raw value. Must not be NULL.
 * @return GG_OK, GG_ERR_NULL, or GG_ERR_RANGE if the raw value is out of range.
 * @req REQ-FUN-004
 */
// DEV-05: service API, called by app/ and tests; cppcheck sees one caller at this stage
// cppcheck-suppress misra-c2012-8.7
gg_status_t calib_temp_correct(const calib_t *calib, const calib_reading_t *reading, uint16_t *out_raw);

#endif /* CALIB_H */
