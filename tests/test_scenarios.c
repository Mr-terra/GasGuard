/*
 * Scenario tests (L3, docs/05-verification-plan.md §3): scripted ADC sequences run through the
 * whole chain: diagnostics -> filter -> calibration -> channel selection -> detector.
 * Sample period 100 ms, as REQ-FUN-001. Scenarios SC-11, SC-12, SC-14..SC-16 need the
 * configuration, event log, maintenance and overrun modules (Day 4) and are not here yet.
 */
#include <stdbool.h>
#include <stdint.h>

#include "unity.h"
#include "common/gg_types.h"
#include "hal/hal_adc.h"
#include "services/calib.h"
#include "services/diag.h"
#include "services/filter.h"
#include "app/channel_select.h"
#include "app/detector.h"

/* DEV-03: Unity hooks, called by the framework; MISRA 8.6 and unusedFunction do not apply. */
// cppcheck-suppress misra-c2012-8.6
void setUp(void);
// cppcheck-suppress misra-c2012-8.6
void tearDown(void);

#define SC_SAMPLE_MS 100u
#define SC_WARMUP_MS 120000u           /* REQ-CFG-004 default warm-up: 120 s. */
#define SC_WARMUP_TICKS 1200u          /* 120 s at 100 ms. */
#define SC_ZERO_RAW 100u               /* Raw value of clean air. */
#define SC_GAIN_10_UNITS 2560u         /* 10 units (1 %LEL) per LSB, Q8. */
#define SC_CHANNELS 2u
#define SC_OSC_LOW_RAW 109u        /* 9 %LEL above zero. */
#define SC_OSC_HIGH_RAW 111u       /* 11 %LEL above zero. */
#define SC_JITTER_RAW 102u         /* 2 LSB above zero: moves the sensor, so it is not stuck. */

/* Per-channel processing chain. */
typedef struct {
    filter_t filter;
    diag_t diag;
} sc_channel_t;

static sc_channel_t sc_channels[SC_CHANNELS];
static calib_t sc_calib[SC_CHANNELS];
static detector_t sc_det;
static uint32_t sc_now_ms;

/* Run one channel through diagnostics, filter and calibration. */
static void channel_step(uint8_t c, uint16_t raw, bool in_warmup, gg_conc_t *conc, bool *valid)
{
    diag_stuck_sample_t sample;
    uint16_t avg = 0u;
    bool faulty = false;

    sample.raw = raw;
    sample.now_ms = sc_now_ms;
    sample.in_warmup = in_warmup;

    TEST_ASSERT_EQUAL_INT(GG_OK, diag_check_range(&sc_channels[c].diag, raw));
    TEST_ASSERT_EQUAL_INT(GG_OK, diag_check_stuck(&sc_channels[c].diag, &sample));
    TEST_ASSERT_EQUAL_INT(GG_OK, filter_push(&sc_channels[c].filter, raw));
    TEST_ASSERT_EQUAL_INT(GG_OK, filter_value(&sc_channels[c].filter, &avg));
    TEST_ASSERT_EQUAL_INT(GG_OK, calib_to_lel(&sc_calib[c], avg, conc));
    TEST_ASSERT_EQUAL_INT(GG_OK, diag_is_faulty(&sc_channels[c].diag, &faulty));
    *valid = !faulty;
}

static detector_state_t current_state(void)
{
    detector_state_t state = DET_STATE_FAULT;

    TEST_ASSERT_EQUAL_INT(GG_OK, detector_state(&sc_det, &state));
    return state;
}

