/* Unit tests for app/detector: the alarm state machine (REQ-ALM-*) and the relay (REQ-SAF-001). */
#include "unity.h"
#include "common/gg_types.h"
#include "app/detector.h"

/* DEV-03: Unity hooks, called by the framework; MISRA 8.6 and unusedFunction do not apply. */
// cppcheck-suppress misra-c2012-8.6
void setUp(void);
// cppcheck-suppress misra-c2012-8.6
void tearDown(void);

static detector_t s_det;

/* Defaults from REQ-ALM-001..003: AL1 10 %LEL, AL2 25 %LEL, hysteresis 2 %LEL. */
static const detector_thresholds_t s_thresholds = {
    DETECTOR_AL1_DEFAULT, DETECTOR_AL2_DEFAULT, DETECTOR_HYSTERESIS_DEFAULT
};

// cppcheck-suppress unusedFunction
void setUp(void)
{
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_init(&s_det, &s_thresholds));
}

// cppcheck-suppress unusedFunction
void tearDown(void) {}

/* Run one sample through the detector, with no fault and no reset request. */
static void step(gg_conc_t conc)
{
    detector_input_t in = {conc, false, false};

    TEST_ASSERT_EQUAL_INT(GG_OK, detector_step(&s_det, &in));
}

static void step_n(gg_conc_t conc, uint8_t n)
{
    uint8_t i;

    for (i = 0u; i < n; i++) {
        step(conc);
    }
}

static detector_state_t state_now(void)
{
    detector_state_t state = DET_STATE_FAULT;

    TEST_ASSERT_EQUAL_INT(GG_OK, detector_state(&s_det, &state));
    return state;
}

/* Bring the detector out of warm-up, so the alarm logic is active. */
static void finish_warmup(void)
{
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_end_warmup(&s_det));
}

/* @verifies REQ-NFR-002 */
static void test_detector_init_rejects_null(void)
{
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, detector_init(NULL, &s_thresholds));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, detector_init(&s_det, NULL));
}

/* @verifies REQ-ALM-005 */
static void test_detector_rejects_threshold_ordering_and_limits(void)
{
    detector_thresholds_t equal = {250, 250, 20};
    detector_thresholds_t inverted = {300, 250, 20};
    detector_thresholds_t zero_al1 = {0, 250, 20};
    detector_thresholds_t above_max = {100, DETECTOR_AL2_MAX + 1, 20};

    TEST_ASSERT_EQUAL_INT(GG_ERR_RANGE, detector_init(&s_det, &equal));
    TEST_ASSERT_EQUAL_INT(GG_ERR_RANGE, detector_init(&s_det, &inverted));
    TEST_ASSERT_EQUAL_INT(GG_ERR_RANGE, detector_init(&s_det, &zero_al1));
    TEST_ASSERT_EQUAL_INT(GG_ERR_RANGE, detector_init(&s_det, &above_max));
}

/* @verifies REQ-ALM-006 */
static void test_detector_starts_in_warmup_with_alarms_inhibited(void)
{
    TEST_ASSERT_EQUAL_INT(DET_STATE_WARMUP, state_now());
    step_n(500, 10u);
    TEST_ASSERT_EQUAL_INT(DET_STATE_WARMUP, state_now());
}

/* @verifies REQ-ALM-006 */
static void test_detector_warmup_ends_in_normal(void)
{
    finish_warmup();
    TEST_ASSERT_EQUAL_INT(DET_STATE_NORMAL, state_now());
}

/* @verifies REQ-ALM-001 */
static void test_detector_alarm_1_after_three_samples_at_threshold(void)
{
    finish_warmup();
    step_n(DETECTOR_AL1_DEFAULT, 2u);
    TEST_ASSERT_EQUAL_INT(DET_STATE_NORMAL, state_now());
    step(DETECTOR_AL1_DEFAULT);
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_1, state_now());
}

/* @verifies REQ-ALM-001 */
static void test_detector_single_spike_does_not_alarm(void)
{
    finish_warmup();
    step(500);
    step(0);
    step(0);
    TEST_ASSERT_EQUAL_INT(DET_STATE_NORMAL, state_now());
}

/* @verifies REQ-ALM-002 */
static void test_detector_alarm_2_after_three_samples_at_threshold(void)
{
    finish_warmup();
    step_n(DETECTOR_AL1_DEFAULT, 3u);
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_1, state_now());
    step_n(DETECTOR_AL2_DEFAULT, 3u);
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_2, state_now());
}

/* @verifies REQ-ALM-003 */
static void test_detector_alarm_1_clears_after_ten_samples_below_hysteresis(void)
{
    /* Clear level is AL1 - hysteresis = 80 units. */
    finish_warmup();
    step_n(DETECTOR_AL1_DEFAULT, 3u);
    step_n(0, 9u);
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_1, state_now());
    step(0);
    TEST_ASSERT_EQUAL_INT(DET_STATE_NORMAL, state_now());
}

/* @verifies REQ-ALM-003 */
static void test_detector_clear_count_restarts_when_gas_returns(void)
{
    finish_warmup();
    step_n(DETECTOR_AL1_DEFAULT, 3u);
    step_n(0, 9u);
    step(DETECTOR_AL1_DEFAULT);
    step_n(0, 9u);
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_1, state_now());
}

