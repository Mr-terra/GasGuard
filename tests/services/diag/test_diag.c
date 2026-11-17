/* Unit tests for services/diag: open/short, stuck-at and bandgap diagnostics. */
#include "unity.h"
#include "common/gg_types/gg_types.h"
#include "hal/adc/adc.h"
#include "services/diag/diag.h"

/** @brief Time step of the stuck-window loops, in milliseconds. */
#define DIAG_TEST_STEP_MS 1000u

/* DEV-03: Unity hooks, called by the framework; MISRA 8.6 and unusedFunction do not apply. */
// cppcheck-suppress misra-c2012-8.6
void setUp(void);
// cppcheck-suppress misra-c2012-8.6
void tearDown(void);

static diag_t s_diag;

void setUp(void)
{
    TEST_ASSERT_EQUAL_INT(GG_OK, diag_init(&s_diag));
}

void tearDown(void) {}

/* Feed n range checks of the same raw value. Timing is not involved here. */
static void feed_range(uint16_t raw, uint16_t n)
{
    uint16_t i;

    for (i = 0u; i < n; i++) {
        TEST_ASSERT_EQUAL_INT(GG_OK, diag_check_range(&s_diag, raw));
    }
}

static gg_status_t check_stuck(uint16_t raw, uint32_t now_ms, bool in_warmup)
{
    diag_stuck_sample_t sample = {raw, now_ms, in_warmup};

    return diag_check_stuck(&s_diag, &sample);
}

static bool is_faulty(void)
{
    bool faulty = true;

    TEST_ASSERT_EQUAL_INT(GG_OK, diag_is_faulty(&s_diag, &faulty));
    return faulty;
}

/* @verifies REQ-SAF-002 */
static void test_diag_init_rejects_null(void)
{
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, diag_init(NULL));
}

/* @verifies REQ-SAF-002 */
static void test_diag_open_circuit_faulty_after_five_samples(void)
{
    feed_range(0u, DIAG_RANGE_SAMPLES - 1u);
    TEST_ASSERT_FALSE(is_faulty());
    feed_range(0u, 1u);
    TEST_ASSERT_TRUE(is_faulty());
}

/* @verifies REQ-SAF-002 */
static void test_diag_short_circuit_faulty_after_five_samples(void)
{
    feed_range(DIAG_SHORT_MIN, DIAG_RANGE_SAMPLES);
    TEST_ASSERT_TRUE(is_faulty());
}

/* @verifies REQ-SAF-002 */
static void test_diag_valid_sample_resets_the_count(void)
{
    feed_range(0u, DIAG_RANGE_SAMPLES - 1u);
    feed_range(300u, 1u);
    feed_range(0u, DIAG_RANGE_SAMPLES - 1u);
    TEST_ASSERT_FALSE(is_faulty());
}

/* @verifies REQ-SAF-002 */
static void test_diag_range_rejects_out_of_range_input(void)
{
    TEST_ASSERT_EQUAL_INT(GG_ERR_RANGE, diag_check_range(&s_diag, HAL_ADC_RAW_MAX + 1u));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, diag_is_faulty(NULL, NULL));
}

/* @verifies REQ-SAF-003 */
static void test_diag_stuck_value_faulty_after_ten_minutes(void)
{
    uint32_t t;

    for (t = 0u; t < DIAG_STUCK_MS; t += DIAG_TEST_STEP_MS) {
        TEST_ASSERT_EQUAL_INT(GG_OK, check_stuck(512u, t, false));
    }
    TEST_ASSERT_EQUAL_INT(GG_OK, check_stuck(512u, DIAG_STUCK_MS - 1u, false));
    TEST_ASSERT_FALSE(is_faulty());
    TEST_ASSERT_EQUAL_INT(GG_OK, check_stuck(512u, DIAG_STUCK_MS, false));
    TEST_ASSERT_TRUE(is_faulty());
}

