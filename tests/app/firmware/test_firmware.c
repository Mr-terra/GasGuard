/* Integration tests of the firmware application layer on the host fakes (SC-11 .. SC-16 build on these). */
#include "unity.h"
#include "common/gg_types/gg_types.h"
#include "app/firmware/firmware.h"
#include "hal/adc/adc.h"
#include "hal/adc/adc_fake.h"
#include "hal/eeprom/eeprom.h"
#include "hal/eeprom/eeprom_fake.h"
#include "hal/gpio/gpio.h"
#include "hal/gpio/gpio_fake.h"
#include "hal/stack/stack.h"
#include "hal/stack/stack_fake.h"
#include "services/config/config.h"
#include "services/eventlog/eventlog.h"

#include <stdbool.h>
#include <stdint.h>

/* DEV-03: Unity hooks, called by the framework; MISRA 8.6 and unusedFunction do not apply. */
// cppcheck-suppress misra-c2012-8.6
void setUp(void);
// cppcheck-suppress misra-c2012-8.6
void tearDown(void);

/** @brief Raw value of clean air: 2 %LEL with the identity calibration (below AL1). */
#define CLEAN_RAW 20u
/** @brief Clean air plus 2 LSB: the MQ-2 jitter of the frozen-channel test. */
#define CLEAN_RAW_PLUS_2 22u
/** @brief Raw value of a gas alarm level: 15 %LEL (between AL1 and AL2). */
#define ALARM_1_RAW 150u
/** @brief Raw value above AL2 (25 %LEL). */
#define ALARM_2_RAW 300u
/** @brief Hold time of a long press, and one step of the main loop, in milliseconds. */
#define HOLD_LONG_TICKS 35u
#define TICK_MS 100u

static firmware_t fw;
static uint32_t now_test;

void setUp(void)
{
    hal_eeprom_fake_reset();
    hal_adc_fake_reset();
    hal_gpio_fake_reset();
    hal_stack_fake_reset();
    (void)hal_gpio_init();
}

void tearDown(void) {}

/** @brief Set both gas channels to a raw value that the calibration maps to a concentration. */
static void set_gas_raw(uint16_t raw)
{
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_MQ5, raw));
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_MQ2, raw));
}

/** @brief Boot with no stored configuration, so the defaults are used. */
static void boot_defaults(void)
{
    now_test = 0u;
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_stack_paint());
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_boot(&fw, HAL_RESET_POWER_ON, 0u));
}

/**
 * @brief One pass of the main loop at 100 ms: sample, HMI inputs, outputs (as main.c does).
 *
 * The arguments say whether each input is pressed. The button module is active
 * high, so the raw pin level passed to firmware_inputs() is the same as pressed.
 */
static void fw_tick(bool button_pressed, bool touch_pressed)
{
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_sample(&fw, now_test));
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_inputs(&fw, now_test, button_pressed, touch_pressed));
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_output(&fw, now_test));
    now_test += TICK_MS;
}

/** @brief n passes of the loop with the same button and touch levels. */
static void fw_ticks(uint32_t n, bool button, bool touch)
{
    uint32_t i;

    for (i = 0u; i < n; i++) {
        fw_tick(button, touch);
    }
}

/** @brief Boot, then run past the 120 s warm-up on clean air: the detector is in NORMAL. */
static void boot_and_reach_normal(void)
{
    boot_defaults();
    set_gas_raw(CLEAN_RAW);
    fw_ticks(1210u, false, false);
    TEST_ASSERT_EQUAL_INT(DET_STATE_NORMAL, firmware_state(&fw));
}

/* @verifies REQ-SAF-006 */
static void test_firmware_boot_logs_the_reset_cause(void)
{
    eventlog_entry_t entry;

    boot_defaults();
    TEST_ASSERT_EQUAL_INT(GG_OK, eventlog_read_slot(0u, &entry));
    TEST_ASSERT_EQUAL_INT(EVENTLOG_RESET, entry.code);
    TEST_ASSERT_EQUAL_UINT16((uint16_t)HAL_RESET_POWER_ON, entry.arg);
}

/* @verifies REQ-CFG-002 */
static void test_firmware_boot_logs_when_defaults_are_used(void)
{
    eventlog_entry_t entry;

    boot_defaults();
    TEST_ASSERT_EQUAL_INT(GG_OK, eventlog_read_slot(1u, &entry));
    TEST_ASSERT_EQUAL_INT(EVENTLOG_CONFIG, entry.code);
    TEST_ASSERT_EQUAL_UINT16((uint16_t)CONFIG_SRC_DEFAULTS, entry.arg);
}

