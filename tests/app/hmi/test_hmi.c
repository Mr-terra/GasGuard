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
    return UNITY_END();
}
