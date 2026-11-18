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

/**
 * @brief Raw value of clean air on both channels, for the raw-only diagnostics.
 *
 * Above the open-circuit limit; below both zero offsets, so it reads 0 %LEL
 * (2 %LEL on the simulation MQ-5, which has no offset).
 */
#define CLEAN_RAW 20u
/** @brief Clean air plus 2 LSB: the MQ-2 jitter of the frozen-channel test. */
#define CLEAN_RAW_PLUS_2 22u
/** @brief Gas level between AL1 and AL2: 15 %LEL, in LSB above a channel's zero offset. */
#define ALARM_1_RAW 150u
/** @brief Gas level above AL2: 30 %LEL, in LSB above a channel's zero offset. */
#define ALARM_2_RAW 300u
/** @brief Raw value on the MQ-5 that reads 15 %LEL after its zero offset. */
#define MQ5_ALARM_1_RAW (ALARM_1_RAW + FIRMWARE_ZERO_RAW_MQ5)
/** @brief Raw value on the MQ-5 that reads 30 %LEL after its zero offset. */
#define MQ5_ALARM_2_RAW (ALARM_2_RAW + FIRMWARE_ZERO_RAW_MQ5)
/** @brief Raw value on the MQ-2 that reads 15 %LEL after its zero offset. */
#define MQ2_ALARM_1_RAW (ALARM_1_RAW + FIRMWARE_ZERO_RAW_MQ2)
/** @brief Raw value on the MQ-2 that reads 30 %LEL after its zero offset. */
#define MQ2_ALARM_2_RAW (ALARM_2_RAW + FIRMWARE_ZERO_RAW_MQ2)
/** @brief Small MQ-2 reading (1 %LEL after its zero offset), above the open-circuit limit. */
#define MQ2_LOW_RAW (FIRMWARE_ZERO_RAW_MQ2 + 10u)
/** @brief Hold time of a long press, in main loop steps. */
#define HOLD_LONG_TICKS 35u
/** @brief One step of the main loop, in milliseconds. */
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

/** @brief Set both gas channels to the same raw value (clean air, rails, stuck tests). */
static void set_gas_raw(uint16_t raw)
{
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_MQ5, raw));
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_MQ2, raw));
}

/** @brief Set both gas channels to the same gas level, in 0.1 %LEL above each zero offset. */
static void set_gas_level(uint16_t level)
{
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_MQ5, (uint16_t)(level + FIRMWARE_ZERO_RAW_MQ5)));
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_MQ2, (uint16_t)(level + FIRMWARE_ZERO_RAW_MQ2)));
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

/** @brief Number of valid log entries with this code; any argument when arg is ANY_ARG. */
#define ANY_ARG 0xFFFFu
static uint8_t count_entries(eventlog_code_t code, uint16_t arg)
{
    eventlog_entry_t entry;
    uint8_t slot;
    uint8_t count = 0u;

    for (slot = 0u; slot < EVENTLOG_ENTRIES; slot++) {
        if ((eventlog_read_slot(slot, &entry) == GG_OK) && (entry.code == code)
            && ((arg == ANY_ARG) || (entry.arg == arg))) {
            count++;
        }
    }
    return count;
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
    /* Above its zero, one raw LSB is 0.1 %LEL: both channels at 15 %LEL. */
    set_gas_level(ALARM_1_RAW);
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
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_MQ5, MQ5_ALARM_1_RAW));
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_MQ2, CLEAN_RAW));
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(other, ALARM_2_RAW));
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_sample(&fw, 130000u));
    TEST_ASSERT_EQUAL_INT(ALARM_1_RAW, firmware_concentration(&fw));
}

/* @verifies REQ-FUN-003 REQ-FUN-005 */
static void test_firmware_each_channel_subtracts_its_own_zero(void)
{
    boot_defaults();
    /* MQ-5 20 raw above its zero reads 2 %LEL; the MQ-2 10 raw above its own zero reads 1 %LEL. */
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_MQ5, FIRMWARE_ZERO_RAW_MQ5 + 20u));
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_MQ2, MQ2_LOW_RAW));
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_sample(&fw, 130000u));
    TEST_ASSERT_EQUAL_INT(20, firmware_concentration(&fw));
    /* Lowest clean-air values seen by the ADC over 2.5 days on the bench (2026-11-15 to 17). */
    TEST_ASSERT_EQUAL_UINT16(35u, FIRMWARE_ZERO_RAW_MQ2);
#ifndef GG_SIM_POT
    TEST_ASSERT_EQUAL_UINT16(111u, FIRMWARE_ZERO_RAW_MQ5);
#endif
}

/* @verifies REQ-FUN-003 */
static void test_firmware_mq5_below_its_zero_reads_zero(void)
{
#ifdef GG_SIM_POT
    /* The simulation build has no MQ-5 offset: raw 0 would be an open circuit, not a clamp. */
    TEST_IGNORE_MESSAGE("no MQ-5 offset in the simulation build");
#endif
    boot_defaults();
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_MQ5, FIRMWARE_ZERO_RAW_MQ5));
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_MQ2, MQ2_LOW_RAW));
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_sample(&fw, 130000u));
    TEST_ASSERT_EQUAL_INT(10, firmware_concentration(&fw));
}

/* @verifies REQ-FUN-003 REQ-FUN-005 */
static void test_firmware_mq5_alone_above_al2_raises_alarm_2(void)
{
    bool energised = true;

    boot_and_reach_normal();
    /* MQ-2 stays clean: the alarm comes from the MQ-5 path, offset included. */
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_MQ5, MQ5_ALARM_2_RAW));
    fw_ticks(10u, false, false);
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_2, firmware_state(&fw));
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_relay_energised(&fw, &energised));
    TEST_ASSERT_FALSE(energised);
}

/* @verifies REQ-HMI-001 */
static void test_firmware_concentration_is_the_last_sample_for_the_display(void)
{
    boot_defaults();
    /* Both channels at 15 %LEL after their own zero offset. */
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_MQ5, MQ5_ALARM_1_RAW));
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_MQ2, MQ2_ALARM_1_RAW));
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
    set_gas_level(ALARM_2_RAW);
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
    set_gas_level(ALARM_1_RAW);
    /* The filter (median of 3, then average of 8) needs 9 samples to reach the new level. */
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

/* @verifies REQ-HMI-004 */
static void test_firmware_a_new_alarm_1_sounds_after_a_silenced_one(void)
{
    boot_and_reach_normal();
    set_gas_level(ALARM_1_RAW);
    fw_ticks(10u, false, false);
    fw_ticks(5u, true, false);     /* Short press: this ALARM_1 is silenced. */
    fw_ticks(3u, false, false);
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_1, firmware_state(&fw));
    /* The gas goes away: back to NORMAL. */
    set_gas_raw(CLEAN_RAW);
    fw_ticks(20u, false, false);
    TEST_ASSERT_EQUAL_INT(DET_STATE_NORMAL, firmware_state(&fw));
    /* A new leak: the new ALARM_1 must sound, the old silence does not carry over. */
    set_gas_level(ALARM_1_RAW);
    fw_ticks(10u, false, false);
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_1, firmware_state(&fw));
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_output(&fw, 130000u));
    TEST_ASSERT_TRUE(hal_gpio_fake_level(HAL_GPIO_PIN_BUZZER));
}

/* @verifies REQ-HMI-005 */
static void test_firmware_long_press_in_alarm_2_resets_the_latch_when_gas_is_gone(void)
{
    boot_and_reach_normal();
    set_gas_level(ALARM_2_RAW);
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

/** @brief Samples of the frozen-channel test, one a second: warm-up, the stuck window, one minute more. */
#define FROZEN_TEST_SAMPLES (CONFIG_WARMUP_DEFAULT_S + (DIAG_STUCK_MS / FIRMWARE_MS_PER_S) + 60u)

/* @verifies REQ-SAF-003 */
static void test_firmware_frozen_channel_is_excluded_after_ten_minutes(void)
{
    uint32_t i;

    boot_defaults();
    /* MQ-5 frozen at a plausible gas level; MQ-2 moves by 2 LSB, so it is not stuck. */
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_MQ5, MQ5_ALARM_1_RAW));
    /* One sample a second keeps the test fast. */
    for (i = 0u; i < FROZEN_TEST_SAMPLES; i++) {
        uint16_t mq2_raw = CLEAN_RAW;

        if (((i % 2u) == 1u)) {
            mq2_raw = CLEAN_RAW_PLUS_2;
        }
        TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_MQ2, mq2_raw));
        TEST_ASSERT_EQUAL_INT(GG_OK, firmware_sample(&fw, i * FIRMWARE_MS_PER_S));
        if (i == (CONFIG_WARMUP_DEFAULT_S + (DIAG_STUCK_MS / FIRMWARE_MS_PER_S) - 10u)) {
            /* Just before the stuck window ends: the frozen MQ-5 reading still holds ALARM_1. */
            TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_1, firmware_state(&fw));
        }
    }
    /* Without the stuck check the frozen MQ-5 would keep ALARM_1 on. */
    TEST_ASSERT_EQUAL_INT(DET_STATE_NORMAL, firmware_state(&fw));
}