/* @verifies REQ-SAF-003 */
static void test_diag_moving_value_is_not_stuck(void)
{
    uint32_t t;
    uint16_t raw = 300u;

    /* Two LSB of change every step: the window never settles, so no fault. */
    for (t = 0u; t <= (DIAG_STUCK_MS + 1000u); t += DIAG_TEST_STEP_MS) {
        raw = (uint16_t)((raw == 300u) ? 302u : 300u);
        TEST_ASSERT_EQUAL_INT(GG_OK, check_stuck(raw, t, false));
    }
    TEST_ASSERT_FALSE(is_faulty());
}

/* @verifies REQ-SAF-003 */
static void test_diag_stuck_check_is_inhibited_during_warmup(void)
{
    uint32_t t;

    for (t = 0u; t <= (DIAG_STUCK_MS + 1000u); t += DIAG_TEST_STEP_MS) {
        TEST_ASSERT_EQUAL_INT(GG_OK, check_stuck(512u, t, true));
    }
    TEST_ASSERT_FALSE(is_faulty());
}

/* @verifies REQ-SAF-004 */
static void test_diag_bandgap_nominal_is_ok(void)
{
    bool ok = false;

    TEST_ASSERT_EQUAL_INT(GG_OK, diag_bandgap_ok(DIAG_BANDGAP_NOMINAL, &ok));
    TEST_ASSERT_TRUE(ok);
}

/* @verifies REQ-SAF-004 */
static void test_diag_bandgap_within_ten_percent_is_ok(void)
{
    bool ok = false;

    TEST_ASSERT_EQUAL_INT(GG_OK, diag_bandgap_ok(205u, &ok));
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_INT(GG_OK, diag_bandgap_ok(245u, &ok));
    TEST_ASSERT_TRUE(ok);
}

/* @verifies REQ-SAF-004 */
static void test_diag_bandgap_outside_ten_percent_is_fault(void)
{
    bool ok = true;

    TEST_ASSERT_EQUAL_INT(GG_OK, diag_bandgap_ok(200u, &ok));
    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_EQUAL_INT(GG_OK, diag_bandgap_ok(250u, &ok));
    TEST_ASSERT_FALSE(ok);
}

/* @verifies REQ-SAF-004 */
static void test_diag_bandgap_rejects_null_and_out_of_range(void)
{
    bool ok = false;

    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, diag_bandgap_ok(DIAG_BANDGAP_NOMINAL, NULL));
    TEST_ASSERT_EQUAL_INT(GG_ERR_RANGE, diag_bandgap_ok(HAL_ADC_RAW_MAX + 1u, &ok));
}

/* @verifies REQ-SAF-002 */
static void test_diag_open_circuit_recovers_after_sixty_seconds_of_valid_samples(void)
{
    feed_range(0u, DIAG_RANGE_SAMPLES);
    TEST_ASSERT_TRUE(is_faulty());
    feed_range(500u, DIAG_RECOVER_SAMPLES - 1u);
    TEST_ASSERT_TRUE(is_faulty());
    feed_range(500u, 1u);
    TEST_ASSERT_FALSE(is_faulty());
}

/* @verifies REQ-SAF-002 */
static void test_diag_range_recovery_restarts_when_the_fault_comes_back(void)
{
    feed_range(0u, DIAG_RANGE_SAMPLES);
    feed_range(500u, DIAG_RECOVER_SAMPLES - 1u);
    feed_range(0u, 1u);
    feed_range(500u, DIAG_RECOVER_SAMPLES - 1u);
    TEST_ASSERT_TRUE(is_faulty());
}

/* @verifies REQ-SAF-003 */
static void test_diag_stuck_channel_recovers_when_the_value_moves(void)
{
    TEST_ASSERT_EQUAL_INT(GG_OK, check_stuck(500u, 0u, false));
    TEST_ASSERT_EQUAL_INT(GG_OK, check_stuck(500u, DIAG_STUCK_MS, false));
    TEST_ASSERT_TRUE(is_faulty());
    TEST_ASSERT_EQUAL_INT(GG_OK, check_stuck(510u, DIAG_STUCK_MS + 100u, false));
    TEST_ASSERT_FALSE(is_faulty());
}

