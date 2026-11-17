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
    detector_input_t in = {conc, false, false, false, false, DET_FAULT_NONE, false};

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
    detector_input_t reset = {DETECTOR_AL2_DEFAULT, false, true, false, false, DET_FAULT_NONE, false};

    finish_warmup();
    step_n(DETECTOR_AL2_DEFAULT, 6u);
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_step(&det_test_state, &reset));
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_2, state_now());
}

/* @verifies REQ-ALM-004 */
static void test_detector_alarm_2_reset_accepted_when_gas_gone(void)
{
    detector_input_t reset = {0, false, true, false, false, DET_FAULT_NONE, false};

    finish_warmup();
    step_n(DETECTOR_AL2_DEFAULT, 6u);
    step_n(0, 1u);
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_step(&det_test_state, &reset));
    TEST_ASSERT_EQUAL_INT(DET_STATE_NORMAL, state_now());
}

/* @verifies REQ-SAF-001 */
static void test_detector_relay_released_on_alarm(void)
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
    detector_input_t fault_in = {0, true, false, false, false, DET_FAULT_NONE, false};
    detector_input_t clear_in = {0, false, false, false, false, DET_FAULT_NONE, false};
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
    detector_input_t fault_in = {0, true, false, false, false, DET_FAULT_NONE, false};

    TEST_ASSERT_EQUAL_INT(GG_OK, detector_step(&det_test_state, &fault_in));
    TEST_ASSERT_EQUAL_INT(DET_STATE_FAULT, state_now());
}

/* @verifies REQ-NFR-002 */
static void test_detector_step_and_getters_reject_null(void)
{
    detector_input_t in = {0, false, false, false, false, DET_FAULT_NONE, false};
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
    detector_input_t fault_in = {0, true, false, false, false, DET_FAULT_NONE, false};
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
    detector_input_t in = {0, false, false, false, false, DET_FAULT_NONE, false};
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

    detector_input_t in = {0, true, false, false, false, DET_FAULT_NONE, false};
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
    detector_input_t in = {0, true, false, false, false, DET_FAULT_NONE, false};

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
    detector_input_t in = {0, false, false, false, false, DET_FAULT_NONE, false};

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
    in.fault_stuck = false;
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

/** @brief One sample where every channel is out only because it is stuck (REQ-SAF-003). */
static void step_all_stuck(void)
{
    detector_input_t in = {0, false, false, false, false, DET_FAULT_NONE, true};

    TEST_ASSERT_EQUAL_INT(GG_OK, detector_step(&det_test_state, &in));
}

/** @brief One sample with every channel faulty for a real reason (open, short, ADC). */
static void step_input_fault(void)
{
    detector_input_t in = {0, true, false, false, false, DET_FAULT_NONE, false};

    TEST_ASSERT_EQUAL_INT(GG_OK, detector_step(&det_test_state, &in));
}

/* @verifies REQ-SAF-007 */
static void test_detector_stuck_only_fault_ends_by_itself(void)
{
    detector_fault_code_t code = DET_FAULT_NONE;

    finish_warmup();
    step_all_stuck();
    TEST_ASSERT_EQUAL_INT(DET_STATE_FAULT, state_now());
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_fault_code(&det_test_state, &code));
    TEST_ASSERT_EQUAL_INT(DET_FAULT_STUCK, code);
    /* A channel moves again: back to NORMAL without a restart. */
    step(0);
    TEST_ASSERT_EQUAL_INT(DET_STATE_NORMAL, state_now());
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_fault_code(&det_test_state, &code));
    TEST_ASSERT_EQUAL_INT(DET_FAULT_NONE, code);
}

/* @verifies REQ-SAF-007 REQ-ALM-004 */
static void test_detector_stuck_only_fault_keeps_the_alarm_2_latch(void)
{
    finish_warmup();
    /* NORMAL to ALARM_1, then ALARM_2: three samples each. */
    step_n(DETECTOR_AL2_DEFAULT, 6u);
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_2, state_now());
    step_all_stuck();
    TEST_ASSERT_EQUAL_INT(DET_STATE_FAULT, state_now());
    /* The gas is gone, the channels are back: ALARM_2 is still latched (no operator reset). */
    step(0);
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_2, state_now());
}