/** @brief One sample a second for n seconds, from now_test on. */
static void sample_seconds(uint32_t n)
{
    uint32_t i;

    for (i = 0u; i < n; i++) {
        TEST_ASSERT_EQUAL_INT(GG_OK, firmware_sample(&fw, now_test));
        now_test += FIRMWARE_MS_PER_S;
    }
}

/** @brief Seconds from the start until a channel steady from the start is stuck, plus one minute. */
#define STUCK_RUN_S (CONFIG_WARMUP_DEFAULT_S + (DIAG_STUCK_MS / FIRMWARE_MS_PER_S) + 60u)

/* SC-10b (docs/05): both sensors stuck in calm air, then one moves. */
/* @verifies REQ-SAF-003 REQ-SAF-007 */
static void test_firmware_both_channels_stuck_give_a_fault_that_ends_by_itself(void)
{
    bool energised = true;

    boot_defaults();
    /* Calm clean air: both channels steady for more than 10 min after the warm-up. */
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_MQ5, FIRMWARE_ZERO_RAW_MQ5 + 20u));
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_MQ2, CLEAN_RAW));
    sample_seconds(STUCK_RUN_S);
    TEST_ASSERT_EQUAL_INT(DET_STATE_FAULT, firmware_state(&fw));
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_relay_energised(&fw, &energised));
    TEST_ASSERT_FALSE(energised);
    /* The MQ-2 moves: it is in use again and the FAULT ends without a restart. */
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_MQ2, CLEAN_RAW + 5u));
    sample_seconds(1u);
    TEST_ASSERT_EQUAL_INT(DET_STATE_NORMAL, firmware_state(&fw));
}

/** @brief Both channels steady in clean air, run until both are stuck: a stuck-only FAULT. */
static void both_channels_stuck(void)
{
    boot_defaults();
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_MQ5, FIRMWARE_ZERO_RAW_MQ5 + 20u));
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_MQ2, CLEAN_RAW));
    sample_seconds(STUCK_RUN_S);
    TEST_ASSERT_EQUAL_INT(DET_STATE_FAULT, firmware_state(&fw));
}

/* @verifies REQ-SAF-007 */
static void test_firmware_adc_error_during_a_stuck_fault_latches_it(void)
{
    detector_fault_code_t code = DET_FAULT_NONE;

    both_channels_stuck();
    /* An ADC read error is a real fault, never calm air: the FAULT latches. */
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_fail_channel(FIRMWARE_ADC_MQ5, true));
    sample_seconds(1u);
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_fault_code(&fw.det, &code));
    TEST_ASSERT_EQUAL_INT(DET_FAULT_INPUT, code);
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_fail_channel(FIRMWARE_ADC_MQ5, false));
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_MQ2, CLEAN_RAW + 5u));
    sample_seconds(5u);
    TEST_ASSERT_EQUAL_INT(DET_STATE_FAULT, firmware_state(&fw));
}

/* @verifies REQ-SAF-007 REQ-SAF-001 */
static void test_firmware_gas_ending_a_stuck_fault_gives_the_alarm_with_the_usual_confirmation(void)
{
    bool energised = true;

    /* Decision of the author (2026-11-17, option X): the filter of a channel left
     * out keeps its history, so gas that ends a stuck FAULT is treated like gas
     * that starts in NORMAL: the FAULT ends, then the alarm follows once the
     * filtered value has been at AL1 for 3 samples. */
    both_channels_stuck();
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_MQ5, MQ5_ALARM_1_RAW));
    sample_seconds(1u);
    TEST_ASSERT_EQUAL_INT(DET_STATE_NORMAL, firmware_state(&fw));   /* One sample: not a leak yet. */
    sample_seconds(11u);
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_1, firmware_state(&fw));
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_relay_energised(&fw, &energised));
    TEST_ASSERT_FALSE(energised);
}

/* @verifies REQ-SAF-007 REQ-LOG-001 */
static void test_firmware_stuck_fault_episode_is_logged(void)
{
    both_channels_stuck();
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_MQ2, CLEAN_RAW + 5u));
    sample_seconds(1u);
    TEST_ASSERT_EQUAL_UINT8(1u, count_entries(EVENTLOG_FAULT, (uint16_t)DET_FAULT_STUCK));
    TEST_ASSERT_EQUAL_UINT8(1u, count_entries(EVENTLOG_MODE_CHANGE, (uint16_t)DET_STATE_FAULT));
    TEST_ASSERT_EQUAL_UINT8(1u, count_entries(EVENTLOG_CHANNEL_RECOVERED, (uint16_t)FIRMWARE_CHANNEL_MQ2));
}

/* @verifies REQ-SAF-003 REQ-SAF-007 */
static void test_firmware_stuck_channel_with_the_other_open_latches_the_fault(void)
{
    boot_defaults();
    /* MQ-2 open circuit: a real fault. MQ-5 steady: stuck after 10 min. */
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_MQ5, FIRMWARE_ZERO_RAW_MQ5 + 20u));
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_MQ2, 0u));
    sample_seconds(STUCK_RUN_S);
    TEST_ASSERT_EQUAL_INT(DET_STATE_FAULT, firmware_state(&fw));
    /* The MQ-5 moves again, but the open MQ-2 is a real fault: the FAULT stays latched. */
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_MQ5, FIRMWARE_ZERO_RAW_MQ5 + 30u));
    sample_seconds(5u);
    TEST_ASSERT_EQUAL_INT(DET_STATE_FAULT, firmware_state(&fw));
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

/** @brief Number of BANDGAP entries in the whole event log. */
static uint8_t count_bandgap_entries(void)
{
    eventlog_entry_t entry;
    uint8_t slot;
    uint8_t count = 0u;

    for (slot = 0u; slot < EVENTLOG_ENTRIES; slot++) {
        if ((eventlog_read_slot(slot, &entry) == GG_OK) && (entry.code == EVENTLOG_BANDGAP)) {
            count++;
        }
    }
    return count;
}

/* @verifies REQ-SAF-004 */
static void test_firmware_bandgap_out_of_tolerance_logs_its_raw_value_once(void)
{
    eventlog_entry_t entry;

    boot_defaults();
    set_gas_raw(CLEAN_RAW);
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_bandgap(150u));
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_health(&fw, now_test));
    TEST_ASSERT_EQUAL_INT(GG_OK, eventlog_read_slot(2u, &entry));
    TEST_ASSERT_EQUAL_INT(EVENTLOG_BANDGAP, entry.code);
    TEST_ASSERT_EQUAL_UINT16(150u, entry.arg);
    /* Still out of tolerance at the next measurement: no second entry (REQ-LOG-003). */
    fw_ticks(600u, false, false);
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_health(&fw, now_test));
    TEST_ASSERT_EQUAL_UINT8(1u, count_bandgap_entries());
}

/* @verifies REQ-SAF-004 */
static void test_firmware_bandgap_within_tolerance_logs_nothing(void)
{
    boot_defaults();
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_bandgap(DIAG_BANDGAP_NOMINAL));
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_health(&fw, now_test));
    TEST_ASSERT_EQUAL_UINT8(0u, count_bandgap_entries());
}

/* @verifies REQ-SAF-004 */
static void test_firmware_failed_bandgap_read_is_logged_as_zero_and_raises_fault(void)
{
    eventlog_entry_t entry;

    boot_defaults();
    set_gas_raw(CLEAN_RAW);
    hal_adc_fake_fail_bandgap(true);
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_health(&fw, now_test));
    TEST_ASSERT_EQUAL_INT(GG_OK, eventlog_read_slot(2u, &entry));
    TEST_ASSERT_EQUAL_INT(EVENTLOG_BANDGAP, entry.code);
    TEST_ASSERT_EQUAL_UINT16(0u, entry.arg);
    fw_tick(false, false);
    TEST_ASSERT_EQUAL_INT(DET_STATE_FAULT, firmware_state(&fw));
}

/* @verifies REQ-SAF-004 */
static void test_firmware_bandgap_fault_latches_even_when_the_log_write_fails(void)
{
    bool energised = true;

    boot_defaults();
    set_gas_raw(CLEAN_RAW);
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_bandgap(150u));
    hal_eeprom_fake_fail_writes(true);
    /* The log entry is a diagnostic: its failure must not stop the fault (FMEA F44). */
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_health(&fw, now_test));
    /* The mode-change entry fails too, so the sample reports an error; the state is what matters. */
    (void)firmware_sample(&fw, now_test);
    TEST_ASSERT_EQUAL_INT(DET_STATE_FAULT, firmware_state(&fw));
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_relay_energised(&fw, &energised));
    TEST_ASSERT_FALSE(energised);
}

