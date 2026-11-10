/* Unit tests for app/detector: the alarm state machine (REQ-ALM-*) and the relay (REQ-SAF-001). */
#include "unity.h"
#include "common/gg_types/gg_types.h"
#include "app/detector/detector.h"

/* DEV-03: Unity hooks, called by the framework; MISRA 8.6 and unusedFunction do not apply. */
// cppcheck-suppress misra-c2012-8.6
void setUp(void);
// cppcheck-suppress misra-c2012-8.6
void tearDown(void);

static detector_t det_test_state;

/* Defaults from REQ-ALM-001..003: AL1 10 %LEL, AL2 25 %LEL, hysteresis 2 %LEL. */
static const detector_thresholds_t det_test_thresholds = {
    DETECTOR_AL1_DEFAULT, DETECTOR_AL2_DEFAULT, DETECTOR_HYSTERESIS_DEFAULT
};

void setUp(void)
{
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_init(&det_test_state, &det_test_thresholds));
}

void tearDown(void) {}

/* Run one sample through the detector, with no fault and no reset request. */
static void step(gg_conc_t conc)
{
    detector_input_t in = {conc, false, false, false, false, DET_FAULT_NONE};

    TEST_ASSERT_EQUAL_INT(GG_OK, detector_step(&det_test_state, &in));
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

    TEST_ASSERT_EQUAL_INT(GG_OK, detector_state(&det_test_state, &state));
    return state;
}

/* Bring the detector out of warm-up, so the alarm logic is active. */
static void finish_warmup(void)
{
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_end_warmup(&det_test_state));
}

/* @verifies REQ-NFR-002 */
static void test_detector_init_rejects_null(void)
{
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, detector_init(NULL, &det_test_thresholds));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, detector_init(&det_test_state, NULL));
}

/* @verifies REQ-ALM-005 */
static void test_detector_rejects_threshold_ordering_and_limits(void)
{
    detector_thresholds_t equal = {250, 250, 20};
    detector_thresholds_t inverted = {300, 250, 20};
    detector_thresholds_t zero_al1 = {0, 250, 20};
    detector_thresholds_t above_max = {100, DETECTOR_AL2_MAX + 1, 20};

    TEST_ASSERT_EQUAL_INT(GG_ERR_RANGE, detector_init(&det_test_state, &equal));
    TEST_ASSERT_EQUAL_INT(GG_ERR_RANGE, detector_init(&det_test_state, &inverted));
    TEST_ASSERT_EQUAL_INT(GG_ERR_RANGE, detector_init(&det_test_state, &zero_al1));
    TEST_ASSERT_EQUAL_INT(GG_ERR_RANGE, detector_init(&det_test_state, &above_max));
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
    /* Clear level: AL1 - hysteresis (80 units with the defaults). */
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
        step(((i % 2u) == 0u) ? (gg_conc_t)85 : (gg_conc_t)105);
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
    detector_input_t reset = {DETECTOR_AL2_DEFAULT, false, true, false, false, DET_FAULT_NONE};

    finish_warmup();
    step_n(DETECTOR_AL2_DEFAULT, 6u);
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_step(&det_test_state, &reset));
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_2, state_now());
}

/* @verifies REQ-ALM-004 */
static void test_detector_alarm_2_reset_accepted_when_gas_gone(void)
{
    detector_input_t reset = {0, false, true, false, false, DET_FAULT_NONE};

    finish_warmup();
    step_n(DETECTOR_AL2_DEFAULT, 6u);
    step_n(0, 1u);
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_step(&det_test_state, &reset));
    TEST_ASSERT_EQUAL_INT(DET_STATE_NORMAL, state_now());
}

/* @verifies REQ-SAF-001 */
static void test_detector_relay_energised_only_in_healthy_normal_state(void)
{
    bool energised = false;

    finish_warmup();
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_relay_energised(&det_test_state, &energised));
    TEST_ASSERT_TRUE(energised);

    step_n(DETECTOR_AL1_DEFAULT, 3u);
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_relay_energised(&det_test_state, &energised));
    TEST_ASSERT_FALSE(energised);

    step_n(DETECTOR_AL2_DEFAULT, 3u);
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_relay_energised(&det_test_state, &energised));
    TEST_ASSERT_FALSE(energised);
}