/* @verifies REQ-SAF-001 */
static void test_firmware_boot_starts_in_warmup_and_the_output_follows_it(void)
{
    bool energised = false;

    boot_defaults();
    TEST_ASSERT_EQUAL_INT(DET_STATE_WARMUP, firmware_state(&fw));
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_relay_energised(&fw, &energised));
    TEST_ASSERT_TRUE(energised);
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_output(&fw, 0u));
    TEST_ASSERT_TRUE(hal_gpio_fake_level(HAL_GPIO_PIN_RELAY));
}

/* @verifies REQ-ALM-001 */
static void test_firmware_alarm_1_after_warmup_and_three_samples(void)
{
    uint8_t i;
    eventlog_entry_t entry;

    boot_defaults();
    /* Calibration maps the raw value to 0.1 %LEL one for one: 150 raw is 150 units (15 %LEL). */
    set_gas_raw(ALARM_1_RAW);
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_sample(&fw, 130000u));
    for (i = 0u; i < 3u; i++) {
        TEST_ASSERT_EQUAL_INT(GG_OK, firmware_sample(&fw, 130000u + (uint32_t)i * 100u));
    }
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_1, firmware_state(&fw));
    TEST_ASSERT_EQUAL_INT(GG_OK, eventlog_read_slot(2u, &entry));
    TEST_ASSERT_EQUAL_INT(EVENTLOG_MODE_CHANGE, entry.code);
}

/* @verifies REQ-FUN-006 */
static void test_firmware_mq5_channel_reads_the_mapped_adc_input(void)
{
    uint8_t other = 0u;

#ifdef GG_SIM_POT
    /* Simulation build: A2 is the MQ-5 channel, A0 is not used. */
    other = 0u;
#else
    /* Normal build: A0 is the MQ-5 channel, A2 is not used. */
    other = 2u;
#endif
    boot_defaults();
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_MQ5, ALARM_1_RAW));
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_MQ2, CLEAN_RAW));
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(other, ALARM_2_RAW));
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_sample(&fw, 130000u));
    TEST_ASSERT_EQUAL_INT(ALARM_1_RAW, firmware_concentration(&fw));
}

/* @verifies REQ-HMI-001 */
static void test_firmware_concentration_is_the_last_sample_for_the_display(void)
{
    boot_defaults();
    set_gas_raw(ALARM_1_RAW);
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_sample(&fw, 130000u));
    TEST_ASSERT_EQUAL_INT(150, firmware_concentration(&fw));
}

/* @verifies REQ-SAF-007 */
static void test_firmware_both_channels_short_raise_fault_and_drop_the_relay(void)
{
    uint8_t i;
    bool energised = true;
    eventlog_entry_t entry;

    boot_defaults();
    set_gas_raw(1020u);
    for (i = 0u; i < 5u; i++) {
        TEST_ASSERT_EQUAL_INT(GG_OK, firmware_sample(&fw, 130000u + (uint32_t)i * 100u));
    }
    TEST_ASSERT_EQUAL_INT(DET_STATE_FAULT, firmware_state(&fw));
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_output(&fw, 0u));
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_relay_energised(&fw, &energised));
    TEST_ASSERT_FALSE(energised);
    TEST_ASSERT_FALSE(hal_gpio_fake_level(HAL_GPIO_PIN_RELAY));
    TEST_ASSERT_TRUE(hal_gpio_fake_level(HAL_GPIO_PIN_BUZZER));
    /* Out-of-range readings are never used: the fault comes at the first sample,
     * and the mode change to FAULT (slot 2) is followed by the fault (slot 3). */
    TEST_ASSERT_EQUAL_INT(GG_OK, eventlog_read_slot(2u, &entry));
    TEST_ASSERT_EQUAL_INT(EVENTLOG_MODE_CHANGE, entry.code);
    TEST_ASSERT_EQUAL_UINT16((uint16_t)DET_STATE_FAULT, entry.arg);
    TEST_ASSERT_EQUAL_INT(GG_OK, eventlog_read_slot(3u, &entry));
    TEST_ASSERT_EQUAL_INT(EVENTLOG_FAULT, entry.code);
}