/* @verifies REQ-SAF-004 */
static void test_firmware_bandgap_is_logged_again_after_a_reset(void)
{
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_bandgap(150u));
    boot_defaults();
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_health(&fw, now_test));
    /* Second boot on the same EEPROM: the first bad value of this boot is logged too. */
    boot_defaults();
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_health(&fw, now_test));
    TEST_ASSERT_EQUAL_UINT8(2u, count_bandgap_entries());
}

/** @brief Running time that clears the start counter: the default warm-up, then 60 s outside it. */
#define START_CLEAR_MS (((uint32_t)CONFIG_WARMUP_DEFAULT_S * 1000u) + FIRMWARE_STABLE_RUN_MS)

/** @brief Start the firmware and run one loop pass, as a start cut short by a reset. */
static void short_start(void)
{
    boot_defaults();
    fw_tick(false, false);
}

/** @brief Run the given number of short starts on clean air. */
static void short_starts(uint8_t n)
{
    uint8_t i;

    set_gas_raw(CLEAN_RAW);
    for (i = 0u; i < n; i++) {
        short_start();
    }
}

/* @verifies REQ-SAF-011 */
static void test_firmware_fourth_short_start_enters_fault_and_logs_it(void)
{
    uint8_t i;
    bool energised = true;
    detector_fault_code_t code = DET_FAULT_NONE;

    set_gas_raw(CLEAN_RAW);
    for (i = 0u; i < FIRMWARE_SHORT_STARTS_MAX; i++) {
        short_start();
        TEST_ASSERT_EQUAL_INT(DET_STATE_WARMUP, firmware_state(&fw));
    }
    short_start();
    TEST_ASSERT_EQUAL_INT(DET_STATE_FAULT, firmware_state(&fw));
    TEST_ASSERT_EQUAL_INT(GG_OK, detector_fault_code(&fw.det, &code));
    TEST_ASSERT_EQUAL_INT(DET_FAULT_RESET_LOOP, code);
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_relay_energised(&fw, &energised));
    TEST_ASSERT_FALSE(energised);
    TEST_ASSERT_EQUAL_UINT8(1u, count_entries(EVENTLOG_FAULT, (uint16_t)DET_FAULT_RESET_LOOP));
    /* Every start logs its reset, the fourth one included. */
    TEST_ASSERT_EQUAL_UINT8(FIRMWARE_SHORT_STARTS_MAX + 1u, count_entries(EVENTLOG_RESET, ANY_ARG));
}

/* @verifies REQ-SAF-011 */
static void test_firmware_later_short_starts_stay_in_fault_and_log_each_start(void)
{
    /* Six short starts: the 4th, 5th and 6th are in FAULT; each one logs its reset,
     * its configuration source and the reset-loop fault (decision of the author). */
    short_starts(FIRMWARE_SHORT_STARTS_MAX + 3u);
    TEST_ASSERT_EQUAL_INT(DET_STATE_FAULT, firmware_state(&fw));
    TEST_ASSERT_EQUAL_UINT8(3u, count_entries(EVENTLOG_FAULT, (uint16_t)DET_FAULT_RESET_LOOP));
    TEST_ASSERT_EQUAL_UINT8(FIRMWARE_SHORT_STARTS_MAX + 3u, count_entries(EVENTLOG_RESET, ANY_ARG));
    TEST_ASSERT_EQUAL_UINT8(FIRMWARE_SHORT_STARTS_MAX + 3u, count_entries(EVENTLOG_CONFIG, ANY_ARG));
}

/* @verifies REQ-SAF-011 */
static void test_firmware_sixty_seconds_after_warmup_clear_the_start_count(void)
{
    uint8_t i;

    set_gas_raw(CLEAN_RAW);
    for (i = 0u; i < FIRMWARE_SHORT_STARTS_MAX; i++) {
        short_start();
    }
    /* The third start runs through its warm-up and 60 s more: the next start is a normal one. */
    fw_ticks(START_CLEAR_MS / TICK_MS, false, false);
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_health(&fw, now_test));
    short_start();
    TEST_ASSERT_EQUAL_INT(DET_STATE_WARMUP, firmware_state(&fw));
}

/* @verifies REQ-SAF-011 */
static void test_firmware_reset_loop_fault_clears_after_a_full_run_and_a_new_start(void)
{
    uint8_t i;

    set_gas_raw(CLEAN_RAW);
    for (i = 0u; i <= FIRMWARE_SHORT_STARTS_MAX; i++) {
        short_start();
    }
    TEST_ASSERT_EQUAL_INT(DET_STATE_FAULT, firmware_state(&fw));
    fw_ticks(START_CLEAR_MS / TICK_MS, false, false);
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_health(&fw, now_test));
    short_start();
    TEST_ASSERT_EQUAL_INT(DET_STATE_WARMUP, firmware_state(&fw));
}

/* @verifies REQ-SAF-011 */
static void test_firmware_start_count_is_cleared_sixty_seconds_after_warmup_not_before(void)
{
    short_starts(FIRMWARE_SHORT_STARTS_MAX);
    /* 60 s from the start is not enough: the warm-up does not count (REQ-SAF-011). */
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_health(&fw, FIRMWARE_STABLE_RUN_MS));
    /* 1 ms before the warm-up end plus 60 s: not cleared, the fourth start is still a loop. */
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_health(&fw, START_CLEAR_MS - 1u));
    short_start();
    TEST_ASSERT_EQUAL_INT(DET_STATE_FAULT, firmware_state(&fw));

    hal_eeprom_fake_reset();
    short_starts(FIRMWARE_SHORT_STARTS_MAX);
    /* Exactly the warm-up end plus 60 s: cleared. */
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_health(&fw, START_CLEAR_MS));
    short_start();
    TEST_ASSERT_EQUAL_INT(DET_STATE_WARMUP, firmware_state(&fw));
}

/* @verifies REQ-SAF-011 */
static void test_firmware_failed_clear_is_tried_again_at_the_next_check(void)
{
    short_starts(FIRMWARE_SHORT_STARTS_MAX);
    hal_eeprom_fake_fail_writes(true);
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_health(&fw, START_CLEAR_MS));
    hal_eeprom_fake_fail_writes(false);
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_health(&fw, START_CLEAR_MS + 1000u));
    short_start();
    TEST_ASSERT_EQUAL_INT(DET_STATE_WARMUP, firmware_state(&fw));
}

/* @verifies REQ-SAF-011 */
static void test_firmware_start_counter_fits_after_the_log_and_a_write_error_fails_the_boot(void)
{
    /* The counter (2 bytes) sits after the last log entry and inside the EEPROM. */
    TEST_ASSERT_EQUAL_UINT32(1020u, FIRMWARE_ADDR_START_COUNT);
    TEST_ASSERT_TRUE((FIRMWARE_ADDR_START_COUNT + 2u) <= HAL_EEPROM_SIZE);
    hal_eeprom_fake_fail_writes(true);
    /* main.c then arms the watchdog without kicking it: the board restarts with the relay low. */
    TEST_ASSERT_NOT_EQUAL(GG_OK, firmware_boot(&fw, HAL_RESET_POWER_ON, 0u));
}

/* @verifies REQ-SAF-011 */
static void test_firmware_start_count_stays_at_its_maximum_in_a_long_loop(void)
{
    static const uint8_t full[2] = { 255u, 0u };   /* Count 255 and its complement. */
    uint8_t stored[2] = { 0u, 0u };

    TEST_ASSERT_EQUAL_INT(GG_OK, hal_eeprom_write(FIRMWARE_ADDR_START_COUNT, full, 2u));
    short_starts(1u);
    /* No wrap to 0: the start is still a loop start, in FAULT with the relay off. */
    TEST_ASSERT_EQUAL_INT(DET_STATE_FAULT, firmware_state(&fw));
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_eeprom_read(FIRMWARE_ADDR_START_COUNT, stored, 2u));
    TEST_ASSERT_EQUAL_UINT8(255u, stored[0]);
    TEST_ASSERT_EQUAL_UINT8(0u, stored[1]);
}

/* @verifies REQ-SAF-011 */
static void test_firmware_start_count_clear_follows_the_stored_warmup(void)
{
    config_t cfg;

    TEST_ASSERT_EQUAL_INT(GG_OK, config_defaults(&cfg));
    cfg.warmup_s = 600u;
    TEST_ASSERT_EQUAL_INT(GG_OK, config_store(&cfg));
    short_starts(FIRMWARE_SHORT_STARTS_MAX);
    /* 600 s of warm-up then 60 s: 659.999 s does not clear, 660 s does. */
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_health(&fw, 660000u - 1u));
    short_start();
    TEST_ASSERT_EQUAL_INT(DET_STATE_FAULT, firmware_state(&fw));

    hal_eeprom_fake_reset();
    TEST_ASSERT_EQUAL_INT(GG_OK, config_store(&cfg));
    short_starts(FIRMWARE_SHORT_STARTS_MAX);
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_health(&fw, 660000u));
    short_start();
    TEST_ASSERT_EQUAL_INT(DET_STATE_WARMUP, firmware_state(&fw));
}

