/* Unit tests for app/hmi: press classification, silence, buzzer patterns, functional test (REQ-HMI-*). */
#include "unity.h"
#include "common/gg_types/gg_types.h"
#include "app/detector/detector.h"
#include "app/hmi/hmi.h"

#include <stdbool.h>
#include <stdint.h>

/* DEV-03: Unity hooks, called by the framework; MISRA 8.6 and unusedFunction do not apply. */
// cppcheck-suppress misra-c2012-8.6
void setUp(void);
// cppcheck-suppress misra-c2012-8.6
void tearDown(void);

void setUp(void) {}

void tearDown(void) {}

/* @verifies REQ-HMI-004 */
static void test_hmi_press_shorter_than_one_second_is_a_short_press(void)
{
    hmi_press_t press;
    hmi_press_event_t event = HMI_PRESS_LONG;

    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_press_init(&press));
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_press_step(&press, true, 1000u, &event));
    TEST_ASSERT_EQUAL_INT(HMI_PRESS_NONE, event);
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_press_step(&press, false, 1500u, &event));
    TEST_ASSERT_EQUAL_INT(HMI_PRESS_SHORT, event);
}

/* @verifies REQ-HMI-004 */
static void test_hmi_press_between_one_and_three_seconds_is_neither(void)
{
    hmi_press_t press;
    hmi_press_event_t event = HMI_PRESS_LONG;

    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_press_init(&press));
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_press_step(&press, true, 0u, &event));
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_press_step(&press, false, 2000u, &event));
    TEST_ASSERT_EQUAL_INT(HMI_PRESS_NONE, event);
}

/* @verifies REQ-HMI-005 */
static void test_hmi_press_held_for_three_seconds_is_a_long_press_fired_once(void)
{
    hmi_press_t press;
    hmi_press_event_t event = HMI_PRESS_NONE;

    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_press_init(&press));
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_press_step(&press, true, 0u, &event));
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_press_step(&press, true, 2999u, &event));
    TEST_ASSERT_EQUAL_INT(HMI_PRESS_NONE, event);
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_press_step(&press, true, 3000u, &event));
    TEST_ASSERT_EQUAL_INT(HMI_PRESS_LONG, event);
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_press_step(&press, true, 3500u, &event));
    TEST_ASSERT_EQUAL_INT(HMI_PRESS_NONE, event);
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_press_step(&press, false, 4000u, &event));
    TEST_ASSERT_EQUAL_INT(HMI_PRESS_NONE, event);
}

/* @verifies REQ-NFR-002 */
static void test_hmi_press_rejects_null(void)
{
    hmi_press_event_t event = HMI_PRESS_NONE;

    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, hmi_press_init(NULL));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, hmi_press_step(NULL, true, 0u, &event));
}

/* @verifies REQ-HMI-004 */
static void test_hmi_ack_silences_alarm_1_buzzer_until_the_state_changes(void)
{
    hmi_silence_t silence;
    bool on = true;

    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_silence_init(&silence));
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_acknowledge(DET_STATE_ALARM_1, &silence, 0u));
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_buzzer_on(DET_STATE_ALARM_1, &silence, 0u, &on));
    TEST_ASSERT_FALSE(on);
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_buzzer_on(DET_STATE_ALARM_1, &silence, 600000u, &on));
    TEST_ASSERT_FALSE(on);
}

/* @verifies REQ-HMI-004 */
static void test_hmi_alarm_2_silence_expires_after_60_seconds(void)
{
    hmi_silence_t silence;
    bool on = false;

    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_silence_init(&silence));
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_acknowledge(DET_STATE_ALARM_2, &silence, 0u));
    /* 59.999 s after the ack: silenced, even at a phase where the buzzer would sound. */
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_buzzer_on(DET_STATE_ALARM_2, &silence, 59999u, &on));
    TEST_ASSERT_FALSE(on);
    /* At 60 s the silence is over: the 4 Hz pattern sounds again (phase 0 is on). */
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_buzzer_on(DET_STATE_ALARM_2, &silence, 60000u, &on));
    TEST_ASSERT_TRUE(on);
}

/* @verifies REQ-HMI-004 */
static void test_hmi_silence_granted_in_alarm_1_does_not_carry_into_alarm_2(void)
{
    hmi_silence_t silence;
    bool on = false;

    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_silence_init(&silence));
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_acknowledge(DET_STATE_ALARM_1, &silence, 0u));
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_buzzer_on(DET_STATE_ALARM_2, &silence, 0u, &on));
    TEST_ASSERT_TRUE(on);
}