/* @verifies REQ-HMI-008 */
static void test_firmware_touch_for_three_seconds_enters_maintenance(void)
{
    bool energised = false;

    boot_and_reach_normal();
    fw_ticks(HOLD_LONG_TICKS, false, true);
    TEST_ASSERT_EQUAL_INT(DET_STATE_MAINTENANCE, firmware_state(&fw));
    /* REQ-HMI-009: an alarm is shown but not output; the relay keeps its healthy state. */
    set_gas_raw(ALARM_2_RAW);
    fw_ticks(10u, false, false);
    TEST_ASSERT_EQUAL_INT(DET_STATE_MAINTENANCE, firmware_state(&fw));
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_relay_energised(&fw, &energised));
    TEST_ASSERT_TRUE(energised);
    TEST_ASSERT_FALSE(hal_gpio_fake_level(HAL_GPIO_PIN_BUZZER));
}

/* @verifies REQ-HMI-008 */
static void test_firmware_touch_shorter_than_three_seconds_is_ignored(void)
{
    boot_and_reach_normal();
    fw_ticks(20u, false, true);
    fw_ticks(3u, false, false);
    TEST_ASSERT_EQUAL_INT(DET_STATE_NORMAL, firmware_state(&fw));
}

/* @verifies REQ-HMI-004 */
static void test_firmware_short_press_silences_the_alarm_1_buzzer(void)
{
    boot_and_reach_normal();
    set_gas_raw(ALARM_1_RAW);
    /* The 8-sample moving average needs 8 samples to reach the new level. */
    fw_ticks(10u, false, false);
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_1, firmware_state(&fw));
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_output(&fw, 130000u));
    TEST_ASSERT_TRUE(hal_gpio_fake_level(HAL_GPIO_PIN_BUZZER));

    fw_ticks(5u, true, false);
    fw_ticks(3u, false, false);
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_output(&fw, 130000u));
    TEST_ASSERT_FALSE(hal_gpio_fake_level(HAL_GPIO_PIN_BUZZER));
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_1, firmware_state(&fw));
}

/* @verifies REQ-HMI-005 */
static void test_firmware_long_press_in_alarm_2_resets_the_latch_when_gas_is_gone(void)
{
    boot_and_reach_normal();
    set_gas_raw(ALARM_2_RAW);
    fw_ticks(10u, false, false);
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_2, firmware_state(&fw));
    set_gas_raw(CLEAN_RAW);
    fw_ticks(15u, false, false);
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_2, firmware_state(&fw));

    fw_ticks(HOLD_LONG_TICKS, true, false);
    fw_ticks(3u, false, false);
    TEST_ASSERT_EQUAL_INT(DET_STATE_NORMAL, firmware_state(&fw));
}

/* @verifies REQ-HMI-006 */
static void test_firmware_long_press_in_normal_runs_the_functional_test(void)
{
    uint32_t i;
    bool went_low = false;

    boot_and_reach_normal();
    fw_ticks(HOLD_LONG_TICKS, true, false);
    TEST_ASSERT_TRUE(firmware_test_running(&fw));
    for (i = 0u; i < 100u; i++) {
        fw_tick(false, false);
        if (!hal_gpio_fake_level(HAL_GPIO_PIN_RELAY)) {
            went_low = true;
        }
    }
    /* The test releases the relay in its fifth second, then returns to NORMAL. */
    TEST_ASSERT_TRUE(went_low);
    TEST_ASSERT_TRUE(hal_gpio_fake_level(HAL_GPIO_PIN_RELAY));
    TEST_ASSERT_FALSE(firmware_test_running(&fw));
    TEST_ASSERT_EQUAL_INT(DET_STATE_NORMAL, firmware_state(&fw));
}

/* @verifies REQ-SAF-008 */
static void test_firmware_overrun_is_written_to_the_event_log(void)
{
    eventlog_entry_t entry;

    boot_defaults();
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_report_timing(&fw, true, false, 0u));
    TEST_ASSERT_EQUAL_INT(GG_OK, eventlog_read_slot(2u, &entry));
    TEST_ASSERT_EQUAL_INT(EVENTLOG_OVERRUN, entry.code);
}

/* @verifies REQ-SAF-008 */
static void test_firmware_three_overruns_latch_fault(void)
{
    boot_and_reach_normal();
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_report_timing(&fw, true, true, now_test));
    fw_tick(false, false);
    TEST_ASSERT_EQUAL_INT(DET_STATE_FAULT, firmware_state(&fw));
}