/* @verifies REQ-ALM-003 */
static void test_detector_hysteresis_band_does_not_chatter(void)
{
    /* Oscillation 85..105 stays inside the band: ALARM_1 must hold (SC-04). */
    uint8_t i;

    finish_warmup();
    step_n(DETECTOR_AL1_DEFAULT, 3u);
    for (i = 0u; i < 40u; i++) {
        step((i % 2u == 0u) ? 85 : 105);
    }
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_1, state_now());
}

/* @verifies REQ-ALM-004 */
static void test_detector_alarm_2_is_latched_when_gas_drops(void)
{
    finish_warmup();
    step_n(DETECTOR_AL2_DEFAULT, 6u);
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_2, state_now());
    step_n(0, 50u);
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_2, state_now());
}

/* @verifies REQ-ALM-004 */
static void test_detector_alarm_2_reset_refused_while_gas_present(void)
{
    detector_input_t reset = {DETECTOR_AL2_DEFAULT, false, true};

    finish_warmup();
    step_n(DETECTOR_AL2_DEFAULT, 6u);
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_step(&s_det, &reset));
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_2, state_now());
}

/* @verifies REQ-ALM-004 */
static void test_detector_alarm_2_reset_accepted_when_gas_gone(void)
{
    detector_input_t reset = {0, false, true};

    finish_warmup();
    step_n(DETECTOR_AL2_DEFAULT, 6u);
    step_n(0, 1u);
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_step(&s_det, &reset));
    TEST_ASSERT_EQUAL_INT(DET_STATE_NORMAL, state_now());
}

/* @verifies REQ-SAF-001 */
static void test_detector_relay_energised_only_in_healthy_normal_state(void)
{
    bool energised = false;

    finish_warmup();
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_relay_energised(&s_det, &energised));
    TEST_ASSERT_TRUE(energised);

    step_n(DETECTOR_AL1_DEFAULT, 3u);
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_relay_energised(&s_det, &energised));
    TEST_ASSERT_FALSE(energised);

    step_n(DETECTOR_AL2_DEFAULT, 3u);
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_relay_energised(&s_det, &energised));
    TEST_ASSERT_FALSE(energised);
}

/* @verifies REQ-SAF-007 */
static void test_detector_fault_latches_and_drops_relay(void)
{
    detector_input_t fault_in = {0, true, false};
    detector_input_t clear_in = {0, false, false};
    bool energised = true;

    finish_warmup();
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_step(&s_det, &fault_in));
    TEST_ASSERT_EQUAL_INT(DET_STATE_FAULT, state_now());
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_relay_energised(&s_det, &energised));
    TEST_ASSERT_FALSE(energised);

    /* The fault input going away does not clear a latched FAULT. */
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_step(&s_det, &clear_in));
    TEST_ASSERT_EQUAL_INT(DET_STATE_FAULT, state_now());
}

/* @verifies REQ-SAF-007 */
static void test_detector_fault_during_warmup_is_not_hidden(void)
{
    detector_input_t fault_in = {0, true, false};

    TEST_ASSERT_EQUAL_INT(GG_OK, detector_step(&s_det, &fault_in));
    TEST_ASSERT_EQUAL_INT(DET_STATE_FAULT, state_now());
}

/* @verifies REQ-NFR-002 */
static void test_detector_step_and_getters_reject_null(void)
{
    detector_input_t in = {0, false, false};
    detector_state_t state = DET_STATE_NORMAL;
    bool energised = false;

    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, detector_step(NULL, &in));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, detector_step(&s_det, NULL));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, detector_state(NULL, &state));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, detector_relay_energised(NULL, &energised));
}

// cppcheck-suppress misra-c2012-8.6
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_detector_init_rejects_null);
    RUN_TEST(test_detector_rejects_threshold_ordering_and_limits);
    RUN_TEST(test_detector_starts_in_warmup_with_alarms_inhibited);
    RUN_TEST(test_detector_warmup_ends_in_normal);
    RUN_TEST(test_detector_alarm_1_after_three_samples_at_threshold);
    RUN_TEST(test_detector_single_spike_does_not_alarm);
    RUN_TEST(test_detector_alarm_2_after_three_samples_at_threshold);
    RUN_TEST(test_detector_alarm_1_clears_after_ten_samples_below_hysteresis);
    RUN_TEST(test_detector_clear_count_restarts_when_gas_returns);
    RUN_TEST(test_detector_hysteresis_band_does_not_chatter);
    RUN_TEST(test_detector_alarm_2_is_latched_when_gas_drops);
    RUN_TEST(test_detector_alarm_2_reset_refused_while_gas_present);
    RUN_TEST(test_detector_alarm_2_reset_accepted_when_gas_gone);
    RUN_TEST(test_detector_relay_energised_only_in_healthy_normal_state);
    RUN_TEST(test_detector_fault_latches_and_drops_relay);
    RUN_TEST(test_detector_fault_during_warmup_is_not_hidden);
    RUN_TEST(test_detector_step_and_getters_reject_null);
    return UNITY_END();
}