/* @verifies REQ-SAF-011 */
static void test_firmware_corrupted_start_count_is_read_as_zero(void)
{
    static const uint8_t garbage[2] = { 7u, 7u };   /* The second byte is not the complement. */

    TEST_ASSERT_EQUAL_INT(GG_OK, hal_eeprom_write(FIRMWARE_ADDR_START_COUNT, garbage, 2u));
    set_gas_raw(CLEAN_RAW);
    short_start();
    TEST_ASSERT_EQUAL_INT(DET_STATE_WARMUP, firmware_state(&fw));
}

/* @verifies REQ-HMI-003 */
static void test_firmware_alarm_2_buzzer_beeps_four_times_a_second_at_the_output_rate(void)
{
    uint32_t i;
    uint32_t steps = FIRMWARE_OUTPUT_STEPS_PER_S;
    uint8_t edges = 0u;
    bool prev = false;

    boot_and_reach_normal();
    set_gas_level(ALARM_2_RAW);
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
    /* MQ-5 open circuit: the range fault is set after 5 samples, and is logged once. */
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

/** @brief Steps of the main loop in the 10 minutes of MAINTENANCE. */
#define MAINT_TICKS (FIRMWARE_MAINT_MAX_MS / TICK_MS)

/** @brief Expected argument of a PEAK entry: channel index and peak in 0.1 %LEL. */
static uint16_t peak_arg(uint8_t ch, uint16_t peak)
{
    return (uint16_t)(((uint16_t)ch << EVENTLOG_PEAK_CHANNEL_SHIFT) | peak);
}

/**
 * @brief From NORMAL, a 3 s touch, gas on each sensor, then clean air until MAINTENANCE ends.
 *
 * The clean value changes by 2 LSB half-way, so no channel is seen as stuck.
 */
static void maintenance_with_gas(uint16_t mq5_raw, uint16_t mq2_raw)
{
    fw_ticks(HOLD_LONG_TICKS, false, true);
    TEST_ASSERT_EQUAL_INT(DET_STATE_MAINTENANCE, firmware_state(&fw));
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_MQ5, mq5_raw));
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_MQ2, mq2_raw));
    fw_ticks(20u, false, false);
    set_gas_raw(CLEAN_RAW_PLUS_2);
    fw_ticks(MAINT_TICKS / 2u, false, false);
    set_gas_raw(CLEAN_RAW);
    fw_ticks(MAINT_TICKS / 2u, false, false);
    TEST_ASSERT_EQUAL_INT(DET_STATE_NORMAL, firmware_state(&fw));
}

/* @verifies REQ-LOG-001 REQ-HMI-010 */
static void test_firmware_maintenance_end_logs_the_peak_of_each_channel(void)
{
    boot_and_reach_normal();
    maintenance_with_gas(MQ5_ALARM_1_RAW, MQ2_ALARM_2_RAW);
    /* MQ-5: 15 %LEL, MQ-2: 30 %LEL, each after its zero offset. */
    TEST_ASSERT_EQUAL_UINT8(1u, count_entries(EVENTLOG_MAINT_PEAK, peak_arg(FIRMWARE_CHANNEL_MQ5, ALARM_1_RAW)));
    TEST_ASSERT_EQUAL_UINT8(1u, count_entries(EVENTLOG_MAINT_PEAK, peak_arg(FIRMWARE_CHANNEL_MQ2, ALARM_2_RAW)));
}

/* @verifies REQ-LOG-001 */
static void test_firmware_each_maintenance_starts_a_new_peak(void)
{
    boot_and_reach_normal();
    maintenance_with_gas(MQ5_ALARM_1_RAW, MQ2_ALARM_2_RAW);
    /* Second test, the MQ-2 only: the MQ-5 peak of the first test is not carried over. */
    maintenance_with_gas(CLEAN_RAW, MQ2_ALARM_1_RAW);
    TEST_ASSERT_EQUAL_UINT8(1u, count_entries(EVENTLOG_MAINT_PEAK, peak_arg(FIRMWARE_CHANNEL_MQ5, ALARM_1_RAW)));
    TEST_ASSERT_EQUAL_UINT8(1u, count_entries(EVENTLOG_MAINT_PEAK, peak_arg(FIRMWARE_CHANNEL_MQ2, ALARM_1_RAW)));
    TEST_ASSERT_EQUAL_UINT8(4u, count_entries(EVENTLOG_MAINT_PEAK, ANY_ARG));
}

/* @verifies REQ-LOG-001 REQ-SAF-008 */
static void test_firmware_peaks_are_logged_one_per_sample_after_the_mode_change(void)
{
    uint32_t guard = 0u;

    boot_and_reach_normal();
    fw_ticks(HOLD_LONG_TICKS, false, true);
    TEST_ASSERT_EQUAL_INT(DET_STATE_MAINTENANCE, firmware_state(&fw));
    while ((firmware_state(&fw) == DET_STATE_MAINTENANCE) && (guard < (2u * MAINT_TICKS))) {
        set_gas_raw(((guard % 2u) == 0u) ? CLEAN_RAW : CLEAN_RAW_PLUS_2);
        fw_tick(false, false);
        guard++;
    }
    TEST_ASSERT_EQUAL_INT(DET_STATE_NORMAL, firmware_state(&fw));
    /* The sample that ends MAINTENANCE writes only its MODE entry; each next sample writes one PEAK. */
    TEST_ASSERT_EQUAL_UINT8(0u, count_entries(EVENTLOG_MAINT_PEAK, ANY_ARG));
    fw_tick(false, false);
    TEST_ASSERT_EQUAL_UINT8(1u, count_entries(EVENTLOG_MAINT_PEAK, ANY_ARG));
    fw_tick(false, false);
    TEST_ASSERT_EQUAL_UINT8(2u, count_entries(EVENTLOG_MAINT_PEAK, ANY_ARG));
}

/* @verifies REQ-LOG-001 REQ-SAF-007 */
static void test_firmware_a_failed_peak_write_does_not_stop_the_sample(void)
{
    uint32_t guard = 0u;

    boot_and_reach_normal();
    fw_ticks(HOLD_LONG_TICKS, false, true);
    while ((firmware_state(&fw) == DET_STATE_MAINTENANCE) && (guard < (2u * MAINT_TICKS))) {
        set_gas_raw(((guard % 2u) == 0u) ? CLEAN_RAW : CLEAN_RAW_PLUS_2);
        fw_tick(false, false);
        guard++;
    }
    hal_eeprom_fake_fail_writes(true);
    /* The PEAK writes fail: each sample still returns GG_OK (checked in fw_tick). */
    fw_ticks(3u, false, false);
    hal_eeprom_fake_fail_writes(false);
    TEST_ASSERT_EQUAL_UINT8(0u, count_entries(EVENTLOG_MAINT_PEAK, ANY_ARG));
    TEST_ASSERT_EQUAL_INT(DET_STATE_NORMAL, firmware_state(&fw));
}

/* @verifies REQ-LOG-001 REQ-LOG-003 */
static void test_firmware_no_peak_is_logged_outside_maintenance(void)
{
    boot_and_reach_normal();
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_MQ5, MQ5_ALARM_1_RAW));
    fw_ticks(20u, false, false);
    TEST_ASSERT_EQUAL_UINT8(0u, count_entries(EVENTLOG_MAINT_PEAK, ANY_ARG));
}

/** @brief Raw value of the temperature input at about 25 degC (Grove Temperature Sensor v1.2, buffered output). */
#define TEMP_ROOM_RAW 512u
/** @brief Raw values just below 0 degC and just under 50 degC (about -0.7 and +48.8 degC, bench conversion). */
#define TEMP_MINUS_1_RAW 210u
#define TEMP_PLUS_49_RAW 760u
/** @brief Raw value of the temperature input below 0 degC: valid for the sensor, out of the range of a room. */
#define TEMP_BELOW_ZERO_RAW 190u

/* @verifies REQ-FUN-004 REQ-LOG-005 */
static void test_firmware_status_reports_the_raw_inputs_and_a_valid_temperature(void)
{
    cli_status_t snap;

    boot_and_reach_normal();
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_TEMP, TEMP_ROOM_RAW));
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_MQ2, CLEAN_RAW_PLUS_2));
    fw_tick(false, false);
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_status(&fw, &snap));
    TEST_ASSERT_EQUAL_UINT8((uint8_t)DET_STATE_NORMAL, snap.state);
    TEST_ASSERT_EQUAL_UINT16(CLEAN_RAW, snap.raw_mq5);
    TEST_ASSERT_EQUAL_UINT16(CLEAN_RAW_PLUS_2, snap.raw_mq2);
    TEST_ASSERT_EQUAL_UINT16(TEMP_ROOM_RAW, snap.raw_temp);
    TEST_ASSERT_TRUE(snap.temp_valid);
}