/* @verifies REQ-HMI-004 */
static void test_hmi_ack_outside_an_alarm_does_nothing(void)
{
    hmi_silence_t silence;
    bool on = true;

    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_silence_init(&silence));
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_acknowledge(DET_STATE_NORMAL, &silence, 0u));
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_buzzer_on(DET_STATE_NORMAL, &silence, 0u, &on));
    TEST_ASSERT_FALSE(on);
}

/* @verifies REQ-HMI-003 */
static void test_hmi_alarm_1_pattern_is_1_hz(void)
{
    hmi_silence_t silence;
    bool on = false;

    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_silence_init(&silence));
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_buzzer_on(DET_STATE_ALARM_1, &silence, 0u, &on));
    TEST_ASSERT_TRUE(on);
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_buzzer_on(DET_STATE_ALARM_1, &silence, 500u, &on));
    TEST_ASSERT_FALSE(on);
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_buzzer_on(DET_STATE_ALARM_1, &silence, 1000u, &on));
    TEST_ASSERT_TRUE(on);
}

/* @verifies REQ-HMI-003 */
static void test_hmi_alarm_2_pattern_is_4_hz(void)
{
    hmi_silence_t silence;
    bool on = false;

    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_silence_init(&silence));
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_buzzer_on(DET_STATE_ALARM_2, &silence, 0u, &on));
    TEST_ASSERT_TRUE(on);
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_buzzer_on(DET_STATE_ALARM_2, &silence, 125u, &on));
    TEST_ASSERT_FALSE(on);
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_buzzer_on(DET_STATE_ALARM_2, &silence, 250u, &on));
    TEST_ASSERT_TRUE(on);
}

/* @verifies REQ-HMI-003 */
static void test_hmi_fault_beeps_once_every_five_seconds_and_cannot_be_silenced(void)
{
    hmi_silence_t silence;
    bool on = false;

    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_silence_init(&silence));
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_acknowledge(DET_STATE_FAULT, &silence, 0u));
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_buzzer_on(DET_STATE_FAULT, &silence, 0u, &on));
    TEST_ASSERT_TRUE(on);
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_buzzer_on(DET_STATE_FAULT, &silence, 200u, &on));
    TEST_ASSERT_FALSE(on);
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_buzzer_on(DET_STATE_FAULT, &silence, 4999u, &on));
    TEST_ASSERT_FALSE(on);
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_buzzer_on(DET_STATE_FAULT, &silence, 5000u, &on));
    TEST_ASSERT_TRUE(on);
}

/* @verifies REQ-HMI-003 */
static void test_hmi_quiet_states_keep_the_buzzer_off(void)
{
    hmi_silence_t silence;
    bool on = true;

    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_silence_init(&silence));
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_buzzer_on(DET_STATE_WARMUP, &silence, 0u, &on));
    TEST_ASSERT_FALSE(on);
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_buzzer_on(DET_STATE_NORMAL, &silence, 0u, &on));
    TEST_ASSERT_FALSE(on);
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_buzzer_on(DET_STATE_MAINTENANCE, &silence, 0u, &on));
    TEST_ASSERT_FALSE(on);
}

/* @verifies REQ-HMI-006 */
static void test_hmi_functional_test_runs_buzzer_colours_and_relay_then_ends(void)
{
    hmi_test_out_t out;
    bool running = false;

    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_test_outputs(0u, &out, &running));
    TEST_ASSERT_TRUE(running);
    TEST_ASSERT_TRUE(out.buzzer);
    TEST_ASSERT_EQUAL_UINT8(0xFFu, out.red);
    TEST_ASSERT_EQUAL_UINT8(0u, out.green);
    TEST_ASSERT_TRUE(out.relay_energised);

    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_test_outputs(1000u, &out, &running));
    TEST_ASSERT_EQUAL_UINT8(0u, out.red);
    TEST_ASSERT_EQUAL_UINT8(0xFFu, out.green);

    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_test_outputs(4000u, &out, &running));
    TEST_ASSERT_FALSE(out.relay_energised);

    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_test_outputs(5000u, &out, &running));
    TEST_ASSERT_TRUE(out.relay_energised);

    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_test_outputs(6000u, &out, &running));
    TEST_ASSERT_FALSE(running);
}