/* @verifies REQ-SAF-007 */
static void test_detector_real_fault_during_a_stuck_fault_latches(void)
{
    detector_fault_code_t code = DET_FAULT_NONE;

    finish_warmup();
    step_all_stuck();
    step_input_fault();
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_fault_code(&det_test_state, &code));
    TEST_ASSERT_EQUAL_INT(DET_FAULT_INPUT, code);
    step(0);
    TEST_ASSERT_EQUAL_INT(DET_STATE_FAULT, state_now());
}

/* @verifies REQ-SAF-007 */
static void test_detector_stuck_after_a_real_fault_keeps_the_latch(void)
{
    detector_fault_code_t code = DET_FAULT_NONE;

    finish_warmup();
    step_input_fault();
    step_all_stuck();
    step(0);
    TEST_ASSERT_EQUAL_INT(DET_STATE_FAULT, state_now());
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_fault_code(&det_test_state, &code));
    TEST_ASSERT_EQUAL_INT(DET_FAULT_INPUT, code);
}

/** @brief One stuck-only sample with a given concentration and maintenance timer state. */
static void step_stuck_with(gg_conc_t conc, bool maint_expired)
{
    detector_input_t in = {conc, false, false, false, maint_expired, DET_FAULT_NONE, true};

    TEST_ASSERT_EQUAL_INT(GG_OK, detector_step(&det_test_state, &in));
}

/* @verifies REQ-SAF-007 */
static void test_detector_stuck_fault_ended_by_gas_goes_straight_to_the_alarm(void)
{
    finish_warmup();
    step_all_stuck();
    /* The channel moved because gas came: ALARM_1 at once, so the relay stays off. */
    step(DETECTOR_AL1_DEFAULT);
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_1, state_now());
    step_all_stuck();
    step(DETECTOR_AL2_DEFAULT);
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_2, state_now());
}

/* @verifies REQ-SAF-007 REQ-ALM-003 */
static void test_detector_stuck_fault_returns_to_alarm_1(void)
{
    finish_warmup();
    step_n(DETECTOR_AL1_DEFAULT, 3u);
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_1, state_now());
    step_all_stuck();
    step(0);
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_1, state_now());
}

/* @verifies REQ-SAF-007 REQ-HMI-010 */
static void test_detector_stuck_fault_in_maintenance_honours_the_timer(void)
{
    detector_input_t touch = {0, false, false, true, false, DET_FAULT_NONE, false};

    finish_warmup();
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_step(&det_test_state, &touch));
    TEST_ASSERT_EQUAL_INT(DET_STATE_MAINTENANCE, state_now());
    step_stuck_with(0, false);
    TEST_ASSERT_EQUAL_INT(DET_STATE_FAULT, state_now());
    /* Still within the 10 minutes: back to MAINTENANCE. */
    step(0);
    TEST_ASSERT_EQUAL_INT(DET_STATE_MAINTENANCE, state_now());
    step_stuck_with(0, true);
    {
        detector_input_t expired = {0, false, false, false, true, DET_FAULT_NONE, false};

        /* The 10 minutes ran out during the FAULT: straight to NORMAL. */
        TEST_ASSERT_EQUAL_INT(GG_OK, detector_step(&det_test_state, &expired));
    }
    TEST_ASSERT_EQUAL_INT(DET_STATE_NORMAL, state_now());
}