/* @verifies REQ-FUN-004 */
static void test_firmware_temperature_outside_its_range_or_unread_is_invalid(void)
{
    cli_status_t snap;

    boot_and_reach_normal();
    /* Thermistor open: the 100 kOhm resistor of the module pulls the output near 0. */
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_TEMP, FIRMWARE_TEMP_RAW_MIN - 1u));
    fw_tick(false, false);
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_status(&fw, &snap));
    TEST_ASSERT_FALSE(snap.temp_valid);
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_TEMP, FIRMWARE_TEMP_RAW_MAX + 1u));
    fw_tick(false, false);
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_status(&fw, &snap));
    TEST_ASSERT_FALSE(snap.temp_valid);
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_TEMP, TEMP_ROOM_RAW));
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_fail_channel(FIRMWARE_ADC_TEMP, true));
    fw_tick(false, false);
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_status(&fw, &snap));
    TEST_ASSERT_FALSE(snap.temp_valid);
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_fail_channel(FIRMWARE_ADC_TEMP, false));
    /* A faulty temperature input is never a gas fault: the detector stays in NORMAL. */
    TEST_ASSERT_EQUAL_INT(DET_STATE_NORMAL, firmware_state(&fw));
}

/* @verifies REQ-FUN-004 */
static void test_firmware_valid_temperature_does_not_move_the_gas_reading_while_the_coefficient_is_zero(void)
{
    cli_status_t cold;
    cli_status_t warm;

    boot_and_reach_normal();
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_TEMP, FIRMWARE_TEMP_RAW_MIN));
    fw_ticks(10u, false, false);
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_status(&fw, &cold));
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_TEMP, FIRMWARE_TEMP_RAW_MAX));
    fw_ticks(10u, false, false);
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_status(&fw, &warm));
    TEST_ASSERT_TRUE(cold.temp_valid && warm.temp_valid);
    TEST_ASSERT_EQUAL_INT16(cold.conc, warm.conc);
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, firmware_status(NULL, &warm));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, firmware_status(&fw, NULL));
}

/** @brief MQ-5 above AL2 with the temperature input in a given state: ALARM_2 and the relay off. */
static void expect_alarm_2_with_temperature(uint16_t temp_raw, bool temp_fails)
{
    bool energised = true;

    boot_and_reach_normal();
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_TEMP, temp_raw));
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_fail_channel(FIRMWARE_ADC_TEMP, temp_fails));
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_MQ5, MQ5_ALARM_2_RAW));
    fw_ticks(10u, false, false);
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_2, firmware_state(&fw));
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_relay_energised(&fw, &energised));
    TEST_ASSERT_FALSE(energised);
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_fail_channel(FIRMWARE_ADC_TEMP, false));
}

/* @verifies REQ-FUN-004 REQ-ALM-001 */
static void test_firmware_faulty_temperature_never_holds_back_an_alarm(void)
{
    /* Read error, open thermistor, shorted thermistor: the alarm comes all the same. */
    expect_alarm_2_with_temperature(TEMP_ROOM_RAW, true);
    expect_alarm_2_with_temperature(FIRMWARE_TEMP_RAW_MIN - 1u, false);
    expect_alarm_2_with_temperature(FIRMWARE_TEMP_RAW_MAX + 1u, false);
}

/* @verifies REQ-FUN-004 */
static void test_firmware_unplugged_or_freezing_temperature_is_invalid(void)
{
    cli_status_t snap;

    boot_and_reach_normal();
    /* Unplugged: the pull-down on A3 (HW-005) holds the input at 0. */
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_TEMP, 0u));
    fw_tick(false, false);
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_status(&fw, &snap));
    TEST_ASSERT_FALSE(snap.temp_valid);
    /* Below 0 degC: no room the detector protects is that cold, so the reading is not trusted. */
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_TEMP, TEMP_BELOW_ZERO_RAW));
    fw_tick(false, false);
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_status(&fw, &snap));
    TEST_ASSERT_FALSE(snap.temp_valid);
}

/* @verifies REQ-FUN-004 */
static void test_firmware_temperature_range_is_zero_to_fifty_degrees(void)
{
    cli_status_t snap;

    boot_and_reach_normal();
    /* About -1 degC: below the range of a room, not trusted. */
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_TEMP, TEMP_MINUS_1_RAW));
    fw_tick(false, false);
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_status(&fw, &snap));
    TEST_ASSERT_FALSE(snap.temp_valid);
    /* About +49 degC: still a valid room temperature. */
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_TEMP, TEMP_PLUS_49_RAW));
    fw_tick(false, false);
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_status(&fw, &snap));
    TEST_ASSERT_TRUE(snap.temp_valid);
}

/** @brief Read the reset entry written by a boot with this cause (first slot of a blank log). */
static uint16_t reset_arg_after_boot(hal_reset_cause_t cause)
{
    eventlog_entry_t entry;

    hal_eeprom_fake_reset();
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_boot(&fw, cause, 0u));
    TEST_ASSERT_EQUAL_INT(GG_OK, eventlog_read_slot(0u, &entry));
    TEST_ASSERT_EQUAL_INT(EVENTLOG_RESET, entry.code);
    return entry.arg;
}

/* @verifies REQ-SAF-006 */
static void test_firmware_watchdog_and_brown_out_resets_are_logged_with_their_cause(void)
{
    TEST_ASSERT_EQUAL_UINT16((uint16_t)HAL_RESET_WATCHDOG, reset_arg_after_boot(HAL_RESET_WATCHDOG));
    TEST_ASSERT_EQUAL_UINT16((uint16_t)HAL_RESET_BROWN_OUT, reset_arg_after_boot(HAL_RESET_BROWN_OUT));
}

/* @verifies REQ-SAF-006 */
static void test_firmware_every_reset_cause_is_logged_even_the_unknown_one(void)
{
    TEST_ASSERT_EQUAL_UINT16((uint16_t)HAL_RESET_EXTERNAL, reset_arg_after_boot(HAL_RESET_EXTERNAL));
    /* Several flags or none: logged as unknown, never dropped (a hidden watchdog reset). */
    TEST_ASSERT_EQUAL_UINT16((uint16_t)HAL_RESET_UNKNOWN, reset_arg_after_boot(HAL_RESET_UNKNOWN));
}

/* @verifies REQ-SAF-004 */
static void test_firmware_bandgap_is_measured_after_60_s_not_before(void)
{
    boot_and_reach_normal();
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_health(&fw, now_test));
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_bandgap(150u));
    fw_ticks((FIRMWARE_BANDGAP_PERIOD_MS / TICK_MS) - 1u, false, false);
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_health(&fw, now_test));   /* 59.9 s: not due yet. */
    fw_tick(false, false);
    TEST_ASSERT_EQUAL_INT(DET_STATE_NORMAL, firmware_state(&fw));
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_health(&fw, now_test));   /* 60 s: measured, out of tolerance. */
    fw_tick(false, false);
    TEST_ASSERT_EQUAL_INT(DET_STATE_FAULT, firmware_state(&fw));
}

/* @verifies REQ-SAF-004 */
static void test_firmware_bandgap_at_the_tolerance_edges_raises_no_fault(void)
{
    boot_and_reach_normal();
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_bandgap(DIAG_BANDGAP_NOMINAL - 22u));
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_health(&fw, now_test));
    fw_ticks(FIRMWARE_BANDGAP_PERIOD_MS / TICK_MS, false, false);
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_bandgap(DIAG_BANDGAP_NOMINAL + 22u));
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_health(&fw, now_test));
    fw_tick(false, false);
    TEST_ASSERT_EQUAL_INT(DET_STATE_NORMAL, firmware_state(&fw));
}

/* @verifies REQ-HMI-008 */
static void test_firmware_touch_of_3_s_is_ignored_and_3_2_s_enters_maintenance(void)
{
    /* The test loop runs every 100 ms: the debounce accepts the touch one step
     * after it starts, the 3 s count from there, and the request is acted on at
     * the next sample. So 30 steps held (3.0 s) are one step short, 32 are enough. */
    boot_and_reach_normal();
    fw_ticks(30u, false, true);
    fw_ticks(3u, false, false);
    TEST_ASSERT_EQUAL_INT(DET_STATE_NORMAL, firmware_state(&fw));
    fw_ticks(32u, false, true);
    fw_tick(false, false);
    TEST_ASSERT_EQUAL_INT(DET_STATE_MAINTENANCE, firmware_state(&fw));
}