/* @verifies REQ-SAF-003 */
static void test_diag_stuck_only_tells_a_stuck_fault_from_a_range_fault(void)
{
    bool only = true;

    TEST_ASSERT_EQUAL_INT(GG_OK, diag_stuck_only(&s_diag, &only));
    TEST_ASSERT_FALSE(only);   /* No fault at all. */
    TEST_ASSERT_EQUAL_INT(GG_OK, check_stuck(500u, 0u, false));
    TEST_ASSERT_EQUAL_INT(GG_OK, check_stuck(500u, DIAG_STUCK_MS, false));
    TEST_ASSERT_EQUAL_INT(GG_OK, diag_stuck_only(&s_diag, &only));
    TEST_ASSERT_TRUE(only);    /* Stuck, nothing else. */
    feed_range(0u, DIAG_RANGE_SAMPLES);
    TEST_ASSERT_EQUAL_INT(GG_OK, diag_stuck_only(&s_diag, &only));
    TEST_ASSERT_FALSE(only);   /* Stuck and open circuit: not only stuck. */
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, diag_stuck_only(NULL, &only));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, diag_stuck_only(&s_diag, NULL));
}

/* @verifies REQ-SAF-002 */
static void test_diag_open_circuit_limit_is_raw_five(void)
{
    feed_range(DIAG_OPEN_MAX, DIAG_RANGE_SAMPLES);
    TEST_ASSERT_TRUE(is_faulty());
    TEST_ASSERT_EQUAL_INT(GG_OK, diag_init(&s_diag));
    feed_range(DIAG_OPEN_MAX + 1u, DIAG_RANGE_SAMPLES);
    TEST_ASSERT_FALSE(is_faulty());
}

/* @verifies REQ-SAF-002 */
static void test_diag_short_circuit_limit_is_raw_1018(void)
{
    feed_range(DIAG_SHORT_MIN - 1u, DIAG_RANGE_SAMPLES);
    TEST_ASSERT_FALSE(is_faulty());
    feed_range(DIAG_SHORT_MIN, DIAG_RANGE_SAMPLES - 1u);
    TEST_ASSERT_FALSE(is_faulty());
    feed_range(HAL_ADC_RAW_MAX, 1u);
    TEST_ASSERT_TRUE(is_faulty());
}

/* @verifies REQ-SAF-002 */
static void test_diag_open_and_short_samples_count_together(void)
{
    /* A loose wire that jumps between the two rails is out of range at every
     * sample: it must not escape detection by never staying on one rail. */
    feed_range(0u, 1u);
    feed_range(HAL_ADC_RAW_MAX, 1u);
    feed_range(0u, 1u);
    feed_range(HAL_ADC_RAW_MAX, 1u);
    TEST_ASSERT_FALSE(is_faulty());
    feed_range(0u, 1u);
    TEST_ASSERT_TRUE(is_faulty());
}

/* @verifies REQ-SAF-002 */
static void test_diag_rejected_range_input_changes_nothing(void)
{
    feed_range(0u, DIAG_RANGE_SAMPLES - 1u);
    TEST_ASSERT_EQUAL_INT(GG_ERR_RANGE, diag_check_range(&s_diag, HAL_ADC_RAW_MAX + 1u));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, diag_check_range(NULL, 0u));
    TEST_ASSERT_FALSE(is_faulty());
    feed_range(0u, 1u);   /* The count was kept: the fifth valid open sample faults. */
    TEST_ASSERT_TRUE(is_faulty());
}

/* @verifies REQ-SAF-002 */
static void test_diag_valid_samples_in_a_healthy_channel_never_fault(void)
{
    feed_range(DIAG_OPEN_MAX + 1u, DIAG_RECOVER_SAMPLES);
    feed_range(DIAG_SHORT_MIN - 1u, DIAG_RECOVER_SAMPLES);
    TEST_ASSERT_FALSE(is_faulty());
}