/* @verifies REQ-SAF-009 */
static void test_firmware_stack_below_64_bytes_latches_fault(void)
{
    boot_and_reach_normal();
    hal_stack_fake_set_free(64u);
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_health(&fw, now_test));
    fw_tick(false, false);
    TEST_ASSERT_EQUAL_INT(DET_STATE_NORMAL, firmware_state(&fw));

    hal_stack_fake_set_free(63u);
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_health(&fw, now_test));
    fw_tick(false, false);
    TEST_ASSERT_EQUAL_INT(DET_STATE_FAULT, firmware_state(&fw));
}

/* @verifies REQ-NFR-002 */
static void test_firmware_null_arguments_are_rejected(void)
{
    bool energised = false;

    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, firmware_boot(NULL, HAL_RESET_POWER_ON, 0u));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, firmware_sample(NULL, 0u));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, firmware_output(NULL, 0u));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, firmware_relay_energised(&fw, NULL));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, firmware_inputs(NULL, 0u, false, false));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, firmware_health(NULL, 0u));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, firmware_report_display(NULL, false, 0u));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, firmware_report_timing(NULL, false, false, 0u));
    (void)energised;
}

/* @verifies REQ-SAF-003 */
static void test_firmware_frozen_channel_is_excluded_after_ten_minutes(void)
{
    uint32_t i;

    boot_defaults();
    /* MQ-5 frozen at a plausible gas level; MQ-2 moves by 2 LSB, so it is not stuck. */
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_MQ5, ALARM_1_RAW));
    for (i = 0u; i < 7400u; i++) {
        uint16_t mq2_raw = CLEAN_RAW;

        if (((i % 2u) == 1u)) {
            mq2_raw = CLEAN_RAW_PLUS_2;
        }
        TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_MQ2, mq2_raw));
        fw_tick(false, false);
    }
    /* Without the stuck check the frozen MQ-5 would keep ALARM_1 on. */
    TEST_ASSERT_EQUAL_INT(DET_STATE_NORMAL, firmware_state(&fw));
}

/* @verifies REQ-SAF-004 */
static void test_firmware_bandgap_out_of_tolerance_raises_fault(void)
{
    boot_and_reach_normal();
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_health(&fw, now_test));
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_bandgap(150u));
    /* The next measurement is due 60 s later. */
    fw_ticks(600u, false, false);
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_health(&fw, now_test));
    fw_tick(false, false);
    TEST_ASSERT_EQUAL_INT(DET_STATE_FAULT, firmware_state(&fw));
}

/* @verifies REQ-HMI-003 */
static void test_firmware_alarm_2_buzzer_beeps_four_times_a_second_at_the_output_rate(void)
{
    uint32_t i;
    uint32_t steps = FIRMWARE_OUTPUT_STEPS_PER_S;
    uint8_t edges = 0u;
    bool prev = false;

    boot_and_reach_normal();
    set_gas_raw(ALARM_2_RAW);
    fw_ticks(10u, false, false);
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_2, firmware_state(&fw));
    /* now_test is a multiple of 250 ms here: the pattern starts with a beep. */
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_output(&fw, now_test));
    prev = hal_gpio_fake_level(HAL_GPIO_PIN_BUZZER);
    for (i = 0u; i < steps; i++) {
        now_test += FIRMWARE_OUTPUT_PERIOD_MS;
        TEST_ASSERT_EQUAL_INT(GG_OK, firmware_output(&fw, now_test));
        if (hal_gpio_fake_level(HAL_GPIO_PIN_BUZZER) && !prev) {
            edges++;
        }
        prev = hal_gpio_fake_level(HAL_GPIO_PIN_BUZZER);
    }
    TEST_ASSERT_EQUAL_UINT8(4u, edges);
}

/* @verifies REQ-SAF-002 */
static void test_firmware_channel_fault_is_logged_once(void)
{
    uint8_t i;
    eventlog_entry_t entry;

    boot_defaults();
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_MQ2, CLEAN_RAW));
    /* MQ-5 open circuit: the range fault latches after 5 samples, and is logged once. */
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_MQ5, 0u));
    for (i = 0u; i < 8u; i++) {
        TEST_ASSERT_EQUAL_INT(GG_OK, firmware_sample(&fw, (uint32_t)i * FIRMWARE_OUTPUT_PERIOD_MS));
    }
    TEST_ASSERT_EQUAL_INT(GG_OK, eventlog_read_slot(2u, &entry));
    TEST_ASSERT_EQUAL_INT(EVENTLOG_CHANNEL_FAULT, entry.code);
    TEST_ASSERT_EQUAL_UINT16(0u, entry.arg);
    TEST_ASSERT_EQUAL_INT(GG_ERR_CRC, eventlog_read_slot(3u, &entry));
}