/* @verifies REQ-SAF-007 */
static void test_detector_fault_latches_and_drops_relay(void)
{
    detector_input_t fault_in = {0, true, false, false, false, DET_FAULT_NONE};
    detector_input_t clear_in = {0, false, false, false, false, DET_FAULT_NONE};
    bool energised = true;

    finish_warmup();
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_step(&det_test_state, &fault_in));
    TEST_ASSERT_EQUAL_INT(DET_STATE_FAULT, state_now());
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_relay_energised(&det_test_state, &energised));
    TEST_ASSERT_FALSE(energised);

    /* The fault input going away does not clear a latched FAULT. */
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_step(&det_test_state, &clear_in));
    TEST_ASSERT_EQUAL_INT(DET_STATE_FAULT, state_now());
}

/* @verifies REQ-SAF-007 */
static void test_detector_fault_during_warmup_is_not_hidden(void)
{
    detector_input_t fault_in = {0, true, false, false, false, DET_FAULT_NONE};

    TEST_ASSERT_EQUAL_INT(GG_OK, detector_step(&det_test_state, &fault_in));
    TEST_ASSERT_EQUAL_INT(DET_STATE_FAULT, state_now());
}

/* @verifies REQ-NFR-002 */
static void test_detector_step_and_getters_reject_null(void)
{
    detector_input_t in = {0, false, false, false, false, DET_FAULT_NONE};
    detector_state_t state = DET_STATE_NORMAL;
    bool energised = false;

    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, detector_step(NULL, &in));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, detector_step(&det_test_state, NULL));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, detector_state(NULL, &state));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, detector_relay_energised(NULL, &energised));
}

/* @verifies REQ-ALM-005 */
static void test_detector_rejects_hysteresis_not_between_zero_and_al1(void)
{
    /* Clear level is al1 - hysteresis: it must stay above zero, so 0 <= hyst < al1. */
    detector_thresholds_t negative = {100, 250, -1};
    detector_thresholds_t equal_to_al1 = {100, 250, 100};
    detector_thresholds_t valid_zero = {100, 250, 0};
    detector_thresholds_t valid_below = {100, 250, 99};

    TEST_ASSERT_EQUAL_INT(GG_ERR_RANGE, detector_init(&det_test_state, &negative));
    TEST_ASSERT_EQUAL_INT(GG_ERR_RANGE, detector_init(&det_test_state, &equal_to_al1));
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_init(&det_test_state, &valid_zero));
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_init(&det_test_state, &valid_below));
}

/* @verifies REQ-SAF-007 */
static void test_detector_input_fault_records_its_reason(void)
{
    detector_input_t fault_in = {0, true, false, false, false, DET_FAULT_NONE};
    detector_fault_code_t code = DET_FAULT_NONE;

    finish_warmup();
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_step(&det_test_state, &fault_in));
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_fault_code(&det_test_state, &code));
    TEST_ASSERT_EQUAL_INT(DET_FAULT_INPUT, code);
}

/* An impossible state must go to FAULT with an internal reason (CLAUDE.md: default -> diag_raise). */
/* @verifies REQ-SAF-007 */
static void test_detector_impossible_state_raises_internal_fault(void)
{
    detector_input_t in = {0, false, false, false, false, DET_FAULT_NONE};
    detector_fault_code_t code = DET_FAULT_NONE;

    det_test_state.state = (detector_state_t)99;
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_step(&det_test_state, &in));
    TEST_ASSERT_EQUAL_INT(DET_STATE_FAULT, state_now());
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_fault_code(&det_test_state, &code));
    TEST_ASSERT_EQUAL_INT(DET_FAULT_INTERNAL, code);
}