/* @verifies REQ-SAF-007 REQ-SAF-008 */
static void test_detector_system_fault_during_a_stuck_fault_is_reported_and_latched(void)
{
    detector_input_t timing = {0, false, false, false, false, DET_FAULT_TIMING, false};
    bool is_new = false;
    detector_fault_code_t code = DET_FAULT_NONE;

    finish_warmup();
    step_all_stuck();
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_take_new_fault(&det_test_state, &is_new, &code));
    TEST_ASSERT_EQUAL_INT(DET_FAULT_STUCK, code);
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_step(&det_test_state, &timing));
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_take_new_fault(&det_test_state, &is_new, &code));
    TEST_ASSERT_TRUE(is_new);
    TEST_ASSERT_EQUAL_INT(DET_FAULT_TIMING, code);
    step(0);
    TEST_ASSERT_EQUAL_INT(DET_STATE_FAULT, state_now());
}

/* @verifies REQ-SAF-007 REQ-ALM-006 */
static void test_detector_stuck_fault_from_warmup_ends_in_normal_once_warmup_is_over(void)
{
    step_all_stuck();
    TEST_ASSERT_EQUAL_INT(DET_STATE_FAULT, state_now());
    finish_warmup();
    step(0);
    TEST_ASSERT_EQUAL_INT(DET_STATE_NORMAL, state_now());
}

/* @verifies REQ-ALM-001 */
static void test_detector_gas_one_unit_below_al1_never_alarms(void)
{
    gg_conc_t below = DETECTOR_AL1_DEFAULT;

    below = (gg_conc_t)(below - 1);
    finish_warmup();
    step_n(below, 200u);
    TEST_ASSERT_EQUAL_INT(DET_STATE_NORMAL, state_now());
}

/* @verifies REQ-ALM-001 */
static void test_detector_two_samples_then_a_dip_restart_the_count(void)
{
    gg_conc_t below = DETECTOR_AL1_DEFAULT;

    below = (gg_conc_t)(below - 1);
    finish_warmup();
    step_n(DETECTOR_AL1_DEFAULT, 2u);
    step(below);
    step_n(DETECTOR_AL1_DEFAULT, 2u);
    TEST_ASSERT_EQUAL_INT(DET_STATE_NORMAL, state_now());
    step(DETECTOR_AL1_DEFAULT);   /* Third in a row since the dip: a real, lasting leak is not missed. */
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_1, state_now());
}

/* @verifies REQ-ALM-001 REQ-ALM-002 */
static void test_detector_largest_concentration_alarms(void)
{
    finish_warmup();
    step_n(INT16_MAX, DETECTOR_ENTER_SAMPLES);
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_2, state_now());   /* Above AL2 too: ALARM_2 at once. */
}

/* @verifies REQ-ALM-002 */
static void test_detector_gas_one_unit_below_al2_stays_in_alarm_1(void)
{
    finish_warmup();
    step_n((gg_conc_t)(DETECTOR_AL2_DEFAULT - 1), 200u);
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_1, state_now());
}

/* @verifies REQ-ALM-002 */
static void test_detector_two_samples_at_al2_do_not_give_alarm_2(void)
{
    finish_warmup();
    step_n(DETECTOR_AL1_DEFAULT, DETECTOR_ENTER_SAMPLES);
    step_n(DETECTOR_AL2_DEFAULT, 2u);
    step(DETECTOR_AL1_DEFAULT);
    step_n(DETECTOR_AL2_DEFAULT, 2u);
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_1, state_now());
}

/* @verifies REQ-ALM-003 */
static void test_detector_clear_level_itself_does_not_count_as_below(void)
{
    gg_conc_t clear = DETECTOR_AL1_DEFAULT;

    clear = (gg_conc_t)(clear - DETECTOR_HYSTERESIS_DEFAULT);   /* 80: the clear level. */
    finish_warmup();
    step_n(DETECTOR_AL1_DEFAULT, DETECTOR_ENTER_SAMPLES);
    step_n(clear, 50u);   /* "Below" the clear level is strict: 80 holds the alarm. */
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_1, state_now());
    step_n((gg_conc_t)(clear - 1), DETECTOR_CLEAR_SAMPLES);
    TEST_ASSERT_EQUAL_INT(DET_STATE_NORMAL, state_now());
}