/* @verifies REQ-SAF-003 */
static void test_diag_one_lsb_of_noise_does_not_hide_a_stuck_sensor(void)
{
    uint32_t t;
    uint16_t raw = 500u;

    /* The value toggles by exactly 1 LSB: that is "not more than 1 LSB", so the
     * sensor is still stuck after 10 minutes. */
    for (t = 0u; t < DIAG_STUCK_MS; t += DIAG_TEST_STEP_MS) {
        TEST_ASSERT_EQUAL_INT(GG_OK, check_stuck(raw, t, false));
        raw = (uint16_t)((raw == 500u) ? 501u : 500u);
    }
    TEST_ASSERT_FALSE(is_faulty());
    TEST_ASSERT_EQUAL_INT(GG_OK, check_stuck(raw, DIAG_STUCK_MS, false));
    TEST_ASSERT_TRUE(is_faulty());
}

/* @verifies REQ-SAF-003 */
static void test_diag_stuck_sensor_recovers_on_two_lsb_not_on_one(void)
{
    TEST_ASSERT_EQUAL_INT(GG_OK, check_stuck(500u, 0u, false));
    TEST_ASSERT_EQUAL_INT(GG_OK, check_stuck(500u, DIAG_STUCK_MS, false));
    TEST_ASSERT_TRUE(is_faulty());
    TEST_ASSERT_EQUAL_INT(GG_OK, check_stuck(501u, DIAG_STUCK_MS + 100u, false));
    TEST_ASSERT_TRUE(is_faulty());
    TEST_ASSERT_EQUAL_INT(GG_OK, check_stuck(502u, DIAG_STUCK_MS + 200u, false));
    TEST_ASSERT_FALSE(is_faulty());
}

/* @verifies REQ-SAF-003 */
static void test_diag_stuck_sensor_through_the_warmup_is_caught_ten_minutes_after_it(void)
{
    const uint32_t warmup_end_ms = 120000u;

    /* Stuck from the start: the warm-up does not count, so the fault comes 10
     * minutes after the warm-up, neither before nor never. */
    TEST_ASSERT_EQUAL_INT(GG_OK, check_stuck(500u, 0u, true));
    TEST_ASSERT_EQUAL_INT(GG_OK, check_stuck(500u, warmup_end_ms, true));
    TEST_ASSERT_EQUAL_INT(GG_OK, check_stuck(500u, warmup_end_ms + DIAG_STUCK_MS - 1u, false));
    TEST_ASSERT_FALSE(is_faulty());
    TEST_ASSERT_EQUAL_INT(GG_OK, check_stuck(500u, warmup_end_ms + DIAG_STUCK_MS, false));
    TEST_ASSERT_TRUE(is_faulty());
}

/* @verifies REQ-SAF-003 */
static void test_diag_stuck_window_survives_the_clock_wrap(void)
{
    const uint32_t start_ms = UINT32_MAX - 1000u;

    TEST_ASSERT_EQUAL_INT(GG_OK, check_stuck(500u, start_ms, false));
    TEST_ASSERT_EQUAL_INT(GG_OK, check_stuck(500u, start_ms + DIAG_STUCK_MS - 1u, false));
    TEST_ASSERT_FALSE(is_faulty());
    TEST_ASSERT_EQUAL_INT(GG_OK, check_stuck(500u, start_ms + DIAG_STUCK_MS, false));
    TEST_ASSERT_TRUE(is_faulty());
}

/* @verifies REQ-SAF-003 */
static void test_diag_stuck_check_rejects_bad_arguments(void)
{
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, diag_check_stuck(NULL, NULL));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, diag_check_stuck(&s_diag, NULL));
    TEST_ASSERT_EQUAL_INT(GG_ERR_RANGE, check_stuck(HAL_ADC_RAW_MAX + 1u, 0u, false));
    TEST_ASSERT_FALSE(is_faulty());
}

