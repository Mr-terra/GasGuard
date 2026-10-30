/**
 * @file detector.h
 * @brief Alarm state machine: warm-up, normal, two alarm levels with a latch, and fault.
 *
 * The detector receives one concentration per sample (the maximum of the valid
 * channels, REQ-FUN-005) and decides the state and the relay output. It has no
 * hardware dependency: this file includes no AVR header (R20).
 *
 * @req REQ-ALM-001 REQ-ALM-002 REQ-ALM-003 REQ-ALM-004 REQ-ALM-005 REQ-ALM-006
 * @req REQ-SAF-001 REQ-SAF-007 REQ-FUN-005
 */
#ifndef DETECTOR_H
// DEV-02: include guard, not an unused macro
// cppcheck-suppress misra-c2012-2.5
#define DETECTOR_H

#include <stdbool.h>
#include <stdint.h>

#include "common/gg_types.h"

/** @brief Default ALARM_1 threshold: 10 %LEL (REQ-ALM-001). */
#define DETECTOR_AL1_DEFAULT 100

/** @brief Default ALARM_2 threshold: 25 %LEL (REQ-ALM-002). */
#define DETECTOR_AL2_DEFAULT 250

/** @brief Default hysteresis: 2 %LEL (REQ-ALM-003). */
#define DETECTOR_HYSTERESIS_DEFAULT 20

/** @brief Highest allowed ALARM_2 threshold: 60 %LEL (REQ-ALM-005). */
#define DETECTOR_AL2_MAX 600

/** @brief Consecutive samples needed to enter an alarm (REQ-ALM-001, REQ-ALM-002). */
#define DETECTOR_ENTER_SAMPLES 3u

/** @brief Consecutive samples below AL1 - hysteresis needed to clear ALARM_1 (REQ-ALM-003). */
#define DETECTOR_CLEAR_SAMPLES 10u

/** @brief Operating state of the detector. */
typedef enum {
    DET_STATE_WARMUP = 0,  /**< Warm-up: alarms inhibited and shown (REQ-ALM-006). */
    DET_STATE_NORMAL,      /**< Healthy, no alarm. Relay energised (REQ-SAF-001). */
    DET_STATE_ALARM_1,     /**< First-level alarm, clears automatically (REQ-ALM-003). */
    DET_STATE_ALARM_2,     /**< Second-level alarm, latched until operator reset (REQ-ALM-004). */
    DET_STATE_FAULT        /**< Latched fault. Relay de-energised (REQ-SAF-007). */
} detector_state_t;

/** @brief Alarm thresholds, in 0.1 %LEL. Validated by detector_init(). */
typedef struct {
    gg_conc_t al1;          /**< ALARM_1 threshold. */
    gg_conc_t al2;          /**< ALARM_2 threshold. */
    gg_conc_t hysteresis;   /**< Clear level is al1 - hysteresis. */
} detector_thresholds_t;

/** @brief One sample for the state machine. */
typedef struct {
    gg_conc_t conc;         /**< Maximum concentration of the valid channels (REQ-FUN-005). */
    bool fault;             /**< True if all channels are faulty or the ADC is faulty (REQ-SAF-007). */
    bool reset_requested;   /**< True if the operator long-pressed to reset ALARM_2 (REQ-HMI-005). */
} detector_input_t;

/** @brief State of the detector. Owned by the caller. */
typedef struct {
    detector_state_t state;           /**< Current state. */
    detector_thresholds_t thresholds; /**< Thresholds in use. */
    uint8_t above_al1;                /**< Consecutive samples at or above al1. */
    uint8_t above_al2;                /**< Consecutive samples at or above al2. */
    uint8_t below_clear;              /**< Consecutive samples below al1 - hysteresis. */
} detector_t;

/**
 * @brief Validate thresholds: 0 < AL1 < AL2 <= 60 %LEL (REQ-ALM-005).
 *
 * @param thresholds  Thresholds to check. Must not be NULL.
 * @return GG_OK, GG_ERR_NULL, or GG_ERR_RANGE if the ordering or the limits are broken.
 * @req REQ-ALM-005
 */
// DEV-05: service API, called by app/ and tests; cppcheck sees one caller at this stage
// cppcheck-suppress misra-c2012-8.7
gg_status_t detector_thresholds_check(const detector_thresholds_t *thresholds);

/**
 * @brief Start the detector in WARMUP with validated thresholds.
 *
 * @param det         Detector to initialise. Must not be NULL.
 * @param thresholds  Thresholds to use. Must not be NULL and must pass detector_thresholds_check().
 * @return GG_OK, GG_ERR_NULL, or GG_ERR_RANGE if the thresholds are rejected.
 * @req REQ-ALM-005 REQ-ALM-006
 */
// DEV-05: service API, called by app/ and tests; cppcheck sees one caller at this stage
// cppcheck-suppress misra-c2012-8.7
gg_status_t detector_init(detector_t *det, const detector_thresholds_t *thresholds);

/**
 * @brief End the warm-up period: WARMUP becomes NORMAL, unless a fault is latched.
 *
 * @param det  Detector. Must not be NULL.
 * @return GG_OK or GG_ERR_NULL.
 * @req REQ-ALM-006
 */
// DEV-05: service API, called by app/ and tests; cppcheck sees one caller at this stage
// cppcheck-suppress misra-c2012-8.7
gg_status_t detector_end_warmup(detector_t *det);

/**
 * @brief Advance the state machine by one sample.
 *
 * A fault input latches FAULT from any state. Otherwise the consecutive-sample
 * counters are updated and the transitions of docs/02-architecture.md §3 apply.
 *
 * @param det  Detector. Must not be NULL.
 * @param in   Sample. Must not be NULL.
 * @return GG_OK or GG_ERR_NULL.
 * @req REQ-ALM-001 REQ-ALM-002 REQ-ALM-003 REQ-ALM-004 REQ-ALM-006 REQ-SAF-007
 */
// DEV-05: service API, called by app/ and tests; cppcheck sees one caller at this stage
// cppcheck-suppress misra-c2012-8.7
gg_status_t detector_step(detector_t *det, const detector_input_t *in);

/**
 * @brief Report the current state.
 *
 * @param det    Detector. Must not be NULL.
 * @param state  Receives the state. Must not be NULL.
 * @return GG_OK or GG_ERR_NULL.
 * @req REQ-ALM-006
 */
// DEV-05: service API, called by app/ and tests; cppcheck sees one caller at this stage
// cppcheck-suppress misra-c2012-8.7
gg_status_t detector_state(const detector_t *det, detector_state_t *state);

/**
 * @brief Report whether the alarm relay is energised.
 *
 * Energised only in the healthy, no-alarm states (WARMUP and NORMAL). ALARM_1,
 * ALARM_2 and FAULT de-energise it. ALARM_1 is not named in REQ-SAF-001; this is
 * the fail-safe reading, to be confirmed by the author.
 *
 * @param det         Detector. Must not be NULL.
 * @param energised   Receives true if the relay coil must be driven. Must not be NULL.
 * @return GG_OK or GG_ERR_NULL.
 * @req REQ-SAF-001
 */
// DEV-05: service API, called by app/ and tests; cppcheck sees one caller at this stage
// cppcheck-suppress misra-c2012-8.7
gg_status_t detector_relay_energised(const detector_t *det, bool *energised);

#endif /* DETECTOR_H */