/* One 100 ms sample of the whole detector. Returns the state after the sample. */
static detector_state_t tick(uint16_t raw0, uint16_t raw1, bool reset)
{
    uint16_t raw[SC_CHANNELS] = {raw0, raw1};
    gg_conc_t conc[SC_CHANNELS] = {0, 0};
    bool valid[SC_CHANNELS] = {false, false};
    gg_conc_t max = 0;
    bool any = false;
    detector_input_t in;
    uint8_t c;

    if ((current_state() == DET_STATE_WARMUP) && (sc_now_ms >= SC_WARMUP_MS)) {
        TEST_ASSERT_EQUAL_INT(GG_OK, detector_end_warmup(&sc_det));
    }
    for (c = 0u; c < SC_CHANNELS; c++) {
        channel_step(c, raw[c], current_state() == DET_STATE_WARMUP, &conc[c], &valid[c]);
    }
    TEST_ASSERT_EQUAL_INT(GG_OK, channel_select_max(conc, valid, SC_CHANNELS, &max, &any));

    in.conc = max;
    in.fault = !any;
    in.reset_requested = reset;
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_step(&sc_det, &in));

    sc_now_ms += SC_SAMPLE_MS;
    return current_state();
}

/* Run n ticks with the same raw values; return the state after the last one. */
static detector_state_t run(uint16_t raw0, uint16_t raw1, uint32_t n)
{
    detector_state_t state = current_state();
    uint32_t i;

    for (i = 0u; i < n; i++) {
        state = tick(raw0, raw1, false);
    }
    return state;
}

/* Boot to NORMAL on clean air, both channels at zero. */
static void boot_to_normal(void)
{
    TEST_ASSERT_EQUAL_INT(DET_STATE_NORMAL, run(SC_ZERO_RAW, SC_ZERO_RAW, SC_WARMUP_TICKS + 1u));
}

void setUp(void)
{
    uint8_t c;
    calib_config_t cfg = {SC_ZERO_RAW, SC_GAIN_10_UNITS, 0u, 0};

    sc_now_ms = 0u;
    for (c = 0u; c < SC_CHANNELS; c++) {
        TEST_ASSERT_EQUAL_INT(GG_OK, filter_init(&sc_channels[c].filter));
        TEST_ASSERT_EQUAL_INT(GG_OK, diag_init(&sc_channels[c].diag));
        TEST_ASSERT_EQUAL_INT(GG_OK, calib_init(&sc_calib[c], &cfg));
    }
    const detector_thresholds_t thresholds = {
        DETECTOR_AL1_DEFAULT, DETECTOR_AL2_DEFAULT, DETECTOR_HYSTERESIS_DEFAULT
    };

    TEST_ASSERT_EQUAL_INT(GG_OK, detector_init(&sc_det, &thresholds));
}

void tearDown(void) {}

/* SC-01: boot on healthy sensors: WARMUP for 120 s, then NORMAL, relay energised. */
/* @verifies REQ-ALM-006 REQ-CFG-004 REQ-SAF-001 */
static void test_sc01_boot_reaches_normal_after_warmup(void)
{
    bool energised = false;

    TEST_ASSERT_EQUAL_INT(DET_STATE_WARMUP, run(SC_ZERO_RAW, SC_ZERO_RAW, SC_WARMUP_TICKS));
    TEST_ASSERT_EQUAL_INT(DET_STATE_NORMAL, run(SC_ZERO_RAW, SC_ZERO_RAW, 1u));
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_relay_energised(&sc_det, &energised));
    TEST_ASSERT_TRUE(energised);
}

/* SC-02: gas ramp 0 -> 30 %LEL. ALARM_1 first (at 10 %LEL), then ALARM_2 (at 25 %LEL). */
/* @verifies REQ-ALM-001 REQ-ALM-002 REQ-FUN-002 */
static void test_sc02_gas_ramp_enters_alarm_1_then_alarm_2(void)
{
    uint32_t first_alarm_1 = 0u;
    uint32_t first_alarm_2 = 0u;
    uint32_t i;
    detector_state_t state;

    boot_to_normal();
    for (i = 0u; i < 60u; i++) {
        uint16_t raw = (uint16_t)(SC_ZERO_RAW + ((i < 30u) ? i : 30u));

        state = tick(raw, raw, false);
        if ((state == DET_STATE_ALARM_1) && (first_alarm_1 == 0u)) {
            first_alarm_1 = i + 1u;
        }
        if ((state == DET_STATE_ALARM_2) && (first_alarm_2 == 0u)) {
            first_alarm_2 = i + 1u;
        }
    }
    TEST_ASSERT_TRUE(first_alarm_1 > 0u);
    TEST_ASSERT_TRUE(first_alarm_2 > first_alarm_1);
}

