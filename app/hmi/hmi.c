/**
 * @file hmi.c
 * @brief Operator interface logic: press classification, silence, buzzer patterns, functional test.
 *
 * How it works
 * ------------
 * A press is classified when it ends (short, under 1 s) or when it reaches 3 s
 * while still held (long, reported once). A short press on an alarm silences
 * the buzzer; the silence is tied to the alarm level it was given for, so a
 * higher alarm sounds again, and an ALARM_2 silence ends after 60 s. Each
 * buzzer pattern is a phase of the time modulo its period: the buzzer sounds
 * in the first part of each period. The functional test is a fixed sequence of
 * six one-second steps, after which the caller returns to NORMAL.
 *
 * @req REQ-HMI-003 REQ-HMI-004 REQ-HMI-005 REQ-HMI-006 REQ-HMI-007
 */
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "app/hmi/hmi.h"
#include "common/gg_types/gg_types.h"

/** @brief Digits of the integer part of a concentration (up to 5 for an int16). */
#define HMI_LEL_DIGITS 5u

/** @brief ASCII codes used by the formatter. */
#define HMI_ASCII_ZERO 48u
#define HMI_ASCII_POINT 46u

/** @brief Backlight levels used by the functional test. */
#define HMI_LEVEL_OFF 0x00u
#define HMI_LEVEL_FULL 0xFFu

/** @brief Step of the functional test in which the relay is de-energised, then energised again. */
#define HMI_TEST_STEP_RELAY_OFF 4u

/** @brief Buzzer pattern of one state: on for on_ms at the start of each period_ms. */
typedef struct {
    uint32_t period_ms;  /**< Period of the pattern, in milliseconds. */
    uint32_t on_ms;      /**< Time at the start of each period during which the buzzer sounds. */
} hmi_pattern_t;

/**
 * @brief Fill in the pattern of a state (REQ-HMI-003).
 *
 * Quiet states get a pattern that is never on (period 1, on 0). An unknown
 * state gets one that is always on, so the operator notices it.
 *
 * @param state    Detector state.
 * @param pattern  Receives the pattern. Must not be NULL.
 */
static void pattern_of(detector_state_t state, hmi_pattern_t *pattern)
{
    switch (state) {
        case DET_STATE_ALARM_1:
            pattern->period_ms = HMI_ALARM_1_PERIOD_MS;
            pattern->on_ms = HMI_ALARM_1_ON_MS;
            break;
        case DET_STATE_ALARM_2:
            pattern->period_ms = HMI_ALARM_2_PERIOD_MS;
            pattern->on_ms = HMI_ALARM_2_ON_MS;
            break;
        case DET_STATE_FAULT:
            pattern->period_ms = HMI_FAULT_PERIOD_MS;
            pattern->on_ms = HMI_FAULT_BEEP_MS;
            break;
        case DET_STATE_WARMUP:
        case DET_STATE_NORMAL:
        case DET_STATE_MAINTENANCE:
            pattern->period_ms = 1u;
            pattern->on_ms = 0u;
            break;
        default:
            pattern->period_ms = 1u;
            pattern->on_ms = 1u;
            break;
    }
}

/** @brief True when the state can be silenced (REQ-HMI-004). */
static bool is_alarm(detector_state_t state)
{
    return (state == DET_STATE_ALARM_1) || (state == DET_STATE_ALARM_2);
}

/** @brief True while the silence granted by an acknowledge still applies. */
static bool silence_applies(detector_state_t state, const hmi_silence_t *silence, uint32_t now_ms)
{
    bool applies = silence->active && (silence->state == state);

    if (applies && (state == DET_STATE_ALARM_2)) {
        /* REQ-HMI-004: an ALARM_2 silence never lasts more than 60 s. Unsigned difference handles wrap. */
        applies = (now_ms - silence->since_ms) < HMI_ALARM_2_SILENCE_MAX_MS;
    }
    return applies;
}

gg_status_t hmi_press_init(hmi_press_t *press)
{
    gg_status_t status = GG_OK;

    if (press == NULL) {
        status = GG_ERR_NULL;
    } else {
        press->held = false;
        press->long_sent = false;
        press->since_ms = 0u;
    }
    return status;
}

/** @brief Start of a press: remember when it began. */
static void press_begin(hmi_press_t *press, uint32_t now_ms)
{
    press->held = true;
    press->long_sent = false;
    press->since_ms = now_ms;
}

/** @brief End of a press: a short one is reported, a long one already was or is dropped. */
static hmi_press_event_t press_end(hmi_press_t *press, uint32_t now_ms)
{
    hmi_press_event_t event = HMI_PRESS_NONE;

    press->held = false;
    if (!press->long_sent && ((now_ms - press->since_ms) < HMI_SHORT_PRESS_MAX_MS)) {
        event = HMI_PRESS_SHORT;
    }
    return event;
}