/* @verifies REQ-HMI-008 */
static void test_firmware_touch_during_an_alarm_does_not_hide_it(void)
{
    boot_and_reach_normal();
    set_gas_level(ALARM_1_RAW);
    fw_ticks(10u, false, false);
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_1, firmware_state(&fw));
    fw_ticks(HOLD_LONG_TICKS, false, true);
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_1, firmware_state(&fw));
}

/**
 * @brief n loop steps in clean air that moves by 2 LSB every step, as a live sensor does.
 *
 * Ten minutes of a perfectly flat value would trip the stuck-sensor check
 * (REQ-SAF-003) and hide what the test looks at.
 */
static void fw_ticks_live_air(uint32_t n, bool touch)
{
    uint32_t i;

    for (i = 0u; i < n; i++) {
        set_gas_raw(((i % 2u) == 0u) ? CLEAN_RAW : CLEAN_RAW_PLUS_2);
        fw_tick(false, touch);
    }
}

/* @verifies REQ-HMI-010 */
static void test_firmware_maintenance_ends_after_ten_minutes_not_before(void)
{
    boot_and_reach_normal();
    fw_ticks(HOLD_LONG_TICKS, false, true);
    TEST_ASSERT_EQUAL_INT(DET_STATE_MAINTENANCE, firmware_state(&fw));
    fw_ticks_live_air((FIRMWARE_MAINT_MAX_MS / TICK_MS) - 20u, false);   /* About 598 s after the start. */
    TEST_ASSERT_EQUAL_INT(DET_STATE_MAINTENANCE, firmware_state(&fw));
    fw_ticks_live_air(40u, false);
    TEST_ASSERT_EQUAL_INT(DET_STATE_NORMAL, firmware_state(&fw));
}

/* @verifies REQ-HMI-010 */
static void test_firmware_touch_held_during_maintenance_does_not_extend_it(void)
{
    /* A finger left on the sensor (or a stuck touch input) must not keep the
     * alarms inhibited past 10 minutes. */
    boot_and_reach_normal();
    fw_ticks(HOLD_LONG_TICKS, false, true);
    fw_ticks_live_air((FIRMWARE_MAINT_MAX_MS / TICK_MS) + 20u, true);
    TEST_ASSERT_EQUAL_INT(DET_STATE_NORMAL, firmware_state(&fw));
}

/* @verifies REQ-FUN-004 */
static void test_firmware_temperature_valid_range_ends_exactly_at_217_and_769(void)
{
    cli_status_t snap;

    boot_and_reach_normal();
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_TEMP, FIRMWARE_TEMP_RAW_MIN));
    fw_tick(false, false);
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_status(&fw, &snap));
    TEST_ASSERT_TRUE(snap.temp_valid);
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_TEMP, FIRMWARE_TEMP_RAW_MIN - 1u));
    fw_tick(false, false);
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_status(&fw, &snap));
    TEST_ASSERT_FALSE(snap.temp_valid);
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_TEMP, FIRMWARE_TEMP_RAW_MAX));
    fw_tick(false, false);
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_status(&fw, &snap));
    TEST_ASSERT_TRUE(snap.temp_valid);
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_TEMP, FIRMWARE_TEMP_RAW_MAX + 1u));
    fw_tick(false, false);
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_status(&fw, &snap));
    TEST_ASSERT_FALSE(snap.temp_valid);
}

/* @verifies REQ-FUN-001 */
static void test_firmware_each_sample_reads_both_gas_channels(void)
{
    cli_status_t snap;

    boot_and_reach_normal();
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_MQ5, 400u));
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_MQ2, 300u));
    fw_tick(false, false);
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_status(&fw, &snap));
    TEST_ASSERT_EQUAL_UINT16(400u, snap.raw_mq5);
    TEST_ASSERT_EQUAL_UINT16(300u, snap.raw_mq2);
    /* The next sample sees new values on both channels at once, none is skipped. */
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_MQ5, 410u));
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_MQ2, 310u));
    fw_tick(false, false);
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_status(&fw, &snap));
    TEST_ASSERT_EQUAL_UINT16(410u, snap.raw_mq5);
    TEST_ASSERT_EQUAL_UINT16(310u, snap.raw_mq2);
}

/* @verifies REQ-SAF-009 */
static void test_firmware_low_stack_during_warmup_is_a_fault(void)
{
    boot_defaults();
    set_gas_raw(CLEAN_RAW);
    fw_ticks(10u, false, false);
    hal_stack_fake_set_free(FIRMWARE_STACK_MIN_FREE - 1u);
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_health(&fw, now_test));
    fw_tick(false, false);
    TEST_ASSERT_EQUAL_INT(DET_STATE_FAULT, firmware_state(&fw));
}

/* @verifies REQ-SAF-009 */
static void test_firmware_low_stack_during_maintenance_is_a_fault(void)
{
    boot_and_reach_normal();
    fw_ticks(HOLD_LONG_TICKS, false, true);
    TEST_ASSERT_EQUAL_INT(DET_STATE_MAINTENANCE, firmware_state(&fw));
    hal_stack_fake_set_free(0u);
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_health(&fw, now_test));
    fw_tick(false, false);
    TEST_ASSERT_EQUAL_INT(DET_STATE_FAULT, firmware_state(&fw));
}

/* @verifies REQ-SAF-009 */
static void test_firmware_plenty_of_stack_never_faults(void)
{
    boot_and_reach_normal();
    hal_stack_fake_set_free(UINT16_MAX);
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_health(&fw, now_test));
    fw_tick(false, false);
    TEST_ASSERT_EQUAL_INT(DET_STATE_NORMAL, firmware_state(&fw));
}

/* @verifies REQ-HMI-009 */
static void test_firmware_gas_is_shown_during_maintenance_but_not_output(void)
{
    bool energised = false;

    boot_and_reach_normal();
    fw_ticks(HOLD_LONG_TICKS, false, true);
    set_gas_level(ALARM_2_RAW);
    fw_ticks(10u, false, false);
    TEST_ASSERT_EQUAL_INT(DET_STATE_MAINTENANCE, firmware_state(&fw));
    TEST_ASSERT_EQUAL_INT16((gg_conc_t)ALARM_2_RAW, firmware_concentration(&fw));
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_relay_energised(&fw, &energised));
    TEST_ASSERT_TRUE(energised);
    TEST_ASSERT_FALSE(hal_gpio_fake_level(HAL_GPIO_PIN_BUZZER));
}

/* @verifies REQ-HMI-005 REQ-HMI-006 */
static void test_firmware_long_press_in_alarm_1_neither_resets_nor_starts_the_test(void)
{
    boot_and_reach_normal();
    set_gas_level(ALARM_1_RAW);
    fw_ticks(10u, false, false);
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_1, firmware_state(&fw));
    fw_ticks(HOLD_LONG_TICKS, true, false);
    fw_tick(false, false);
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_1, firmware_state(&fw));
    TEST_ASSERT_FALSE(firmware_test_running(&fw));
}

/* @verifies REQ-HMI-006 */
static void test_firmware_long_press_in_fault_never_energises_the_relay(void)
{
    uint32_t i;

    /* Both channels shorted: FAULT. A long press must not run a test that
     * would put the relay back on the healthy side. */
    boot_and_reach_normal();
    set_gas_raw(1020u);
    fw_ticks(10u, false, false);
    TEST_ASSERT_EQUAL_INT(DET_STATE_FAULT, firmware_state(&fw));
    for (i = 0u; i < (HOLD_LONG_TICKS + 80u); i++) {
        fw_tick(i < HOLD_LONG_TICKS, false);
        TEST_ASSERT_FALSE(hal_gpio_fake_level(HAL_GPIO_PIN_RELAY));
    }
    TEST_ASSERT_EQUAL_INT(DET_STATE_FAULT, firmware_state(&fw));
}

/** @brief A short press of 0.3 s, then 0.2 s released (debounced, well under 1 s). */
static void short_press(void)
{
    fw_ticks(3u, true, false);
    fw_ticks(2u, false, false);
}

/** @brief Buzzer pin after an output step at the next start of a 4 Hz "on" phase. */
static bool buzzer_sounds_now(void)
{
    uint32_t t = now_test;

    while ((t % 500u) != 0u) {
        t++;
    }
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_output(&fw, t));
    return hal_gpio_fake_level(HAL_GPIO_PIN_BUZZER);
}

/* @verifies REQ-HMI-004 */
static void test_firmware_short_presses_cannot_keep_alarm_2_quiet_past_60_s(void)
{
    uint8_t i;

    boot_and_reach_normal();
    set_gas_level(ALARM_2_RAW);
    fw_ticks(10u, false, false);
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_2, firmware_state(&fw));
    short_press();
    fw_ticks(95u, false, false);
    TEST_ASSERT_FALSE(buzzer_sounds_now());   /* About 10 s after the press: silenced. */
    /* A press every 10 s for 80 s: the silence must still end 60 s after the first one. */
    for (i = 0u; i < 7u; i++) {
        short_press();
        fw_ticks(95u, false, false);
    }
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_2, firmware_state(&fw));
    TEST_ASSERT_TRUE(buzzer_sounds_now());
}