/* SC-03: one sample at 50 %LEL. The filter spreads it over eight samples: no alarm. */
/* @verifies REQ-FUN-002 REQ-ALM-001 */
static void test_sc03_single_spike_does_not_alarm(void)
{
    boot_to_normal();
    TEST_ASSERT_EQUAL_INT(DET_STATE_NORMAL, tick(SC_ZERO_RAW + 50u, SC_ZERO_RAW + 50u, false));
    TEST_ASSERT_EQUAL_INT(DET_STATE_NORMAL, run(SC_ZERO_RAW, SC_ZERO_RAW, 20u));
}

/* SC-04: oscillation between 9 and 11 %LEL. ALARM_1 is entered once and never chatters. */
/* @verifies REQ-ALM-003 */
static void test_sc04_oscillation_does_not_chatter(void)
{
    uint32_t entries = 0u;
    detector_state_t previous;
    detector_state_t state;
    uint32_t i;

    boot_to_normal();
    previous = DET_STATE_NORMAL;
    for (i = 0u; i < 200u; i++) {
        uint16_t raw = SC_OSC_HIGH_RAW;

        if ((i % 2u) == 0u) {
            raw = SC_OSC_LOW_RAW;
        }

        state = tick(raw, raw, false);
        if ((state == DET_STATE_ALARM_1) && (previous != DET_STATE_ALARM_1)) {
            entries++;
        }
        previous = state;
    }
    /* Exactly one entry: the alarm is raised (the average sits at 10 %LEL) and then held. */
    TEST_ASSERT_EQUAL_UINT32(1u, entries);
}

/* SC-05: ALARM_2, then the gas goes away without a reset: ALARM_2 stays latched. */
/* @verifies REQ-ALM-004 */
static void test_sc05_alarm_2_stays_latched_when_gas_drops(void)
{
    boot_to_normal();
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_2, run(SC_ZERO_RAW + 30u, SC_ZERO_RAW + 30u, 20u));
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_2, run(SC_ZERO_RAW, SC_ZERO_RAW, 200u));
}

/* SC-06: ALARM_2, gas gone, long press: back to NORMAL. */
/* @verifies REQ-ALM-004 REQ-HMI-005 */
static void test_sc06_long_press_resets_alarm_2_when_gas_gone(void)
{
    boot_to_normal();
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_2, run(SC_ZERO_RAW + 30u, SC_ZERO_RAW + 30u, 20u));
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_2, run(SC_ZERO_RAW, SC_ZERO_RAW, 20u));
    TEST_ASSERT_EQUAL_INT(DET_STATE_NORMAL, tick(SC_ZERO_RAW, SC_ZERO_RAW, true));
}

/* SC-07: ALARM_2, gas still present, long press: ALARM_2 stays. */
/* @verifies REQ-ALM-004 REQ-HMI-005 */
static void test_sc07_long_press_refused_while_gas_present(void)
{
    boot_to_normal();
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_2, run(SC_ZERO_RAW + 30u, SC_ZERO_RAW + 30u, 20u));
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_2,
                          tick(SC_ZERO_RAW + 30u, SC_ZERO_RAW + 30u, true));
}