/* @verifies REQ-HMI-001 */
static void test_firmware_missing_display_is_logged(void)
{
    eventlog_entry_t entry;

    boot_defaults();
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_report_display(&fw, true, 0u));
    TEST_ASSERT_EQUAL_INT(GG_ERR_CRC, eventlog_read_slot(2u, &entry));
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_report_display(&fw, false, 0u));
    TEST_ASSERT_EQUAL_INT(GG_OK, eventlog_read_slot(2u, &entry));
    TEST_ASSERT_EQUAL_INT(EVENTLOG_DISPLAY_FAULT, entry.code);
}

/* @verifies REQ-SAF-002 */
static void test_firmware_channel_recovery_is_logged(void)
{
    uint32_t i;
    eventlog_entry_t entry;

    boot_defaults();
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_MQ2, CLEAN_RAW));
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_MQ5, 0u));
    for (i = 0u; i < 6u; i++) {
        TEST_ASSERT_EQUAL_INT(GG_OK, firmware_sample(&fw, i * TICK_MS));
    }
    TEST_ASSERT_EQUAL_INT(GG_OK, eventlog_read_slot(2u, &entry));
    TEST_ASSERT_EQUAL_INT(EVENTLOG_CHANNEL_FAULT, entry.code);
    /* The MQ-5 reads clean again: after 60 s of valid samples the channel is back. */
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_MQ5, CLEAN_RAW));
    for (i = 6u; i < (6u + DIAG_RECOVER_SAMPLES); i++) {
        TEST_ASSERT_EQUAL_INT(GG_OK, firmware_sample(&fw, i * TICK_MS));
    }
    TEST_ASSERT_EQUAL_INT(GG_OK, eventlog_read_slot(3u, &entry));
    TEST_ASSERT_EQUAL_INT(EVENTLOG_CHANNEL_RECOVERED, entry.code);
    TEST_ASSERT_EQUAL_UINT16(0u, entry.arg);
}

// DEV-03: Unity runner entry point, called by the test harness; MISRA 8.6 does not apply.
// cppcheck-suppress misra-c2012-8.6
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_firmware_boot_logs_the_reset_cause);
    RUN_TEST(test_firmware_boot_logs_when_defaults_are_used);
    RUN_TEST(test_firmware_boot_starts_in_warmup_and_the_output_follows_it);
    RUN_TEST(test_firmware_alarm_1_after_warmup_and_three_samples);
    RUN_TEST(test_firmware_mq5_channel_reads_the_mapped_adc_input);
    RUN_TEST(test_firmware_concentration_is_the_last_sample_for_the_display);
    RUN_TEST(test_firmware_both_channels_short_raise_fault_and_drop_the_relay);
    RUN_TEST(test_firmware_touch_for_three_seconds_enters_maintenance);
    RUN_TEST(test_firmware_touch_shorter_than_three_seconds_is_ignored);
    RUN_TEST(test_firmware_short_press_silences_the_alarm_1_buzzer);
    RUN_TEST(test_firmware_long_press_in_alarm_2_resets_the_latch_when_gas_is_gone);
    RUN_TEST(test_firmware_long_press_in_normal_runs_the_functional_test);
    RUN_TEST(test_firmware_overrun_is_written_to_the_event_log);
    RUN_TEST(test_firmware_three_overruns_latch_fault);
    RUN_TEST(test_firmware_stack_below_64_bytes_latches_fault);
    RUN_TEST(test_firmware_frozen_channel_is_excluded_after_ten_minutes);
    RUN_TEST(test_firmware_bandgap_out_of_tolerance_raises_fault);
    RUN_TEST(test_firmware_alarm_2_buzzer_beeps_four_times_a_second_at_the_output_rate);
    RUN_TEST(test_firmware_channel_fault_is_logged_once);
    RUN_TEST(test_firmware_missing_display_is_logged);
    RUN_TEST(test_firmware_channel_recovery_is_logged);
    RUN_TEST(test_firmware_null_arguments_are_rejected);
    return UNITY_END();
}