/* @verifies REQ-SAF-007 */
static void test_detector_new_fault_is_reported_once_for_the_event_log(void)
{
    bool is_new = false;
    detector_fault_code_t code = DET_FAULT_NONE;

    detector_input_t in = {0, true, false, false, false, DET_FAULT_NONE};
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_step(&det_test_state, &in));

    TEST_ASSERT_EQUAL_INT(GG_OK, detector_take_new_fault(&det_test_state, &is_new, &code));
    TEST_ASSERT_TRUE(is_new);
    TEST_ASSERT_EQUAL_INT(DET_FAULT_INPUT, code);

    TEST_ASSERT_EQUAL_INT(GG_OK, detector_take_new_fault(&det_test_state, &is_new, &code));
    TEST_ASSERT_FALSE(is_new);
}

/* @verifies REQ-SAF-007 */
static void test_detector_repeated_fault_samples_are_not_reported_again(void)
{
    bool is_new = false;
    detector_fault_code_t code = DET_FAULT_NONE;
    detector_input_t in = {0, true, false, false, false, DET_FAULT_NONE};

    TEST_ASSERT_EQUAL_INT(GG_OK, detector_step(&det_test_state, &in));
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_take_new_fault(&det_test_state, &is_new, &code));
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_step(&det_test_state, &in));
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_take_new_fault(&det_test_state, &is_new, &code));
    TEST_ASSERT_FALSE(is_new);
}

/* @verifies REQ-SAF-007 */
static void test_detector_impossible_state_is_reported_as_a_new_internal_fault(void)
{
    bool is_new = false;
    detector_fault_code_t code = DET_FAULT_NONE;
    detector_input_t in = {0, false, false, false, false, DET_FAULT_NONE};

    det_test_state.state = (detector_state_t)99;
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_step(&det_test_state, &in));
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_take_new_fault(&det_test_state, &is_new, &code));
    TEST_ASSERT_TRUE(is_new);
    TEST_ASSERT_EQUAL_INT(DET_FAULT_INTERNAL, code);
}

/* @verifies REQ-SAF-007 */
static void test_detector_take_new_fault_rejects_null(void)
{
    bool is_new = false;
    detector_fault_code_t code = DET_FAULT_NONE;

    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, detector_take_new_fault(NULL, &is_new, &code));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, detector_take_new_fault(&det_test_state, NULL, &code));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, detector_take_new_fault(&det_test_state, &is_new, NULL));
}

/** @brief Build one sample with the maintenance and system inputs set. */
static detector_input_t input_with(gg_conc_t conc, bool maint_req, bool maint_exp, detector_fault_code_t sys)
{
    detector_input_t in;

    in.conc = conc;
    in.fault = false;
    in.reset_requested = false;
    in.maint_requested = maint_req;
    in.maint_expired = maint_exp;
    in.system_fault = sys;
    return in;
}

/* @verifies REQ-HMI-008 */
static void test_detector_touch_in_normal_enters_maintenance(void)
{
    detector_input_t in = input_with(0, true, false, DET_FAULT_NONE);

    finish_warmup();
    TEST_ASSERT_EQUAL_INT(DET_STATE_NORMAL, state_now());
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_step(&det_test_state, &in));
    TEST_ASSERT_EQUAL_INT(DET_STATE_MAINTENANCE, state_now());
}

/* @verifies REQ-HMI-008 */
static void test_detector_touch_is_ignored_during_warmup_and_alarms(void)
{
    detector_input_t in = input_with(0, true, false, DET_FAULT_NONE);

    TEST_ASSERT_EQUAL_INT(GG_OK, detector_step(&det_test_state, &in));
    TEST_ASSERT_EQUAL_INT(DET_STATE_WARMUP, state_now());
    finish_warmup();
    step_n(DETECTOR_AL1_DEFAULT, DETECTOR_ENTER_SAMPLES);
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_1, state_now());
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_step(&det_test_state, &in));
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_1, state_now());
}