/* SC-08: MQ5 unplugged (raw 0). Its channel is faulty, MQ2 still reads, no FAULT. */
/* @verifies REQ-SAF-002 REQ-FUN-005 REQ-SAF-007 */
static void test_sc08_one_sensor_unplugged_is_channel_fault_only(void)
{
    bool faulty = false;
    detector_state_t state;

    boot_to_normal();
    state = run(0u, SC_ZERO_RAW, 10u);
    TEST_ASSERT_TRUE(state != DET_STATE_FAULT);
    TEST_ASSERT_EQUAL_INT(GG_OK, diag_is_faulty(&sc_channels[0].diag, &faulty));
    TEST_ASSERT_TRUE(faulty);
    TEST_ASSERT_EQUAL_INT(GG_OK, diag_is_faulty(&sc_channels[1].diag, &faulty));
    TEST_ASSERT_FALSE(faulty);
}

/* SC-09: both sensors unplugged: FAULT, relay de-energised, and the fault stays latched. */
/* @verifies REQ-SAF-007 REQ-SAF-001 */
static void test_sc09_both_sensors_unplugged_is_fault_with_relay_off(void)
{
    bool energised = true;

    boot_to_normal();
    TEST_ASSERT_EQUAL_INT(DET_STATE_FAULT, run(0u, 0u, 10u));
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_relay_energised(&sc_det, &energised));
    TEST_ASSERT_FALSE(energised);
    TEST_ASSERT_EQUAL_INT(DET_STATE_FAULT, run(SC_ZERO_RAW, SC_ZERO_RAW, 50u));
}

/* SC-10: one sensor stuck for more than 10 minutes after warm-up: channel fault only. */
/* @verifies REQ-SAF-003 REQ-SAF-007 */
static void test_sc10_stuck_sensor_is_channel_fault(void)
{
    bool faulty = false;
    detector_state_t state;
    uint32_t i;

    boot_to_normal();
    /* Channel 1 keeps moving by 2 LSB, so it is not stuck. Channel 0 is frozen. */
    state = DET_STATE_NORMAL;
    for (i = 0u; i < 6100u; i++) {
        state = tick(SC_ZERO_RAW, ((i % 2u) == 0u) ? SC_ZERO_RAW : SC_JITTER_RAW, false);
    }
    TEST_ASSERT_TRUE(state != DET_STATE_FAULT);
    TEST_ASSERT_EQUAL_INT(GG_OK, diag_is_faulty(&sc_channels[0].diag, &faulty));
    TEST_ASSERT_TRUE(faulty);
    TEST_ASSERT_EQUAL_INT(GG_OK, diag_is_faulty(&sc_channels[1].diag, &faulty));
    TEST_ASSERT_FALSE(faulty);
}

/* SC-13: gas present during warm-up: no alarm at any time, state stays WARMUP. */
/* @verifies REQ-ALM-006 */
static void test_sc13_gas_during_warmup_raises_no_alarm(void)
{
    uint32_t i;
    detector_state_t state;

    for (i = 0u; i < 500u; i++) {
        state = tick(SC_ZERO_RAW + 150u, SC_ZERO_RAW + 150u, false);
        TEST_ASSERT_EQUAL_INT(DET_STATE_WARMUP, state);
    }
}

// cppcheck-suppress misra-c2012-8.6
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_sc01_boot_reaches_normal_after_warmup);
    RUN_TEST(test_sc02_gas_ramp_enters_alarm_1_then_alarm_2);
    RUN_TEST(test_sc03_single_spike_does_not_alarm);
    RUN_TEST(test_sc04_oscillation_does_not_chatter);
    RUN_TEST(test_sc05_alarm_2_stays_latched_when_gas_drops);
    RUN_TEST(test_sc06_long_press_resets_alarm_2_when_gas_gone);
    RUN_TEST(test_sc07_long_press_refused_while_gas_present);
    RUN_TEST(test_sc08_one_sensor_unplugged_is_channel_fault_only);
    RUN_TEST(test_sc09_both_sensors_unplugged_is_fault_with_relay_off);
    RUN_TEST(test_sc10_stuck_sensor_is_channel_fault);
    RUN_TEST(test_sc13_gas_during_warmup_raises_no_alarm);
    return UNITY_END();
}
