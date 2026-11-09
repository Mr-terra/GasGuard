/**
 * @file hmi.h
 * @brief Operator interface logic: button presses, silence, buzzer patterns, functional test.
 *
 * Pure logic, no hardware: the firmware reads the debounced button levels and
 * the detector state, and passes them in with the time. This keeps the timing
 * rules testable on the PC (REQ-NFR-001).
 *
 * @req REQ-HMI-003 REQ-HMI-004 REQ-HMI-005 REQ-HMI-006 REQ-HMI-007
 */
#ifndef HMI_H
// DEV-02: include guard, not an unused macro
// cppcheck-suppress misra-c2012-2.5
#define HMI_H

#include <stdbool.h>
#include <stdint.h>

#include "app/detector/detector.h"
#include "common/gg_types/gg_types.h"

/** @brief A press shorter than this is a short press (REQ-HMI-004), in milliseconds. */
#define HMI_SHORT_PRESS_MAX_MS 1000u

/** @brief A press held this long is a long press (REQ-HMI-005, REQ-HMI-006), in milliseconds. */
#define HMI_LONG_PRESS_MS 3000u

/** @brief An ALARM_2 buzzer silence lasts at most this long (REQ-HMI-004), in milliseconds. */
#define HMI_ALARM_2_SILENCE_MAX_MS 60000u

/** @brief ALARM_1 beeps at 1 Hz: a period of 1000 ms, on for 500 ms of it (REQ-HMI-003). */
#define HMI_ALARM_1_PERIOD_MS 1000u
#define HMI_ALARM_1_ON_MS 500u

/** @brief ALARM_2 beeps at 4 Hz: a period of 250 ms, on for 125 ms of it (REQ-HMI-003). */
#define HMI_ALARM_2_PERIOD_MS 250u
#define HMI_ALARM_2_ON_MS 125u

/** @brief FAULT beeps once every 5 s, for HMI_FAULT_BEEP_MS (REQ-HMI-003). */
#define HMI_FAULT_PERIOD_MS 5000u
#define HMI_FAULT_BEEP_MS 200u

/** @brief Functional test: six steps of one second each (REQ-HMI-006). */
#define HMI_TEST_STEP_MS 1000u
#define HMI_TEST_TOTAL_MS 6000u

/** @brief Length of the concentration text "3276.7 %LEL" with its zero terminator (REQ-HMI-001). */
#define HMI_LEL_TEXT_MAX 12u

/** @brief Event of one button, reported when the press ends or when the long threshold is reached. */
typedef enum {
    HMI_PRESS_NONE = 0,  /**< Nothing to act on. */
    HMI_PRESS_SHORT,     /**< Released after less than HMI_SHORT_PRESS_MAX_MS. */
    HMI_PRESS_LONG       /**< Held for HMI_LONG_PRESS_MS: reported once, while still held. */
} hmi_press_event_t;

/** @brief Press tracking of one button. The level must already be debounced (REQ-HMI-007). */
typedef struct {
    bool held;       /**< True while the button is pressed. */
    bool long_sent; /**< True once the long press was reported for this press. */
    uint32_t since_ms; /**< Time of the start of this press. */
} hmi_press_t;

/** @brief Buzzer silence granted by a short press. Owned by the caller. */
typedef struct {
    bool active;                /**< True when a silence was granted. */
    detector_state_t state;     /**< The alarm state the silence was granted for. */
    uint32_t since_ms;          /**< Time of the acknowledge. */
} hmi_silence_t;

/** @brief Outputs of one step of the functional test. */
typedef struct {
    bool buzzer;            /**< Buzzer on. */
    bool relay_energised;   /**< Relay coil energised. */
    uint8_t red;            /**< Backlight red level. */
    uint8_t green;          /**< Backlight green level. */
    uint8_t blue;           /**< Backlight blue level. */
} hmi_test_out_t;

/**
 * @brief Reset a press tracker to "released".
 * @param press  Tracker. Must not be NULL.
 * @return GG_OK or GG_ERR_NULL.
 * @req REQ-HMI-004
 */