/* @verifies REQ-HMI-009 */
static void test_detector_maintenance_keeps_relay_and_ignores_alarms(void)
{
    bool energised = false;
    detector_input_t enter = input_with(0, true, false, DET_FAULT_NONE);
    detector_input_t high = input_with(DETECTOR_AL2_DEFAULT, false, false, DET_FAULT_NONE);

    finish_warmup();
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_step(&det_test_state, &enter));
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_step(&det_test_state, &high));
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_step(&det_test_state, &high));
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_step(&det_test_state, &high));
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_step(&det_test_state, &high));
    TEST_ASSERT_EQUAL_INT(DET_STATE_MAINTENANCE, state_now());
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_relay_energised(&det_test_state, &energised));
    TEST_ASSERT_TRUE(energised);
}

/* @verifies REQ-HMI-010 */
static void test_detector_maintenance_ends_when_timer_expires(void)
{
    detector_input_t enter = input_with(0, true, false, DET_FAULT_NONE);
    detector_input_t expired = input_with(0, false, true, DET_FAULT_NONE);

    finish_warmup();
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_step(&det_test_state, &enter));
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_step(&det_test_state, &expired));
    TEST_ASSERT_EQUAL_INT(DET_STATE_NORMAL, state_now());
}

/* @verifies REQ-SAF-007 */
static void test_detector_fault_overrides_maintenance(void)
{
    bool energised = true;
    detector_input_t enter = input_with(0, true, false, DET_FAULT_NONE);
    detector_input_t broken = input_with(0, false, false, DET_FAULT_NONE);

    finish_warmup();
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_step(&det_test_state, &enter));
    broken.fault = true;
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_step(&det_test_state, &broken));
    TEST_ASSERT_EQUAL_INT(DET_STATE_FAULT, state_now());
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_relay_energised(&det_test_state, &energised));
    TEST_ASSERT_FALSE(energised);
}

/* @verifies REQ-SAF-008 */
static void test_detector_timing_fault_latches_with_its_reason(void)
{
    detector_fault_code_t code = DET_FAULT_NONE;
    detector_input_t in = input_with(0, false, false, DET_FAULT_TIMING);

    finish_warmup();
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_step(&det_test_state, &in));
    TEST_ASSERT_EQUAL_INT(DET_STATE_FAULT, state_now());
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_fault_code(&det_test_state, &code));
    TEST_ASSERT_EQUAL_INT(DET_FAULT_TIMING, code);
}

/* @verifies REQ-SAF-009 */
static void test_detector_stack_fault_latches_with_its_reason(void)
{
    detector_fault_code_t code = DET_FAULT_NONE;
    detector_input_t in = input_with(0, false, false, DET_FAULT_STACK);

    finish_warmup();
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_step(&det_test_state, &in));
    TEST_ASSERT_EQUAL_INT(DET_STATE_FAULT, state_now());
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_fault_code(&det_test_state, &code));
    TEST_ASSERT_EQUAL_INT(DET_FAULT_STACK, code);
}

// DEV-03: Unity runner entry point, called by the test harness; MISRA 8.6 does not apply.
// cppcheck-suppress misra-c2012-8.6
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_detector_init_rejects_null);
    RUN_TEST(test_detector_rejects_threshold_ordering_and_limits);
    RUN_TEST(test_detector_rejects_hysteresis_not_between_zero_and_al1);
    RUN_TEST(test_detector_input_fault_records_its_reason);
    RUN_TEST(test_detector_impossible_state_raises_internal_fault);
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
    RUN_TEST(test_detector_new_fault_is_reported_once_for_the_event_log);
    RUN_TEST(test_detector_repeated_fault_samples_are_not_reported_again);
    RUN_TEST(test_detector_impossible_state_is_reported_as_a_new_internal_fault);
    RUN_TEST(test_detector_take_new_fault_rejects_null);
    RUN_TEST(test_detector_touch_in_normal_enters_maintenance);
    RUN_TEST(test_detector_touch_is_ignored_during_warmup_and_alarms);
    RUN_TEST(test_detector_maintenance_keeps_relay_and_ignores_alarms);
    RUN_TEST(test_detector_maintenance_ends_when_timer_expires);
    RUN_TEST(test_detector_fault_overrides_maintenance);
    RUN_TEST(test_detector_timing_fault_latches_with_its_reason);
    RUN_TEST(test_detector_stack_fault_latches_with_its_reason);
    return UNITY_END();
}