/* @verifies REQ-NFR-002 */
static void test_hmi_null_arguments_are_rejected(void)
{
    hmi_silence_t silence;
    bool on = false;
    hmi_test_out_t out;
    bool running = false;

    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_silence_init(&silence));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, hmi_silence_init(NULL));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, hmi_acknowledge(DET_STATE_ALARM_1, NULL, 0u));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, hmi_buzzer_on(DET_STATE_ALARM_1, &silence, 0u, NULL));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, hmi_buzzer_on(DET_STATE_ALARM_1, NULL, 0u, &on));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, hmi_test_outputs(0u, NULL, &running));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, hmi_test_outputs(0u, &out, NULL));
}

/* @verifies REQ-HMI-001 */
static void test_hmi_format_lel_puts_the_tenth_after_the_point(void)
{
    char text[HMI_LEL_TEXT_MAX];

    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_format_lel(123, text, HMI_LEL_TEXT_MAX));
    TEST_ASSERT_EQUAL_STRING("12.3 %LEL", text);
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_format_lel(5, text, HMI_LEL_TEXT_MAX));
    TEST_ASSERT_EQUAL_STRING("0.5 %LEL", text);
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_format_lel(1000, text, HMI_LEL_TEXT_MAX));
    TEST_ASSERT_EQUAL_STRING("100.0 %LEL", text);
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_format_lel(32767, text, HMI_LEL_TEXT_MAX));
    TEST_ASSERT_EQUAL_STRING("3276.7 %LEL", text);
}

/* @verifies REQ-HMI-001 */
static void test_hmi_format_lel_shows_negative_values_as_zero(void)
{
    char text[HMI_LEL_TEXT_MAX];

    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_format_lel(-5, text, HMI_LEL_TEXT_MAX));
    TEST_ASSERT_EQUAL_STRING("0.0 %LEL", text);
}

/* @verifies REQ-NFR-002 */
static void test_hmi_format_lel_rejects_small_buffer_and_null(void)
{
    char text[HMI_LEL_TEXT_MAX];

    TEST_ASSERT_EQUAL_INT(GG_ERR_RANGE, hmi_format_lel(1, text, HMI_LEL_TEXT_MAX - 1u));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, hmi_format_lel(1, NULL, HMI_LEL_TEXT_MAX));
}

/** @brief Press at 0 ms, release at @p release_ms; return the event of the release. */
static hmi_press_event_t press_for(uint32_t release_ms)
{
    hmi_press_t press;
    hmi_press_event_t event = HMI_PRESS_NONE;

    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_press_init(&press));
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_press_step(&press, true, 0u, &event));
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_press_step(&press, false, release_ms, &event));
    return event;
}

/* @verifies REQ-HMI-004 */
static void test_hmi_short_press_ends_at_999_ms(void)
{
    TEST_ASSERT_EQUAL_INT(HMI_PRESS_SHORT, press_for(HMI_SHORT_PRESS_MAX_MS - 1u));
    TEST_ASSERT_EQUAL_INT(HMI_PRESS_NONE, press_for(HMI_SHORT_PRESS_MAX_MS));
}

/* @verifies REQ-HMI-004 */
static void test_hmi_release_without_a_press_is_no_event(void)
{
    hmi_press_t press;
    hmi_press_event_t event = HMI_PRESS_SHORT;

    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_press_init(&press));
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_press_step(&press, false, 100u, &event));
    TEST_ASSERT_EQUAL_INT(HMI_PRESS_NONE, event);
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, hmi_press_step(&press, true, 0u, NULL));
}

/* @verifies REQ-HMI-004 */
static void test_hmi_short_press_across_the_clock_wrap_is_still_short(void)
{
    hmi_press_t press;
    hmi_press_event_t event = HMI_PRESS_NONE;

    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_press_init(&press));
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_press_step(&press, true, UINT32_MAX - 100u, &event));
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_press_step(&press, false, 300u, &event));
    TEST_ASSERT_EQUAL_INT(HMI_PRESS_SHORT, event);
}

/* @verifies REQ-HMI-004 */
static void test_hmi_ack_in_warmup_or_maintenance_silences_nothing_later(void)
{
    hmi_silence_t silence;
    bool on = false;

    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_silence_init(&silence));
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_acknowledge(DET_STATE_WARMUP, &silence, 0u));
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_acknowledge(DET_STATE_MAINTENANCE, &silence, 0u));
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_buzzer_on(DET_STATE_ALARM_1, &silence, 0u, &on));
    TEST_ASSERT_TRUE(on);
}

