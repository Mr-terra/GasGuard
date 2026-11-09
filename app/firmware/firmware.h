/**
 * @file firmware.h
 * @brief Firmware application layer: boot, sampling, operator inputs, detector, outputs, event log.
 *
 * The functions here do not touch the registers: they call the HAL, the drivers
 * and the services, so the whole chain runs on the PC with the fakes. main.c
 * only sets up the board and calls these functions from the scheduler.
 *
 * @req REQ-SAF-006 REQ-SAF-007 REQ-SAF-008 REQ-SAF-009 REQ-ALM-001 REQ-SAF-001
 * @req REQ-HMI-004 REQ-HMI-005 REQ-HMI-006 REQ-HMI-007 REQ-HMI-008 REQ-HMI-009 REQ-HMI-010
 */
#ifndef FIRMWARE_H
// DEV-02: include guard, not an unused macro
// cppcheck-suppress misra-c2012-2.5
#define FIRMWARE_H

#include <stdbool.h>
#include <stdint.h>

#include "app/detector/detector.h"
#include "app/hmi/hmi.h"
#include "common/gg_types/gg_types.h"
#include "drivers/button/button.h"
#include "hal/reset_cause/reset_cause.h"
#include "services/calib/calib.h"
#include "services/diag/diag.h"
#include "services/filter/filter.h"

/** @brief ADC channels of the two gas sensors (docs/02-architecture.md §2). */
#define FIRMWARE_CHANNEL_MQ5 0u
/**
 * @brief ADC inputs of the sensors. In the simulation build the MQ-5 channel reads
 * the potentiometer on A2 instead of A0 (REQ-FUN-006, bench tests without gas).
 */
#ifdef GG_SIM_POT
#define FIRMWARE_ADC_MQ5 2u
#else
#define FIRMWARE_ADC_MQ5 0u
#endif
#define FIRMWARE_ADC_MQ2 1u

/** @brief Number of gas channels. */
#define FIRMWARE_CHANNELS 2u

/** @brief Milliseconds per second, for the uptime stamps of the event log. */
#define FIRMWARE_MS_PER_S 1000u

/** @brief Stack watermark: a FAULT below this many free bytes (REQ-SAF-009). */
#define FIRMWARE_STACK_MIN_FREE 64u

/** @brief Maintenance mode ends by itself after this time, in milliseconds (REQ-HMI-010). */
#define FIRMWARE_MAINT_MAX_MS 600000u

/** @brief Firmware state owned by the caller: one instance for the whole program. */
typedef struct {
    detector_t det;                          /**< Alarm state machine. */
    filter_t filter[FIRMWARE_CHANNELS];      /**< One moving average per channel. */
    diag_t diag[FIRMWARE_CHANNELS];          /**< One diagnostic per channel. */
    calib_t calib[FIRMWARE_CHANNELS];        /**< One calibration per channel. */
    uint32_t warmup_end_ms;                  /**< Time at which WARMUP ends. */
    button_t button_db;                      /**< Debounce of the push button (REQ-HMI-007). */
    hmi_press_t button_press;                /**< Press classification of the push button. */
    button_t touch_db;                       /**< Debounce of the touch sensor (REQ-HMI-007). */
    hmi_press_t touch_press;                 /**< Press classification of the touch sensor. */
    hmi_silence_t silence;                   /**< Buzzer silence granted by a short press. */
    bool reset_pending;                      /**< A long press asked for the ALARM_2 reset, for the next sample. */
    bool maint_pending;                      /**< A touch of 3 s asked for MAINTENANCE, for the next sample. */
    uint32_t maint_start_ms;                 /**< Time of the maintenance request. */
    bool test_running;                       /**< The functional test is running (REQ-HMI-006). */
    uint32_t test_start_ms;                  /**< Start of the functional test. */
    detector_fault_code_t pending_fault;     /**< System fault to give the detector at the next sample. */
    gg_conc_t last_conc;                     /**< Concentration of the last sample, for the display (REQ-HMI-001). */
} firmware_t;

/**
 * @brief Boot: log the reset cause, load the configuration, start the detector.
 * @param fw         Firmware state. Must not be NULL.
 * @param cause      Reset cause read from the hardware.
 * @param now_ms     Current time, the start of the warm-up.
 * @return GG_OK, GG_ERR_NULL, or the first error of a driver or service.
 * @req REQ-SAF-006 REQ-CFG-002
 */
gg_status_t firmware_boot(firmware_t *fw, hal_reset_cause_t cause, uint32_t now_ms);