/* @verifies REQ-ALM-003 */
static void test_detector_alarm_1_does_not_clear_while_gas_is_still_above_al1(void)
{
    finish_warmup();
    step_n(DETECTOR_AL1_DEFAULT, 255u);
    step_n(DETECTOR_AL1_DEFAULT, 10u);   /* Saturated counters must not wrap and clear it. */
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_1, state_now());
}

/* @verifies REQ-ALM-004 */
static void test_detector_alarm_2_reset_at_the_clear_level_is_refused_one_below_accepted(void)
{
    gg_conc_t clear = DETECTOR_AL1_DEFAULT;
    detector_input_t reset = {0, false, true, false, false, DET_FAULT_NONE, false};

    clear = (gg_conc_t)(clear - DETECTOR_HYSTERESIS_DEFAULT);   /* 80: the clear level. */
    reset.conc = clear;
    finish_warmup();
    step_n(DETECTOR_AL2_DEFAULT, 6u);
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_step(&det_test_state, &reset));
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_2, state_now());
    reset.conc = (gg_conc_t)(clear - 1);
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_step(&det_test_state, &reset));
    TEST_ASSERT_EQUAL_INT(DET_STATE_NORMAL, state_now());
}

/* @verifies REQ-ALM-004 */
static void test_detector_gas_back_after_an_alarm_2_reset_alarms_again(void)
{
    detector_input_t reset = {0, false, true, false, false, DET_FAULT_NONE, false};

    finish_warmup();
    step_n(DETECTOR_AL2_DEFAULT, 6u);
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_step(&det_test_state, &reset));
    TEST_ASSERT_EQUAL_INT(DET_STATE_NORMAL, state_now());
    step_n(DETECTOR_AL2_DEFAULT, 6u);
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_2, state_now());
}

/* @verifies REQ-ALM-004 */
static void test_detector_reset_request_outside_alarm_2_changes_nothing(void)
{
    detector_input_t reset = {0, false, true, false, false, DET_FAULT_NONE, false};

    finish_warmup();
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_step(&det_test_state, &reset));
    TEST_ASSERT_EQUAL_INT(DET_STATE_NORMAL, state_now());
    step_n(DETECTOR_AL1_DEFAULT, DETECTOR_ENTER_SAMPLES);
    reset.conc = DETECTOR_AL1_DEFAULT;
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_step(&det_test_state, &reset));
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_1, state_now());
}

/* @verifies REQ-ALM-005 */
static void test_detector_accepts_the_widest_valid_thresholds(void)
{
    const detector_thresholds_t lowest = {1, 2, 0};
    const detector_thresholds_t highest = {DETECTOR_AL2_MAX - 1, DETECTOR_AL2_MAX, DETECTOR_AL2_MAX - 2};

    TEST_ASSERT_EQUAL_INT(GG_OK, detector_init(&det_test_state, &lowest));
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_init(&det_test_state, &highest));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, detector_thresholds_check(NULL));
}

/* @verifies REQ-ALM-005 */
static void test_detector_rejected_thresholds_keep_the_ones_in_use(void)
{
    const detector_thresholds_t bad = {0, 250, 20};

    TEST_ASSERT_EQUAL_INT(GG_ERR_RANGE, detector_init(&det_test_state, &bad));
    finish_warmup();
    step_n(DETECTOR_AL1_DEFAULT, DETECTOR_ENTER_SAMPLES);
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_1, state_now());
}

/* @verifies REQ-ALM-006 */
static void test_detector_gas_present_at_the_end_of_warmup_alarms_three_samples_later(void)
{
    step_n(DETECTOR_AL1_DEFAULT, 50u);   /* Not counted during the warm-up. */
    finish_warmup();
    step_n(DETECTOR_AL1_DEFAULT, DETECTOR_ENTER_SAMPLES - 1u);
    TEST_ASSERT_EQUAL_INT(DET_STATE_NORMAL, state_now());
    step(DETECTOR_AL1_DEFAULT);
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_1, state_now());
}