/* @verifies REQ-SAF-004 */
static void test_diag_bandgap_tolerance_ends_at_22_lsb(void)
{
    bool ok = false;

    /* 10 % of 225 is 22.5: 203 and 247 are in, 202 and 248 are out. */
    TEST_ASSERT_EQUAL_INT(GG_OK, diag_bandgap_ok(DIAG_BANDGAP_NOMINAL - 22u, &ok));
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_INT(GG_OK, diag_bandgap_ok(DIAG_BANDGAP_NOMINAL + 22u, &ok));
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_INT(GG_OK, diag_bandgap_ok(DIAG_BANDGAP_NOMINAL - 23u, &ok));
    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_EQUAL_INT(GG_OK, diag_bandgap_ok(DIAG_BANDGAP_NOMINAL + 23u, &ok));
    TEST_ASSERT_FALSE(ok);
}

/* @verifies REQ-SAF-004 */
static void test_diag_bandgap_at_the_adc_ends_is_a_fault(void)
{
    bool ok = true;

    /* A dead reference reads 0, a missing one can read full scale: both are faults. */
    TEST_ASSERT_EQUAL_INT(GG_OK, diag_bandgap_ok(0u, &ok));
    TEST_ASSERT_FALSE(ok);
    ok = true;
    TEST_ASSERT_EQUAL_INT(GG_OK, diag_bandgap_ok(HAL_ADC_RAW_MAX, &ok));
    TEST_ASSERT_FALSE(ok);
}

// cppcheck-suppress misra-c2012-8.6
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_diag_init_rejects_null);
    RUN_TEST(test_diag_open_circuit_faulty_after_five_samples);
    RUN_TEST(test_diag_short_circuit_faulty_after_five_samples);
    RUN_TEST(test_diag_valid_sample_resets_the_count);
    RUN_TEST(test_diag_range_rejects_out_of_range_input);
    RUN_TEST(test_diag_stuck_value_faulty_after_ten_minutes);
    RUN_TEST(test_diag_moving_value_is_not_stuck);
    RUN_TEST(test_diag_stuck_check_is_inhibited_during_warmup);
    RUN_TEST(test_diag_bandgap_nominal_is_ok);
    RUN_TEST(test_diag_bandgap_within_ten_percent_is_ok);
    RUN_TEST(test_diag_bandgap_outside_ten_percent_is_fault);
    RUN_TEST(test_diag_bandgap_rejects_null_and_out_of_range);
    RUN_TEST(test_diag_open_circuit_recovers_after_sixty_seconds_of_valid_samples);
    RUN_TEST(test_diag_range_recovery_restarts_when_the_fault_comes_back);
    RUN_TEST(test_diag_stuck_channel_recovers_when_the_value_moves);
    RUN_TEST(test_diag_stuck_only_tells_a_stuck_fault_from_a_range_fault);
    RUN_TEST(test_diag_open_circuit_limit_is_raw_five);
    RUN_TEST(test_diag_short_circuit_limit_is_raw_1018);
    RUN_TEST(test_diag_open_and_short_samples_count_together);
    RUN_TEST(test_diag_rejected_range_input_changes_nothing);
    RUN_TEST(test_diag_valid_samples_in_a_healthy_channel_never_fault);
    RUN_TEST(test_diag_one_lsb_of_noise_does_not_hide_a_stuck_sensor);
    RUN_TEST(test_diag_stuck_sensor_recovers_on_two_lsb_not_on_one);
    RUN_TEST(test_diag_stuck_sensor_through_the_warmup_is_caught_ten_minutes_after_it);
    RUN_TEST(test_diag_stuck_window_survives_the_clock_wrap);
    RUN_TEST(test_diag_stuck_check_rejects_bad_arguments);
    RUN_TEST(test_diag_bandgap_tolerance_ends_at_22_lsb);
    RUN_TEST(test_diag_bandgap_at_the_adc_ends_is_a_fault);
    return UNITY_END();
}