/**
 * @brief One sampling step: read, filter, correct and select the channels, then run the detector.
 * @param fw      Firmware state. Must not be NULL.
 * @param now_ms  Current time.
 * @return GG_OK or GG_ERR_NULL. A channel that cannot be read is reported to the detector as a fault.
 * @req REQ-ALM-001 REQ-SAF-007
 */
gg_status_t firmware_sample(firmware_t *fw, uint32_t now_ms);

/**
 * @brief Operator inputs: debounce the button and the touch sensor, act on their presses.
 *
 * A short press silences an alarm; a long press in ALARM_2 asks for the reset,
 * in NORMAL it starts the functional test; a touch of 3 s in NORMAL asks for
 * MAINTENANCE. The requests take effect at the next sample.
 *
 * @param fw         Firmware state. Must not be NULL.
 * @param now_ms     Current time.
 * @param button_raw Raw level of the push button: true when the pin reads high (active low).
 * @param touch_raw  Raw level of the touch sensor: true when touched.
 * @return GG_OK or GG_ERR_NULL.
 * @req REQ-HMI-004 REQ-HMI-005 REQ-HMI-006 REQ-HMI-007 REQ-HMI-008
 */
gg_status_t firmware_inputs(firmware_t *fw, uint32_t now_ms, bool button_raw, bool touch_raw);

/**
 * @brief Drive the outputs from the detector state, the silence and the functional test.
 * @param fw      Firmware state. Must not be NULL.
 * @param now_ms  Current time, for the buzzer patterns.
 * @return GG_OK, GG_ERR_NULL, or the GPIO error.
 * @req REQ-SAF-001 REQ-HMI-003 REQ-HMI-006
 */
gg_status_t firmware_output(const firmware_t *fw, uint32_t now_ms);

/**
 * @brief Stack watermark check, called once a second (REQ-SAF-009).
 * @param fw  Firmware state. Must not be NULL.
 * @return GG_OK or GG_ERR_NULL. A stack below FIRMWARE_STACK_MIN_FREE is a fault at the next sample.
 * @req REQ-SAF-009
 */
// DEV-05: service/driver/app API, called by app/, main.c and tests; cppcheck sees one caller at this stage
// cppcheck-suppress misra-c2012-8.7
gg_status_t firmware_health(firmware_t *fw);

/**
 * @brief Report the end of a scheduler cycle: log an overrun, raise FAULT after three.
 * @param fw       Firmware state. Must not be NULL.
 * @param overrun  True if this cycle took longer than 100 ms (REQ-SAF-008).
 * @param fault    True if three overruns came in a row: FAULT at the next sample (REQ-SAF-008).
 * @param now_ms   Current time.
 * @return GG_OK, GG_ERR_NULL, or the event log error.
 * @req REQ-SAF-008
 */
gg_status_t firmware_report_timing(firmware_t *fw, bool overrun, bool fault, uint32_t now_ms);

/**
 * @brief Current detector state.
 * @param fw  Firmware state.
 * @return The state, or DET_STATE_FAULT if fw is NULL.
 * @req REQ-ALM-001
 */
detector_state_t firmware_state(const firmware_t *fw);

/**
 * @brief Concentration of the last sample, in 0.1 %LEL (REQ-HMI-001).
 * @param fw  Firmware state.
 * @return The concentration, or 0 if fw is NULL.
 * @req REQ-HMI-001
 */
// DEV-05: service/driver/app API, called by app/, main.c and tests; cppcheck sees one caller at this stage
// cppcheck-suppress misra-c2012-8.7
gg_conc_t firmware_concentration(const firmware_t *fw);

/**
 * @brief Whether the functional test is running (the display shows TEST).
 * @param fw  Firmware state.
 * @return True while the test runs; false if fw is NULL.
 * @req REQ-HMI-006
 */
// DEV-05: service/driver/app API, called by app/, main.c and tests; cppcheck sees one caller at this stage
// cppcheck-suppress misra-c2012-8.7
bool firmware_test_running(const firmware_t *fw);

/**
 * @brief Whether the relay coil should be energised now.
 * @param fw         Firmware state. Must not be NULL.
 * @param energised  Receives the decision. Must not be NULL.
 * @return GG_OK or GG_ERR_NULL.
 * @req REQ-SAF-001
 */
// DEV-05: service/driver/app API, called by app/, main.c and tests; cppcheck sees one caller at this stage
// cppcheck-suppress misra-c2012-8.7
gg_status_t firmware_relay_energised(const firmware_t *fw, bool *energised);

#endif /* FIRMWARE_H */