/* @verifies REQ-ALM-006 */
static void test_detector_end_warmup_rejects_null_and_acts_once(void)
{
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, detector_end_warmup(NULL));
    finish_warmup();
    step_n(DETECTOR_AL1_DEFAULT, DETECTOR_ENTER_SAMPLES);
    finish_warmup();   /* A second call must not pull an alarm back to NORMAL. */
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_1, state_now());
}

/* @verifies REQ-SAF-001 */
static void test_detector_relay_is_energised_during_warmup(void)
{
    bool energised = false;

    TEST_ASSERT_EQUAL_INT(GG_OK, detector_relay_energised(&det_test_state, &energised));
    TEST_ASSERT_TRUE(energised);
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, detector_relay_energised(NULL, &energised));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, detector_relay_energised(&det_test_state, NULL));
}

/* @verifies REQ-SAF-001 */
static void test_detector_relay_is_energised_again_after_alarm_1_clears(void)
{
    bool energised = false;

    finish_warmup();
    step_n(DETECTOR_AL1_DEFAULT, DETECTOR_ENTER_SAMPLES);
    step_n(0, DETECTOR_CLEAR_SAMPLES);
    TEST_ASSERT_EQUAL_INT(DET_STATE_NORMAL, state_now());
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_relay_energised(&det_test_state, &energised));
    TEST_ASSERT_TRUE(energised);
}

/* @verifies REQ-HMI-008 */
static void test_detector_touch_is_ignored_in_alarm_2_and_fault(void)
{
    detector_input_t touch = input_with(0, true, false, DET_FAULT_NONE);
    detector_input_t broken = input_with(0, false, false, DET_FAULT_NONE);

    finish_warmup();
    step_n(DETECTOR_AL2_DEFAULT, 6u);
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_step(&det_test_state, &touch));
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_2, state_now());
    broken.fault = true;
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_step(&det_test_state, &broken));
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_step(&det_test_state, &touch));
    TEST_ASSERT_EQUAL_INT(DET_STATE_FAULT, state_now());
}

/* @verifies REQ-HMI-009 */
static void test_detector_gas_after_maintenance_alarms_three_samples_after_its_end(void)
{
    detector_input_t enter = input_with(0, true, false, DET_FAULT_NONE);
    detector_input_t gas = input_with(DETECTOR_AL2_DEFAULT, false, false, DET_FAULT_NONE);
    detector_input_t expired = input_with(DETECTOR_AL2_DEFAULT, false, true, DET_FAULT_NONE);
    uint8_t i;

    finish_warmup();
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_step(&det_test_state, &enter));
    for (i = 0u; i < 20u; i++) {
        TEST_ASSERT_EQUAL_INT(GG_OK, detector_step(&det_test_state, &gas));
    }
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_step(&det_test_state, &expired));
    TEST_ASSERT_EQUAL_INT(DET_STATE_NORMAL, state_now());
    /* The gas still there after the test must not be forgotten. */
    step_n(DETECTOR_AL2_DEFAULT, DETECTOR_ENTER_SAMPLES);
    TEST_ASSERT_TRUE(state_now() != DET_STATE_NORMAL);
}

/* @verifies REQ-HMI-010 */
static void test_detector_maintenance_holds_until_the_timer_expires(void)
{
    detector_input_t enter = input_with(0, true, false, DET_FAULT_NONE);
    detector_input_t running = input_with(0, false, false, DET_FAULT_NONE);
    uint16_t i;

    finish_warmup();
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_step(&det_test_state, &enter));
    for (i = 0u; i < 1000u; i++) {
        TEST_ASSERT_EQUAL_INT(GG_OK, detector_step(&det_test_state, &running));
    }
    TEST_ASSERT_EQUAL_INT(DET_STATE_MAINTENANCE, state_now());
}

