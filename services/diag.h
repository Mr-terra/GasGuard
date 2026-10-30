/**
 * @file diag.h
 * @brief Sensor and reference diagnostics: open/short circuit, stuck value, bandgap.
 *
 * A diagnostic that fails never makes a reading "more reassuring". The caller
 * uses diag_is_faulty() to exclude a channel from the maximum (REQ-FUN-005) and
 * to raise a channel fault (REQ-SAF-007, FMEA F01, F02, F03, F06).
 *
 * @req REQ-SAF-002 REQ-SAF-003 REQ-SAF-004
 */
#ifndef DIAG_H
// DEV-02: include guard, not an unused macro
// cppcheck-suppress misra-c2012-2.5
#define DIAG_H

#include <stdbool.h>
#include <stdint.h>

#include "common/gg_types.h"

/** @brief Raw value at or below which a channel reads as an open circuit (REQ-SAF-002). */
#define DIAG_OPEN_MAX 5u

/** @brief Raw value at or above which a channel reads as a short circuit (REQ-SAF-002). */
#define DIAG_SHORT_MIN 1018u

/** @brief Consecutive out-of-range samples that make a range fault (REQ-SAF-002). */
#define DIAG_RANGE_SAMPLES 5u

/** @brief Time without change that makes a channel stuck: 10 minutes (REQ-SAF-003). */
#define DIAG_STUCK_MS 600000u

/** @brief Largest change, in LSB, still counted as "not moving" (REQ-SAF-003). */
#define DIAG_STUCK_LSB 1u

/**
 * @brief Bandgap reading expected at 5 V: 1.1 V / 5 V x 1023, rounded (REQ-SAF-004).
 *
 * The ADC uses AVcc as reference, so the nominal count depends on the supply.
 * The value is set for a USB supply near 5 V. At 4.75 V the reading is about 237
 * counts and at 5.25 V about 214: both inside the band. The band accepts roughly
 * 4.55 V to 5.56 V. A USB-only setup is therefore enough. If the heaters pull the
 * supply below about 4.5 V, the check fails and the detector goes to FAULT (FMEA F20).
 */
#define DIAG_BANDGAP_NOMINAL 225u

/** @brief Allowed deviation of the bandgap reading from nominal, in percent (REQ-SAF-004). */
#define DIAG_BANDGAP_TOLERANCE_PCT 10u

/**
 * @brief Diagnostic state of one channel.
 *
 * Faults are latched until diag_init(): a sensor that failed once is not
 * trusted again without a reset. This is a conservative choice (see docs).
 */
typedef struct {
    uint8_t range_count;        /**< Consecutive out-of-range samples. */
    bool range_fault;           /**< Latched open/short fault. */
    uint16_t window_min;        /**< Lowest raw value since the window started. */
    uint16_t window_max;        /**< Highest raw value since the window started. */
    uint32_t window_start_ms;   /**< Time the current stuck window started. */
    bool window_started;        /**< False until the first stuck-check sample. */
    bool stuck_fault;           /**< Latched stuck-at fault. */
} diag_t;

/**
 * @brief Clear all diagnostic state of a channel.
 *
 * @param diag  Diagnostic state to clear. Must not be NULL.
 * @return GG_OK or GG_ERR_NULL.
 * @req REQ-SAF-002
 */
// DEV-05: service API, called by app/ and tests; cppcheck sees one caller at this stage
// cppcheck-suppress misra-c2012-8.7
gg_status_t diag_init(diag_t *diag);

/**
 * @brief Check one raw sample for an open or short circuit.
 *
 * @param diag  Channel state. Must not be NULL.
 * @param raw   Raw ADC value, 0 .. HAL_ADC_RAW_MAX.
 * @return GG_OK, GG_ERR_NULL, or GG_ERR_RANGE if raw is out of range.
 * @req REQ-SAF-002
 */
// DEV-05: service API, called by app/ and tests; cppcheck sees one caller at this stage
// cppcheck-suppress misra-c2012-8.7
gg_status_t diag_check_range(diag_t *diag, uint16_t raw);

/** @brief One sample for the stuck-at check. Grouped so that its fields cannot be swapped. */
typedef struct {
    uint16_t raw;        /**< Raw ADC value, 0 .. HAL_ADC_RAW_MAX. */
    uint32_t now_ms;     /**< Uptime in milliseconds, non-decreasing between calls. */
    bool in_warmup;      /**< True while the warm-up period is running. */
} diag_stuck_sample_t;

/**
 * @brief Check one raw sample for a stuck value.
 *
 * The window restarts whenever the value moves by more than DIAG_STUCK_LSB.
 * If the window reaches DIAG_STUCK_MS without a move, the channel is stuck.
 * While in warm-up the window keeps restarting and no fault can be raised (REQ-ALM-006).
 *
 * @param diag    Channel state. Must not be NULL.
 * @param sample  Sample to check. Must not be NULL.
 * @return GG_OK, GG_ERR_NULL, or GG_ERR_RANGE if the raw value is out of range.
 * @req REQ-SAF-003
 */
// DEV-05: service API, called by app/ and tests; cppcheck sees one caller at this stage
// cppcheck-suppress misra-c2012-8.7
gg_status_t diag_check_stuck(diag_t *diag, const diag_stuck_sample_t *sample);

/**
 * @brief Report whether any diagnostic has latched a fault on the channel.
 *
 * @param diag       Channel state. Must not be NULL.
 * @param out_fault  Receives true if a range or stuck fault is latched. Must not be NULL.
 * @return GG_OK or GG_ERR_NULL.
 * @req REQ-SAF-002 REQ-SAF-003
 */
// DEV-05: service API, called by app/ and tests; cppcheck sees one caller at this stage
// cppcheck-suppress misra-c2012-8.7
gg_status_t diag_is_faulty(const diag_t *diag, bool *out_fault);

/**
 * @brief Check the internal bandgap reference reading (REQ-SAF-004).
 *
 * @param bandgap_raw  Raw ADC value of the 1.1 V bandgap, 0 .. HAL_ADC_RAW_MAX.
 * @param out_ok       Receives true if the reading is within the tolerance. Must not be NULL.
 * @return GG_OK, GG_ERR_NULL, or GG_ERR_RANGE if bandgap_raw is out of range.
 * @req REQ-SAF-004
 */
// DEV-05: service API, called by app/ and tests; cppcheck sees one caller at this stage
// cppcheck-suppress misra-c2012-8.7
gg_status_t diag_bandgap_ok(uint16_t bandgap_raw, bool *out_ok);

#endif /* DIAG_H */