/* @verifies REQ-HMI-003 */
static void test_hmi_alarm_1_beep_lasts_exactly_half_a_second(void)
{
    hmi_silence_t silence;
    bool on = false;

    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_silence_init(&silence));
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_buzzer_on(DET_STATE_ALARM_1, &silence, HMI_ALARM_1_ON_MS - 1u, &on));
    TEST_ASSERT_TRUE(on);
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_buzzer_on(DET_STATE_ALARM_1, &silence, HMI_ALARM_1_PERIOD_MS - 1u, &on));
    TEST_ASSERT_FALSE(on);
}

/* @verifies REQ-HMI-003 */
static void test_hmi_alarm_2_beep_lasts_exactly_125_ms(void)
{
    hmi_silence_t silence;
    bool on = false;

    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_silence_init(&silence));
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_buzzer_on(DET_STATE_ALARM_2, &silence, HMI_ALARM_2_ON_MS - 1u, &on));
    TEST_ASSERT_TRUE(on);
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_buzzer_on(DET_STATE_ALARM_2, &silence, HMI_ALARM_2_PERIOD_MS - 1u, &on));
    TEST_ASSERT_FALSE(on);
}

/* @verifies REQ-HMI-003 */
static void test_hmi_fault_beep_lasts_exactly_200_ms(void)
{
    hmi_silence_t silence;
    bool on = false;

    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_silence_init(&silence));
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_buzzer_on(DET_STATE_FAULT, &silence, HMI_FAULT_BEEP_MS - 1u, &on));
    TEST_ASSERT_TRUE(on);
}

/* @verifies REQ-HMI-003 */
static void test_hmi_patterns_keep_their_rhythm_across_the_clock_wrap(void)
{
    hmi_silence_t silence;
    bool on = true;

    /* UINT32_MAX % 1000 = 295: on phase. After the wrap, 0 is on too, so the rhythm
     * goes on (one beep of 705 ms at worst, once every 49 days). */
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_silence_init(&silence));
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_buzzer_on(DET_STATE_ALARM_1, &silence, UINT32_MAX, &on));
    TEST_ASSERT_TRUE(on);
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_buzzer_on(DET_STATE_ALARM_1, &silence, 0u, &on));
    TEST_ASSERT_TRUE(on);
}

/* @verifies REQ-HMI-006 */
static void test_hmi_functional_test_runs_until_its_last_millisecond(void)
{
    hmi_test_out_t out;
    bool running = false;

    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_test_outputs(HMI_TEST_TOTAL_MS - 1u, &out, &running));
    TEST_ASSERT_TRUE(running);
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_test_outputs(UINT32_MAX, &out, &running));
    TEST_ASSERT_FALSE(running);
}

/* @verifies REQ-HMI-006 */
static void test_hmi_functional_test_releases_the_relay_for_exactly_one_step(void)
{
    hmi_test_out_t out;
    bool running = false;

    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_test_outputs((4u * HMI_TEST_STEP_MS) - 1u, &out, &running));
    TEST_ASSERT_TRUE(out.relay_energised);
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_test_outputs((5u * HMI_TEST_STEP_MS) - 1u, &out, &running));
    TEST_ASSERT_FALSE(out.relay_energised);
}

/* @verifies REQ-HMI-001 */
static void test_hmi_format_lel_small_values_and_whole_numbers(void)
{
    char text[HMI_LEL_TEXT_MAX];

    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_format_lel(0, text, HMI_LEL_TEXT_MAX));
    TEST_ASSERT_EQUAL_STRING("0.0 %LEL", text);
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_format_lel(9, text, HMI_LEL_TEXT_MAX));
    TEST_ASSERT_EQUAL_STRING("0.9 %LEL", text);
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_format_lel(10, text, HMI_LEL_TEXT_MAX));
    TEST_ASSERT_EQUAL_STRING("1.0 %LEL", text);
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_format_lel(INT16_MIN, text, HMI_LEL_TEXT_MAX));
    TEST_ASSERT_EQUAL_STRING("0.0 %LEL", text);
}

/** @brief Buzzer level in ALARM_2 at a time on the "on" phase of the 4 Hz pattern. */
static bool alarm_2_sounds_at(const hmi_silence_t *silence, uint32_t now_ms)
{
    bool on = false;

    TEST_ASSERT_EQUAL_UINT32(0u, now_ms % HMI_ALARM_2_PERIOD_MS);   /* "On" phase without a silence. */
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_buzzer_on(DET_STATE_ALARM_2, silence, now_ms, &on));
    return on;
}