/* @verifies REQ-HMI-004 REQ-SAF-007 */
static void test_firmware_alarm_2_back_from_a_stuck_fault_gets_a_new_silence(void)
{
    /* Decision of the author (2026-11-17): a change of state, FAULT included,
     * starts a new silence. ALARM_2, its 60 s used, then 10 min of flat sensors
     * (stuck-only FAULT), then a sensor moves: ALARM_2 again, and a press gives
     * 60 s of silence again. The relay stays off all along. */
    bool energised = true;

    boot_and_reach_normal();
    set_gas_level(ALARM_2_RAW);
    fw_ticks(10u, false, false);
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_2, firmware_state(&fw));
    short_press();
    fw_ticks(700u, false, false);   /* 70 s: the 60 s are used. */
    TEST_ASSERT_TRUE(buzzer_sounds_now());
    fw_ticks(6000u, false, false);  /* 10 more minutes of a flat reading. */
    TEST_ASSERT_EQUAL_INT(DET_STATE_FAULT, firmware_state(&fw));
    set_gas_level(ALARM_2_RAW + 5u);
    fw_ticks(2u, false, false);
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_2, firmware_state(&fw));
    short_press();
    fw_ticks(95u, false, false);
    TEST_ASSERT_FALSE(buzzer_sounds_now());
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_relay_energised(&fw, &energised));
    TEST_ASSERT_FALSE(energised);
}

/* @verifies REQ-FUN-002 REQ-SAF-007 */
static void test_firmware_spike_that_ends_a_stuck_fault_raises_no_alarm(void)
{
    /* Both channels flat for 10 min: stuck-only FAULT. Then one wild sample
     * (1017, just below the short-circuit rail) on the MQ-5, then clean air.
     * The spike ends the stuck fault, but it must not reach the alarm logic. */
    uint8_t i;

    boot_and_reach_normal();
    fw_ticks(6100u, false, false);
    TEST_ASSERT_EQUAL_INT(DET_STATE_FAULT, firmware_state(&fw));
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_MQ5, 1017u));
    fw_tick(false, false);
    for (i = 0u; i < 20u; i++) {
        set_gas_raw(((i % 2u) == 0u) ? CLEAN_RAW : CLEAN_RAW_PLUS_2);
        fw_tick(false, false);
        TEST_ASSERT_TRUE(firmware_state(&fw) != DET_STATE_ALARM_1);
        TEST_ASSERT_TRUE(firmware_state(&fw) != DET_STATE_ALARM_2);
    }
    TEST_ASSERT_EQUAL_INT(DET_STATE_NORMAL, firmware_state(&fw));
}

/* @verifies REQ-FUN-002 REQ-SAF-003 */
static void test_firmware_spike_on_a_channel_coming_back_from_stuck_raises_no_alarm(void)
{
    /* Only the MQ-5 is flat (the MQ-2 moves): the MQ-5 alone is left out. One
     * wild sample on it brings it back; it must not raise an alarm. */
    uint32_t i;

    boot_and_reach_normal();
    for (i = 0u; i < 6100u; i++) {
        TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_MQ2,
                                                            ((i % 2u) == 0u) ? CLEAN_RAW : CLEAN_RAW_PLUS_2));
        fw_tick(false, false);
    }
    TEST_ASSERT_EQUAL_INT(DET_STATE_NORMAL, firmware_state(&fw));
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_MQ5, 1017u));
    fw_tick(false, false);
    for (i = 0u; i < 20u; i++) {
        set_gas_raw(((i % 2u) == 0u) ? CLEAN_RAW : CLEAN_RAW_PLUS_2);
        fw_tick(false, false);
    }
    TEST_ASSERT_EQUAL_INT(DET_STATE_NORMAL, firmware_state(&fw));
}

/* @verifies REQ-FUN-002 REQ-SAF-002 */
static void test_firmware_spike_right_after_a_rail_reading_raises_no_alarm(void)
{
    /* A bouncing contact: one reading at the short-circuit rail, then one wild
     * reading just below it (1017), then clean air. The rail reading is left out
     * and must not wipe the filter history, so the median still removes 1017. */
    uint8_t i;

    boot_and_reach_normal();
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_MQ5, DIAG_SHORT_MIN));
    fw_tick(false, false);
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_MQ5, 1017u));
    fw_tick(false, false);
    for (i = 0u; i < 20u; i++) {
        set_gas_raw(CLEAN_RAW);
        fw_tick(false, false);
        TEST_ASSERT_TRUE(firmware_state(&fw) != DET_STATE_ALARM_1);
        TEST_ASSERT_TRUE(firmware_state(&fw) != DET_STATE_ALARM_2);
    }
    TEST_ASSERT_EQUAL_INT(DET_STATE_NORMAL, firmware_state(&fw));
}

/* @verifies REQ-SAF-002 REQ-FUN-002 */
static void test_firmware_gas_during_range_recovery_is_ignored_then_seen_at_recovery(void)
{
    /* The MQ-5 reads open circuit long enough for a range fault, then reads gas
     * above AL2 while it recovers. Until the 600 valid samples are done it stays
     * out (negative case); from the recovery sample on, its filter already holds
     * the gas, so ALARM_2 follows after the usual 3 samples (no hidden gas). */
    uint32_t i;

    boot_and_reach_normal();
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_MQ5, 0u));
    fw_ticks(DIAG_RANGE_SAMPLES + 1u, false, false);
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_MQ5, MQ5_ALARM_2_RAW));
    for (i = 0u; i < (DIAG_RECOVER_SAMPLES - 1u); i++) {
        TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_MQ2,
                                                            ((i % 2u) == 0u) ? CLEAN_RAW : CLEAN_RAW_PLUS_2));
        fw_tick(false, false);
        TEST_ASSERT_TRUE(firmware_state(&fw) != DET_STATE_ALARM_1);
        TEST_ASSERT_TRUE(firmware_state(&fw) != DET_STATE_ALARM_2);
    }
    fw_ticks(DETECTOR_ENTER_SAMPLES, false, false);
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_2, firmware_state(&fw));
}

/* @verifies REQ-FUN-002 */
static void test_firmware_spike_right_after_a_read_error_raises_no_alarm(void)
{
    /* One failed read on the MQ-5, then one wild reading (1017), then clean air.
     * The failed read is left out and must not wipe the filter history, so the
     * median still removes 1017. */
    uint8_t i;

    boot_and_reach_normal();
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_fail_channel(FIRMWARE_ADC_MQ5, true));
    fw_tick(false, false);
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_fail_channel(FIRMWARE_ADC_MQ5, false));
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_MQ5, 1017u));
    fw_tick(false, false);
    for (i = 0u; i < 20u; i++) {
        set_gas_raw(CLEAN_RAW);
        fw_tick(false, false);
        TEST_ASSERT_TRUE(firmware_state(&fw) != DET_STATE_ALARM_1);
        TEST_ASSERT_TRUE(firmware_state(&fw) != DET_STATE_ALARM_2);
    }
    TEST_ASSERT_EQUAL_INT(DET_STATE_NORMAL, firmware_state(&fw));
}

/* @verifies REQ-HMI-008 */
static void test_firmware_touch_refused_for_gas_and_still_held_never_enters_maintenance(void)
{
    /* The touch is held from clean air. Gas exactly at AL1 arrives so that the
     * filtered value reaches AL1 on the sample that takes the request (32nd step,
     * see the 3.2 s test) and on the next one only: no alarm, request refused.
     * The gas then goes and the touch is still held: one touch asks once, so
     * MAINTENANCE never starts. */
    bool energised = false;

    boot_and_reach_normal();
    fw_ticks(24u, false, true);
    set_gas_level((uint16_t)DETECTOR_AL1_DEFAULT);
    fw_ticks(9u, false, true);
    TEST_ASSERT_TRUE(firmware_concentration(&fw) >= DETECTOR_AL1_DEFAULT);
    TEST_ASSERT_EQUAL_INT(DET_STATE_NORMAL, firmware_state(&fw));
    set_gas_raw(CLEAN_RAW);
    fw_ticks(20u, false, true);
    TEST_ASSERT_EQUAL_INT(DET_STATE_NORMAL, firmware_state(&fw));
    fw_tick(false, false);
    TEST_ASSERT_EQUAL_INT(DET_STATE_NORMAL, firmware_state(&fw));
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_relay_energised(&fw, &energised));
    TEST_ASSERT_TRUE(energised);
}