/* @verifies REQ-ALM-002 */
static void test_detector_gas_above_al2_from_normal_reaches_alarm_2_in_three_samples(void)
{
    /* A sudden large leak: three samples at AL2 must give ALARM_2, as REQ-ALM-002
     * says, not one sample later through ALARM_1. */
    bool energised = true;

    finish_warmup();
    step_n(DETECTOR_AL2_DEFAULT, DETECTOR_ENTER_SAMPLES - 1u);
    TEST_ASSERT_EQUAL_INT(DET_STATE_NORMAL, state_now());   /* Not one sample early either. */
    step(DETECTOR_AL2_DEFAULT);
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_2, state_now());
    /* On this direct path too, the relay drops and the alarm is latched. */
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_relay_energised(&det_test_state, &energised));
    TEST_ASSERT_FALSE(energised);
    step_n(0, 20u);
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_2, state_now());
}

/* @verifies REQ-ALM-001 REQ-ALM-002 */
static void test_detector_from_normal_two_samples_at_al2_then_clean_air_stay_normal(void)
{
    finish_warmup();
    step_n(DETECTOR_AL2_DEFAULT, 2u);
    step(0);
    TEST_ASSERT_EQUAL_INT(DET_STATE_NORMAL, state_now());
}

/* @verifies REQ-ALM-002 */
static void test_detector_from_normal_one_unit_below_al2_gives_alarm_1_only(void)
{
    gg_conc_t below = DETECTOR_AL2_DEFAULT;

    below = (gg_conc_t)(below - 1);
    finish_warmup();
    step_n(below, DETECTOR_ENTER_SAMPLES);
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_1, state_now());
}

/* @verifies REQ-ALM-001 REQ-ALM-002 */
static void test_detector_from_normal_two_samples_at_al2_then_one_at_al1_give_alarm_1(void)
{
    /* Three samples above AL1, but only two at AL2: AL2 was not reached three
     * times in a row, so ALARM_1, not ALARM_2. */
    finish_warmup();
    step_n(DETECTOR_AL2_DEFAULT, 2u);
    step(DETECTOR_AL1_DEFAULT);
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_1, state_now());
}

/* @verifies REQ-HMI-008 */
static void test_detector_touch_with_gas_at_al1_is_ignored(void)
{
    detector_input_t touch = input_with(DETECTOR_AL1_DEFAULT, true, false, DET_FAULT_NONE);

    finish_warmup();
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_step(&det_test_state, &touch));
    TEST_ASSERT_EQUAL_INT(DET_STATE_NORMAL, state_now());
}

/* @verifies REQ-HMI-008 */
static void test_detector_touch_with_gas_one_unit_below_al1_enters_maintenance(void)
{
    gg_conc_t below = DETECTOR_AL1_DEFAULT;
    detector_input_t touch = input_with(0, true, false, DET_FAULT_NONE);

    below = (gg_conc_t)(below - 1);
    touch.conc = below;
    finish_warmup();
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_step(&det_test_state, &touch));
    TEST_ASSERT_EQUAL_INT(DET_STATE_MAINTENANCE, state_now());
}

/* @verifies REQ-HMI-008 REQ-ALM-001 */
static void test_detector_touch_on_the_third_gas_sample_gives_the_alarm(void)
{
    /* The review case: the 3 s touch ends on the same sample as the third sample
     * above AL1. The alarm wins and the relay drops; MAINTENANCE is refused. */
    detector_input_t touch = input_with(DETECTOR_AL1_DEFAULT, true, false, DET_FAULT_NONE);
    bool energised = true;

    finish_warmup();
    step_n(DETECTOR_AL1_DEFAULT, DETECTOR_ENTER_SAMPLES - 1u);
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_step(&det_test_state, &touch));
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_1, state_now());
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_relay_energised(&det_test_state, &energised));
    TEST_ASSERT_FALSE(energised);
}