/* @verifies REQ-HMI-004 */
static void test_hmi_second_press_in_alarm_2_does_not_extend_the_silence(void)
{
    hmi_silence_t silence;

    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_silence_init(&silence));
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_acknowledge(DET_STATE_ALARM_2, &silence, 0u));
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_acknowledge(DET_STATE_ALARM_2, &silence, 50000u));
    TEST_ASSERT_FALSE(alarm_2_sounds_at(&silence, 59750u));
    TEST_ASSERT_TRUE(alarm_2_sounds_at(&silence, 60000u));   /* 60 s after the first press. */
}

/* @verifies REQ-HMI-004 */
static void test_hmi_press_after_the_60_s_of_alarm_2_has_no_effect(void)
{
    hmi_silence_t silence;

    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_silence_init(&silence));
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_acknowledge(DET_STATE_ALARM_2, &silence, 0u));
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_acknowledge(DET_STATE_ALARM_2, &silence, 70000u));
    TEST_ASSERT_TRUE(alarm_2_sounds_at(&silence, 70000u));
    TEST_ASSERT_TRUE(alarm_2_sounds_at(&silence, 100000u));
}

/* @verifies REQ-HMI-004 */
static void test_hmi_pressing_every_ten_seconds_cannot_keep_alarm_2_quiet(void)
{
    hmi_silence_t silence;
    uint32_t t;

    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_silence_init(&silence));
    for (t = 0u; t <= 300000u; t += 10000u) {
        TEST_ASSERT_EQUAL_INT(GG_OK, hmi_acknowledge(DET_STATE_ALARM_2, &silence, t));
    }
    TEST_ASSERT_TRUE(alarm_2_sounds_at(&silence, 300000u));
}

/* @verifies REQ-HMI-004 */
static void test_hmi_last_press_before_the_end_of_the_60_s_adds_nothing(void)
{
    hmi_silence_t silence;

    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_silence_init(&silence));
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_acknowledge(DET_STATE_ALARM_2, &silence, 0u));
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_acknowledge(DET_STATE_ALARM_2, &silence, HMI_ALARM_2_SILENCE_MAX_MS - 1u));
    TEST_ASSERT_TRUE(alarm_2_sounds_at(&silence, HMI_ALARM_2_SILENCE_MAX_MS));
}

/* @verifies REQ-HMI-004 */
static void test_hmi_new_alarm_2_episode_gets_its_own_60_s(void)
{
    hmi_silence_t silence;

    /* The firmware resets the silence at every change of state: after an
     * ALARM_2 reset, a new ALARM_2 can be silenced for 60 s again. */
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_silence_init(&silence));
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_acknowledge(DET_STATE_ALARM_2, &silence, 0u));
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_silence_init(&silence));
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_acknowledge(DET_STATE_ALARM_2, &silence, 100000u));
    TEST_ASSERT_FALSE(alarm_2_sounds_at(&silence, 150000u));
    TEST_ASSERT_TRUE(alarm_2_sounds_at(&silence, 160000u));
}

/* @verifies REQ-HMI-004 */
static void test_hmi_alarm_1_silence_after_an_alarm_1_press_is_not_limited(void)
{
    hmi_silence_t silence;
    bool on = true;

    /* Pressing again in ALARM_1 keeps it quiet: only ALARM_2 has the 60 s limit. */
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_silence_init(&silence));
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_acknowledge(DET_STATE_ALARM_1, &silence, 0u));
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_acknowledge(DET_STATE_ALARM_1, &silence, 90000u));
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_buzzer_on(DET_STATE_ALARM_1, &silence, 600000u, &on));
    TEST_ASSERT_FALSE(on);
}

/* @verifies REQ-HMI-004 */
static void test_hmi_alarm_2_silence_budget_across_the_clock_wrap(void)
{
    hmi_silence_t silence;
    const uint32_t first = UINT32_MAX - 9999u;   /* 10 s before the wrap. */

    /* First press 10 s before the wrap, second press after it: the silence
     * still ends 60 s after the first press, at 49 999 ms after the wrap... */
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_silence_init(&silence));
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_acknowledge(DET_STATE_ALARM_2, &silence, first));
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_acknowledge(DET_STATE_ALARM_2, &silence, 20000u));
    TEST_ASSERT_FALSE(alarm_2_sounds_at(&silence, 49750u));
    /* ...so at 50 000 ms after the wrap (60 000 ms after the first press) it sounds. */
    TEST_ASSERT_TRUE(alarm_2_sounds_at(&silence, 50000u));
}