/* @verifies REQ-LOG-001 */
static void test_firmware_touch_right_after_maintenance_keeps_its_peaks(void)
{
    /* The touch that starts MAINTENANCE is taken on step 31 and MAINTENANCE
     * starts on step 32 (see the 3.2 s test), so it ends on step 31 + 6000.
     * A second touch held from step 6000 gives its 3 s on that same step 6031,
     * right after the end, while the peaks of the gas test are still waiting
     * to be logged. They must be logged as measured, not as 0. */
    boot_and_reach_normal();
    fw_ticks(HOLD_LONG_TICKS, false, true);
    TEST_ASSERT_EQUAL_INT(DET_STATE_MAINTENANCE, firmware_state(&fw));
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_MQ5, MQ5_ALARM_1_RAW));
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_MQ2, MQ2_ALARM_2_RAW));
    fw_ticks(20u, false, false);
    set_gas_raw(CLEAN_RAW_PLUS_2);
    fw_ticks(2945u, false, false);
    set_gas_raw(CLEAN_RAW);
    fw_ticks(3000u, false, false);
    fw_ticks(40u, false, true);
    fw_ticks(3u, false, false);
    /* The second touch did start a new MAINTENANCE: it came after the end. */
    TEST_ASSERT_EQUAL_INT(DET_STATE_MAINTENANCE, firmware_state(&fw));
    TEST_ASSERT_EQUAL_UINT8(1u, count_entries(EVENTLOG_MAINT_PEAK, peak_arg(FIRMWARE_CHANNEL_MQ5, ALARM_1_RAW)));
    TEST_ASSERT_EQUAL_UINT8(1u, count_entries(EVENTLOG_MAINT_PEAK, peak_arg(FIRMWARE_CHANNEL_MQ2, ALARM_2_RAW)));
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
    RUN_TEST(test_firmware_each_channel_subtracts_its_own_zero);
    RUN_TEST(test_firmware_mq5_below_its_zero_reads_zero);
    RUN_TEST(test_firmware_mq5_alone_above_al2_raises_alarm_2);
    RUN_TEST(test_firmware_concentration_is_the_last_sample_for_the_display);
    RUN_TEST(test_firmware_both_channels_short_raise_fault_and_drop_the_relay);
    RUN_TEST(test_firmware_touch_for_three_seconds_enters_maintenance);
    RUN_TEST(test_firmware_touch_shorter_than_three_seconds_is_ignored);
    RUN_TEST(test_firmware_short_press_silences_the_alarm_1_buzzer);
    RUN_TEST(test_firmware_a_new_alarm_1_sounds_after_a_silenced_one);
    RUN_TEST(test_firmware_long_press_in_alarm_2_resets_the_latch_when_gas_is_gone);
    RUN_TEST(test_firmware_long_press_in_normal_runs_the_functional_test);
    RUN_TEST(test_firmware_overrun_is_written_to_the_event_log);
    RUN_TEST(test_firmware_three_overruns_latch_fault);
    RUN_TEST(test_firmware_stack_below_64_bytes_latches_fault);
    RUN_TEST(test_firmware_frozen_channel_is_excluded_after_ten_minutes);
    RUN_TEST(test_firmware_both_channels_stuck_give_a_fault_that_ends_by_itself);
    RUN_TEST(test_firmware_stuck_channel_with_the_other_open_latches_the_fault);
    RUN_TEST(test_firmware_adc_error_during_a_stuck_fault_latches_it);
    RUN_TEST(test_firmware_gas_ending_a_stuck_fault_gives_the_alarm_with_the_usual_confirmation);
    RUN_TEST(test_firmware_stuck_fault_episode_is_logged);
    RUN_TEST(test_firmware_bandgap_out_of_tolerance_raises_fault);
    RUN_TEST(test_firmware_bandgap_out_of_tolerance_logs_its_raw_value_once);
    RUN_TEST(test_firmware_bandgap_within_tolerance_logs_nothing);
    RUN_TEST(test_firmware_failed_bandgap_read_is_logged_as_zero_and_raises_fault);
    RUN_TEST(test_firmware_bandgap_fault_latches_even_when_the_log_write_fails);
    RUN_TEST(test_firmware_bandgap_is_logged_again_after_a_reset);
    RUN_TEST(test_firmware_alarm_2_buzzer_beeps_four_times_a_second_at_the_output_rate);
    RUN_TEST(test_firmware_fourth_short_start_enters_fault_and_logs_it);
    RUN_TEST(test_firmware_later_short_starts_stay_in_fault_and_log_each_start);
    RUN_TEST(test_firmware_sixty_seconds_after_warmup_clear_the_start_count);
    RUN_TEST(test_firmware_reset_loop_fault_clears_after_a_full_run_and_a_new_start);
    RUN_TEST(test_firmware_corrupted_start_count_is_read_as_zero);
    RUN_TEST(test_firmware_start_count_stays_at_its_maximum_in_a_long_loop);
    RUN_TEST(test_firmware_start_count_clear_follows_the_stored_warmup);
    RUN_TEST(test_firmware_start_count_is_cleared_sixty_seconds_after_warmup_not_before);
    RUN_TEST(test_firmware_failed_clear_is_tried_again_at_the_next_check);
    RUN_TEST(test_firmware_start_counter_fits_after_the_log_and_a_write_error_fails_the_boot);
    RUN_TEST(test_firmware_channel_fault_is_logged_once);
    RUN_TEST(test_firmware_missing_display_is_logged);
    RUN_TEST(test_firmware_channel_recovery_is_logged);
    RUN_TEST(test_firmware_null_arguments_are_rejected);
    RUN_TEST(test_firmware_maintenance_end_logs_the_peak_of_each_channel);
    RUN_TEST(test_firmware_each_maintenance_starts_a_new_peak);
    RUN_TEST(test_firmware_no_peak_is_logged_outside_maintenance);
    RUN_TEST(test_firmware_status_reports_the_raw_inputs_and_a_valid_temperature);
    RUN_TEST(test_firmware_temperature_outside_its_range_or_unread_is_invalid);
    RUN_TEST(test_firmware_valid_temperature_does_not_move_the_gas_reading_while_the_coefficient_is_zero);
    RUN_TEST(test_firmware_faulty_temperature_never_holds_back_an_alarm);
    RUN_TEST(test_firmware_unplugged_or_freezing_temperature_is_invalid);
    RUN_TEST(test_firmware_temperature_range_is_zero_to_fifty_degrees);
    RUN_TEST(test_firmware_peaks_are_logged_one_per_sample_after_the_mode_change);
    RUN_TEST(test_firmware_a_failed_peak_write_does_not_stop_the_sample);
    RUN_TEST(test_firmware_watchdog_and_brown_out_resets_are_logged_with_their_cause);
    RUN_TEST(test_firmware_every_reset_cause_is_logged_even_the_unknown_one);
    RUN_TEST(test_firmware_bandgap_is_measured_after_60_s_not_before);
    RUN_TEST(test_firmware_bandgap_at_the_tolerance_edges_raises_no_fault);
    RUN_TEST(test_firmware_touch_of_3_s_is_ignored_and_3_2_s_enters_maintenance);
    RUN_TEST(test_firmware_touch_during_an_alarm_does_not_hide_it);
    RUN_TEST(test_firmware_maintenance_ends_after_ten_minutes_not_before);
    RUN_TEST(test_firmware_touch_held_during_maintenance_does_not_extend_it);
    RUN_TEST(test_firmware_temperature_valid_range_ends_exactly_at_217_and_769);
    RUN_TEST(test_firmware_each_sample_reads_both_gas_channels);
    RUN_TEST(test_firmware_low_stack_during_warmup_is_a_fault);
    RUN_TEST(test_firmware_low_stack_during_maintenance_is_a_fault);
    RUN_TEST(test_firmware_plenty_of_stack_never_faults);
    RUN_TEST(test_firmware_gas_is_shown_during_maintenance_but_not_output);
    RUN_TEST(test_firmware_long_press_in_alarm_1_neither_resets_nor_starts_the_test);
    RUN_TEST(test_firmware_long_press_in_fault_never_energises_the_relay);
    RUN_TEST(test_firmware_short_presses_cannot_keep_alarm_2_quiet_past_60_s);
    RUN_TEST(test_firmware_alarm_2_back_from_a_stuck_fault_gets_a_new_silence);
    RUN_TEST(test_firmware_spike_that_ends_a_stuck_fault_raises_no_alarm);
    RUN_TEST(test_firmware_spike_on_a_channel_coming_back_from_stuck_raises_no_alarm);
    RUN_TEST(test_firmware_spike_right_after_a_rail_reading_raises_no_alarm);
    RUN_TEST(test_firmware_gas_during_range_recovery_is_ignored_then_seen_at_recovery);
    RUN_TEST(test_firmware_spike_right_after_a_read_error_raises_no_alarm);
    RUN_TEST(test_firmware_touch_refused_for_gas_and_still_held_never_enters_maintenance);
    RUN_TEST(test_firmware_touch_right_after_maintenance_keeps_its_peaks);
    return UNITY_END();
}