/* @verifies REQ-HMI-008 */
static void test_detector_touch_refused_for_gas_is_not_kept_for_later(void)
{
    /* A refused request is dropped: when the gas is gone, nothing enters
     * MAINTENANCE until the operator touches again. */
    detector_input_t touch = input_with(DETECTOR_AL1_DEFAULT, true, false, DET_FAULT_NONE);

    finish_warmup();
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_step(&det_test_state, &touch));
    step_n(0, 5u);
    TEST_ASSERT_EQUAL_INT(DET_STATE_NORMAL, state_now());
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
    RUN_TEST(test_detector_relay_released_on_alarm);
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
    RUN_TEST(test_detector_stuck_only_fault_ends_by_itself);
    RUN_TEST(test_detector_stuck_only_fault_keeps_the_alarm_2_latch);
    RUN_TEST(test_detector_real_fault_during_a_stuck_fault_latches);
    RUN_TEST(test_detector_stuck_after_a_real_fault_keeps_the_latch);
    RUN_TEST(test_detector_stuck_fault_ended_by_gas_goes_straight_to_the_alarm);
    RUN_TEST(test_detector_stuck_fault_returns_to_alarm_1);
    RUN_TEST(test_detector_stuck_fault_in_maintenance_honours_the_timer);
    RUN_TEST(test_detector_system_fault_during_a_stuck_fault_is_reported_and_latched);
    RUN_TEST(test_detector_stuck_fault_from_warmup_ends_in_normal_once_warmup_is_over);
    RUN_TEST(test_detector_gas_one_unit_below_al1_never_alarms);
    RUN_TEST(test_detector_two_samples_then_a_dip_restart_the_count);
    RUN_TEST(test_detector_largest_concentration_alarms);
    RUN_TEST(test_detector_gas_one_unit_below_al2_stays_in_alarm_1);
    RUN_TEST(test_detector_two_samples_at_al2_do_not_give_alarm_2);
    RUN_TEST(test_detector_clear_level_itself_does_not_count_as_below);
    RUN_TEST(test_detector_alarm_1_does_not_clear_while_gas_is_still_above_al1);
    RUN_TEST(test_detector_alarm_2_reset_at_the_clear_level_is_refused_one_below_accepted);
    RUN_TEST(test_detector_gas_back_after_an_alarm_2_reset_alarms_again);
    RUN_TEST(test_detector_reset_request_outside_alarm_2_changes_nothing);
    RUN_TEST(test_detector_accepts_the_widest_valid_thresholds);
    RUN_TEST(test_detector_rejected_thresholds_keep_the_ones_in_use);
    RUN_TEST(test_detector_gas_present_at_the_end_of_warmup_alarms_three_samples_later);
    RUN_TEST(test_detector_end_warmup_rejects_null_and_acts_once);
    RUN_TEST(test_detector_relay_is_energised_during_warmup);
    RUN_TEST(test_detector_relay_is_energised_again_after_alarm_1_clears);
    RUN_TEST(test_detector_touch_is_ignored_in_alarm_2_and_fault);
    RUN_TEST(test_detector_gas_after_maintenance_alarms_three_samples_after_its_end);
    RUN_TEST(test_detector_maintenance_holds_until_the_timer_expires);
    RUN_TEST(test_detector_gas_above_al2_from_normal_reaches_alarm_2_in_three_samples);
    RUN_TEST(test_detector_from_normal_one_unit_below_al2_gives_alarm_1_only);
    RUN_TEST(test_detector_from_normal_two_samples_at_al2_then_one_at_al1_give_alarm_1);
    RUN_TEST(test_detector_from_normal_two_samples_at_al2_then_clean_air_stay_normal);
    RUN_TEST(test_detector_touch_with_gas_at_al1_is_ignored);
    RUN_TEST(test_detector_touch_with_gas_one_unit_below_al1_enters_maintenance);
    RUN_TEST(test_detector_touch_on_the_third_gas_sample_gives_the_alarm);
    RUN_TEST(test_detector_touch_refused_for_gas_is_not_kept_for_later);
    return UNITY_END();
}
