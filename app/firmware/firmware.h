/**
 * @file firmware.h
 * @brief Firmware application layer: boot, sampling, detector, outputs, event log.
 *
 * The functions here do not touch the registers: they call the HAL, the drivers
 * and the services, so the whole chain runs on the PC with the fakes. main.c
 * only sets up the board and calls these functions from the scheduler.
 *
 * @req REQ-SAF-006 REQ-SAF-007 REQ-ALM-001 REQ-SAF-001
 */
#ifndef FIRMWARE_H
// DEV-02: include guard, not an unused macro
// cppcheck-suppress misra-c2012-2.5
#define FIRMWARE_H

#include <stdbool.h>
#include <stdint.h>

#include "app/detector/detector.h"
#include "common/gg_types/gg_types.h"
#include "hal/reset_cause/reset_cause.h"
#include "services/calib/calib.h"
#include "services/diag/diag.h"
#include "services/filter/filter.h"

/** @brief ADC channels of the two gas sensors (docs/02-architecture.md §2). */
#define FIRMWARE_CHANNEL_MQ5 0u
#define FIRMWARE_CHANNEL_MQ2 1u
/** @brief Number of gas channels. */
#define FIRMWARE_CHANNELS 2u

/** @brief Milliseconds per second, for the uptime stamps of the event log. */
#define FIRMWARE_MS_PER_S 1000u

/** @brief Firmware state owned by the caller: one instance for the whole program. */
typedef struct {
    detector_t det;                          /**< Alarm state machine. */
    filter_t filter[FIRMWARE_CHANNELS];      /**< One moving average per channel. */
    diag_t diag[FIRMWARE_CHANNELS];          /**< One diagnostic per channel. */
    calib_t calib[FIRMWARE_CHANNELS];        /**< One calibration per channel. */
    uint32_t warmup_end_ms;                  /**< Time at which WARMUP ends. */
} firmware_t;

/**
 * @brief Boot: log the reset cause, load the configuration, start the detector.
 * @param fw         Firmware state. Must not be NULL.
 * @param cause      Reset cause read from the hardware.
 * @param now_ms     Current time, the start of the warm-up.
 * @return GG_OK, GG_ERR_NULL, or the first error of a driver or service.
 * @req REQ-SAF-006 REQ-CFG-002
 */
// DEV-05: application API, called by main.c; cppcheck sees one caller
// cppcheck-suppress misra-c2012-8.7
gg_status_t firmware_boot(firmware_t *fw, hal_reset_cause_t cause, uint32_t now_ms);

/**
 * @brief One sampling step: read, filter, correct and select the channels, then run the detector.
 * @param fw      Firmware state. Must not be NULL.
 * @param now_ms  Current time.
 * @return GG_OK or GG_ERR_NULL. A channel that cannot be read is reported to the detector as a fault.
 * @req REQ-ALM-001 REQ-SAF-007
 */
// DEV-05: application API, called by main.c; cppcheck sees one caller
// cppcheck-suppress misra-c2012-8.7
gg_status_t firmware_sample(firmware_t *fw, uint32_t now_ms);

/**
 * @brief Drive the outputs from the detector state: relay and buzzer.
 * @param fw  Firmware state. Must not be NULL.
 * @return GG_OK, GG_ERR_NULL, or the GPIO error.
 * @req REQ-SAF-001 REQ-HMI-003
 */
// DEV-05: application API, called by main.c; cppcheck sees one caller
// cppcheck-suppress misra-c2012-8.7
gg_status_t firmware_output(const firmware_t *fw);

/**
 * @brief Current detector state.
 * @param fw  Firmware state.
 * @return The state, or DET_STATE_FAULT if fw is NULL.
 * @req REQ-ALM-001
 */
// DEV-05: application API, called by main.c and tests; cppcheck sees one caller
// cppcheck-suppress misra-c2012-8.7
detector_state_t firmware_state(const firmware_t *fw);

/**
 * @brief Whether the relay coil should be energised now.
 * @param fw         Firmware state. Must not be NULL.
 * @param energised  Receives the decision. Must not be NULL.
 * @return GG_OK or GG_ERR_NULL.
 * @req REQ-SAF-001
 */
// DEV-05: application API, called by main.c and tests; cppcheck sees one caller
// cppcheck-suppress misra-c2012-8.7
gg_status_t firmware_relay_energised(const firmware_t *fw, bool *energised);

#endif /* FIRMWARE_H */