// DEV-05: app API, called by the firmware and tests; cppcheck sees one caller at this stage
// cppcheck-suppress misra-c2012-8.7
gg_status_t hmi_press_init(hmi_press_t *press);

/**
 * @brief Feed one debounced level and get the event, if any.
 * @param press    Tracker. Must not be NULL.
 * @param pressed  Debounced level: true while pressed.
 * @param now_ms   Current time.
 * @param event    Receives the event. Must not be NULL.
 * @return GG_OK or GG_ERR_NULL.
 * @req REQ-HMI-004 REQ-HMI-005 REQ-HMI-006
 */
// DEV-05: app API, called by the firmware and tests; cppcheck sees one caller at this stage
// cppcheck-suppress misra-c2012-8.7
gg_status_t hmi_press_step(hmi_press_t *press, bool pressed, uint32_t now_ms, hmi_press_event_t *event);

/**
 * @brief Reset the silence: the buzzer sounds as its pattern says.
 * @param silence  Silence state. Must not be NULL.
 * @return GG_OK or GG_ERR_NULL.
 * @req REQ-HMI-004
 */
// DEV-05: app API, called by the firmware and tests; cppcheck sees one caller at this stage
// cppcheck-suppress misra-c2012-8.7
gg_status_t hmi_silence_init(hmi_silence_t *silence);

/**
 * @brief Acknowledge a short press: silence the buzzer for the current alarm.
 *
 * Only ALARM_1 and ALARM_2 can be silenced. The silence is tied to the state it
 * was granted in, so a higher alarm sounds again.
 *
 * @param state    Current detector state.
 * @param silence  Silence state. Must not be NULL.
 * @param now_ms   Time of the acknowledge.
 * @return GG_OK or GG_ERR_NULL.
 * @req REQ-HMI-004
 */
// DEV-05: app API, called by the firmware and tests; cppcheck sees one caller at this stage
// cppcheck-suppress misra-c2012-8.7
gg_status_t hmi_acknowledge(detector_state_t state, hmi_silence_t *silence, uint32_t now_ms);

/**
 * @brief Buzzer output for a state at a given time (REQ-HMI-003, REQ-HMI-004).
 * @param state    Detector state.
 * @param silence  Silence state. Must not be NULL.
 * @param now_ms   Current time.
 * @param on       Receives true when the buzzer must sound. Must not be NULL.
 * @return GG_OK or GG_ERR_NULL.
 * @req REQ-HMI-003 REQ-HMI-004
 */
// DEV-05: app API, called by the firmware and tests; cppcheck sees one caller at this stage
// cppcheck-suppress misra-c2012-8.7
gg_status_t hmi_buzzer_on(detector_state_t state, const hmi_silence_t *silence, uint32_t now_ms, bool *on);

/**
 * @brief Outputs of the functional test at a given time since it started (REQ-HMI-006).
 * @param elapsed_ms  Time since the test started.
 * @param out         Receives the outputs. Must not be NULL.
 * @param running     Receives true while the test runs, false once it is over. Must not be NULL.
 * @return GG_OK or GG_ERR_NULL.
 * @req REQ-HMI-006
 */
// DEV-05: app API, called by the firmware and tests; cppcheck sees one caller at this stage
// cppcheck-suppress misra-c2012-8.7
gg_status_t hmi_test_outputs(uint32_t elapsed_ms, hmi_test_out_t *out, bool *running);

/**
 * @brief Text of a concentration for the second LCD line: "12.3 %LEL".
 *
 * The concentration is in 0.1 %LEL; a negative value is shown as zero.
 *
 * @param conc   Concentration, in 0.1 %LEL.
 * @param out    Receives the zero-terminated text. Must not be NULL.
 * @param size   Size of out, at least HMI_LEL_TEXT_MAX.
 * @return GG_OK, GG_ERR_NULL, or GG_ERR_RANGE if size is too small.
 * @req REQ-HMI-001
 */
// DEV-05: app API, called by the firmware display task and tests; cppcheck sees one caller at this stage
// cppcheck-suppress misra-c2012-8.7
gg_status_t hmi_format_lel(gg_conc_t conc, char *out, uint8_t size);

#endif /* HMI_H */