gg_status_t hmi_press_step(hmi_press_t *press, bool pressed, uint32_t now_ms, hmi_press_event_t *event)
{
    gg_status_t status = GG_OK;

    if ((press == NULL) || (event == NULL)) {
        status = GG_ERR_NULL;
    } else {
        *event = HMI_PRESS_NONE;
        if (pressed && !press->held) {
            press_begin(press, now_ms);
        } else if (pressed && !press->long_sent && ((now_ms - press->since_ms) >= HMI_LONG_PRESS_MS)) {
            press->long_sent = true;
            *event = HMI_PRESS_LONG;
        } else if (!pressed && press->held) {
            *event = press_end(press, now_ms);
        } else {
            /* Still held with the long press already reported, or still released: nothing. */
        }
    }
    return status;
}

gg_status_t hmi_silence_init(hmi_silence_t *silence)
{
    gg_status_t status = GG_OK;

    if (silence == NULL) {
        status = GG_ERR_NULL;
    } else {
        silence->active = false;
        silence->state = DET_STATE_NORMAL;
        silence->since_ms = 0u;
    }
    return status;
}

gg_status_t hmi_acknowledge(detector_state_t state, hmi_silence_t *silence, uint32_t now_ms)
{
    gg_status_t status = GG_OK;

    if (silence == NULL) {
        status = GG_ERR_NULL;
    } else if (is_alarm(state)) {
        silence->active = true;
        silence->state = state;
        silence->since_ms = now_ms;
    } else {
        /* Only an alarm can be silenced; a fault keeps its beep (REQ-HMI-003). */
    }
    return status;
}

gg_status_t hmi_buzzer_on(detector_state_t state, const hmi_silence_t *silence, uint32_t now_ms, bool *on)
{
    gg_status_t status = GG_OK;
    hmi_pattern_t pattern;

    if ((silence == NULL) || (on == NULL)) {
        status = GG_ERR_NULL;
    } else if (is_alarm(state) && silence_applies(state, silence, now_ms)) {
        *on = false;
    } else {
        pattern_of(state, &pattern);
        *on = (now_ms % pattern.period_ms) < pattern.on_ms;
    }
    return status;
}

/** @brief Backlight levels of the functional test step: red, then green, then blue, then yellow. */
static void test_colour(uint32_t step, hmi_test_out_t *out)
{
    out->red = HMI_LEVEL_OFF;
    out->green = HMI_LEVEL_OFF;
    out->blue = HMI_LEVEL_OFF;
    switch (step) {
        case 0u:
            out->red = HMI_LEVEL_FULL;
            break;
        case 1u:
            out->green = HMI_LEVEL_FULL;
            break;
        case 2u:
            out->blue = HMI_LEVEL_FULL;
            break;
        case 3u:
            out->red = HMI_LEVEL_FULL;
            out->green = HMI_LEVEL_FULL;
            break;
        default:
            break;   /* Steps 4 and 5: colours off, the relay is tested. */
    }
}

gg_status_t hmi_test_outputs(uint32_t elapsed_ms, hmi_test_out_t *out, bool *running)
{
    gg_status_t status = GG_OK;
    uint32_t step = elapsed_ms / HMI_TEST_STEP_MS;

    if ((out == NULL) || (running == NULL)) {
        status = GG_ERR_NULL;
    } else {
        *running = (elapsed_ms < HMI_TEST_TOTAL_MS);
        test_colour(step, out);
        out->buzzer = (step == 0u);
        out->relay_energised = (step != HMI_TEST_STEP_RELAY_OFF);
    }
    return status;
}

/**
 * @brief Write the integer part, then the tenth, then the unit (REQ-HMI-001).
 * @param value  Concentration, zero or more, in 0.1 %LEL.
 * @param out    Buffer of at least HMI_LEL_TEXT_MAX bytes.
 */
static void format_lel_digits(uint16_t value, char *out)
{
    /* Unit written after the number, with its zero terminator. */
    static const char lel_unit[] = " %LEL";
    uint16_t whole = value / 10u;
    uint8_t tenth = 0u;
    uint8_t digits[HMI_LEL_DIGITS];
    uint8_t n = 0u;
    uint8_t pos = 0u;
    uint8_t i;

    /* Integer digits come out least significant first; write them back to front. */
    do {
        digits[n] = (uint8_t)(HMI_ASCII_ZERO + (uint8_t)(whole % 10u));
        n++;
        whole = whole / 10u;
    } while ((whole > 0u) && (n < HMI_LEL_DIGITS));
    while (n > 0u) {
        n--;
        out[pos] = (char)digits[n];
        pos++;
    }
    out[pos] = (char)HMI_ASCII_POINT;
    pos++;
    tenth = (uint8_t)(HMI_ASCII_ZERO + (uint8_t)(value % 10u));
    out[pos] = (char)tenth;
    pos++;
    for (i = 0u; i < (uint8_t)sizeof(lel_unit); i++) {
        out[pos] = lel_unit[i];
        pos++;
    }
}

gg_status_t hmi_format_lel(gg_conc_t conc, char *out, uint8_t size)
{
    gg_status_t status = GG_OK;

    if (out == NULL) {
        status = GG_ERR_NULL;
    } else if (size < HMI_LEL_TEXT_MAX) {
        status = GG_ERR_RANGE;
    } else {
        uint16_t value = (conc < 0) ? 0u : (uint16_t)conc;

        format_lel_digits(value, out);
    }
    return status;
}