/* @verifies REQ-HMI-004 */
static void test_hmi_alarm_1_silence_does_not_use_up_the_alarm_2_budget(void)
{
    hmi_silence_t silence;

    /* Silenced in ALARM_1, then a press in ALARM_2 (even without a reset of the
     * silence in between): ALARM_2 gets its full 60 s from that press. */
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_silence_init(&silence));
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_acknowledge(DET_STATE_ALARM_1, &silence, 0u));
    TEST_ASSERT_EQUAL_INT(GG_OK, hmi_acknowledge(DET_STATE_ALARM_2, &silence, 100000u));
    TEST_ASSERT_FALSE(alarm_2_sounds_at(&silence, 159750u));
    TEST_ASSERT_TRUE(alarm_2_sounds_at(&silence, 160000u));
}

// DEV-03: Unity runner entry point, called by the test harness; MISRA 8.6 does not apply.
// cppcheck-suppress misra-c2012-8.6
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_hmi_press_shorter_than_one_second_is_a_short_press);
    RUN_TEST(test_hmi_press_between_one_and_three_seconds_is_neither);
    RUN_TEST(test_hmi_press_held_for_three_seconds_is_a_long_press_fired_once);
    RUN_TEST(test_hmi_press_rejects_null);
    RUN_TEST(test_hmi_ack_silences_alarm_1_buzzer_until_the_state_changes);
    RUN_TEST(test_hmi_alarm_2_silence_expires_after_60_seconds);
    RUN_TEST(test_hmi_silence_granted_in_alarm_1_does_not_carry_into_alarm_2);
    RUN_TEST(test_hmi_ack_outside_an_alarm_does_nothing);
    RUN_TEST(test_hmi_alarm_1_pattern_is_1_hz);
    RUN_TEST(test_hmi_alarm_2_pattern_is_4_hz);
    RUN_TEST(test_hmi_fault_beeps_once_every_five_seconds_and_cannot_be_silenced);
    RUN_TEST(test_hmi_quiet_states_keep_the_buzzer_off);
    RUN_TEST(test_hmi_functional_test_runs_buzzer_colours_and_relay_then_ends);
    RUN_TEST(test_hmi_null_arguments_are_rejected);
    RUN_TEST(test_hmi_format_lel_puts_the_tenth_after_the_point);
    RUN_TEST(test_hmi_format_lel_shows_negative_values_as_zero);
    RUN_TEST(test_hmi_format_lel_rejects_small_buffer_and_null);
    RUN_TEST(test_hmi_short_press_ends_at_999_ms);
    RUN_TEST(test_hmi_release_without_a_press_is_no_event);
    RUN_TEST(test_hmi_short_press_across_the_clock_wrap_is_still_short);
    RUN_TEST(test_hmi_ack_in_warmup_or_maintenance_silences_nothing_later);
    RUN_TEST(test_hmi_alarm_1_beep_lasts_exactly_half_a_second);
    RUN_TEST(test_hmi_alarm_2_beep_lasts_exactly_125_ms);
    RUN_TEST(test_hmi_fault_beep_lasts_exactly_200_ms);
    RUN_TEST(test_hmi_patterns_keep_their_rhythm_across_the_clock_wrap);
    RUN_TEST(test_hmi_functional_test_runs_until_its_last_millisecond);
    RUN_TEST(test_hmi_functional_test_releases_the_relay_for_exactly_one_step);
    RUN_TEST(test_hmi_format_lel_small_values_and_whole_numbers);
    RUN_TEST(test_hmi_second_press_in_alarm_2_does_not_extend_the_silence);
    RUN_TEST(test_hmi_press_after_the_60_s_of_alarm_2_has_no_effect);
    RUN_TEST(test_hmi_pressing_every_ten_seconds_cannot_keep_alarm_2_quiet);
    RUN_TEST(test_hmi_last_press_before_the_end_of_the_60_s_adds_nothing);
    RUN_TEST(test_hmi_new_alarm_2_episode_gets_its_own_60_s);
    RUN_TEST(test_hmi_alarm_1_silence_after_an_alarm_1_press_is_not_limited);
    RUN_TEST(test_hmi_alarm_2_silence_budget_across_the_clock_wrap);
    RUN_TEST(test_hmi_alarm_1_silence_does_not_use_up_the_alarm_2_budget);
    return UNITY_END();
}
