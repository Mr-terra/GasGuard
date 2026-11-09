/*
 * Scenario tests of Day 4 (docs/05-verification-plan.md §3): SC-11, SC-12, SC-14, SC-16.
 * They run the firmware on the host fakes, with the same order as the main loop:
 * sample, operator inputs, outputs. SC-13 and SC-15 are in test_scenarios.c and
 * tests/services/diag/test_diag.c.
 */
#include <stdbool.h>
#include <stdint.h>

#include "unity.h"
#include "common/gg_types/gg_types.h"
#include "app/firmware/firmware.h"
#include "hal/adc/adc_fake.h"
#include "hal/eeprom/eeprom.h"
#include "hal/eeprom/eeprom_fake.h"
#include "hal/gpio/gpio.h"
#include "hal/gpio/gpio_fake.h"
#include "hal/stack/stack_fake.h"
#include "services/config/config.h"
#include "services/eventlog/eventlog.h"
#include "services/scheduler/scheduler.h"

/* DEV-03: Unity hooks, called by the framework; MISRA 8.6 and unusedFunction do not apply. */
// cppcheck-suppress misra-c2012-8.6
void setUp(void);
// cppcheck-suppress misra-c2012-8.6
void tearDown(void);

#define SC_CLEAN_RAW 20u
#define SC_TICK_MS 100u
#define SC_TOUCH_TICKS 35u
#define SC_WARMUP_TICKS 1210u
#define SC_MAINT_CHECK_MS 599000u
#define SC_MAINT_END_MS 601000u
#define SC_OVERRUN_CYCLE_MS 150u

static firmware_t d4_fw;
static uint32_t sc_now;

void setUp(void)
{
    hal_eeprom_fake_reset();
    hal_adc_fake_reset();
    hal_gpio_fake_reset();
    hal_stack_fake_reset();
    (void)hal_gpio_init();
    sc_now = 0u;
}

void tearDown(void) {}

/** @brief One pass of the main loop at 100 ms, with the touch and button pressed or not. */
static void d4_tick(bool touch_pressed)
{
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_sample(&d4_fw, sc_now));
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_inputs(&d4_fw, sc_now, true, touch_pressed));
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_output(&d4_fw, sc_now));
    sc_now += SC_TICK_MS;
}

/** @brief Boot the firmware and set the gas channels to clean air. */
static void boot_clean_air(void)
{
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_boot(&d4_fw, HAL_RESET_POWER_ON, sc_now));
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_MQ5, SC_CLEAN_RAW));
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_ADC_MQ2, SC_CLEAN_RAW));
}

/** @brief Write a damaged copy of the configuration: every byte 0xFF fails the CRC. */
static void corrupt_copy(uint16_t addr)
{
    uint8_t junk[CONFIG_COPY_SIZE];
    uint8_t i;

    for (i = 0u; i < CONFIG_COPY_SIZE; i++) {
        junk[i] = 0xFFu;
    }
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_eeprom_write(addr, junk, CONFIG_COPY_SIZE));
}

/* @verifies REQ-CFG-002 */
static void test_sc11_config_a_corrupted_uses_b_repairs_a_and_logs_it(void)
{
    config_t cfg;
    eventlog_entry_t entry;

    TEST_ASSERT_EQUAL_INT(GG_OK, config_defaults(&cfg));
    cfg.al1 = 120;
    TEST_ASSERT_EQUAL_INT(GG_OK, config_store(&cfg));
    corrupt_copy(CONFIG_ADDR_A);

    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_boot(&d4_fw, HAL_RESET_POWER_ON, 0u));
    /* Slot 0: reset, slot 1: the configuration was taken from copy B. */
    TEST_ASSERT_EQUAL_INT(GG_OK, eventlog_read_slot(1u, &entry));
    TEST_ASSERT_EQUAL_INT(EVENTLOG_CONFIG, entry.code);
    TEST_ASSERT_EQUAL_UINT16((uint16_t)CONFIG_SRC_COPY_B, entry.arg);

    /* Copy A was repaired: the next boot uses it and logs no configuration event. */
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_boot(&d4_fw, HAL_RESET_POWER_ON, 0u));
    TEST_ASSERT_EQUAL_INT(GG_ERR_CRC, eventlog_read_slot(3u, &entry));
}

/* @verifies REQ-CFG-002 */
static void test_sc12_both_copies_corrupted_uses_defaults_and_logs_it(void)
{
    eventlog_entry_t entry;

    corrupt_copy(CONFIG_ADDR_A);
    corrupt_copy(CONFIG_ADDR_B);
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_boot(&d4_fw, HAL_RESET_POWER_ON, 0u));
    TEST_ASSERT_EQUAL_INT(GG_OK, eventlog_read_slot(1u, &entry));
    TEST_ASSERT_EQUAL_INT(EVENTLOG_CONFIG, entry.code);
    TEST_ASSERT_EQUAL_UINT16((uint16_t)CONFIG_SRC_DEFAULTS, entry.arg);
}

/* @verifies REQ-HMI-010 */
static void test_sc14_maintenance_ends_by_itself_after_ten_minutes(void)
{
    uint32_t entered_ms = 0u;
    uint32_t i;

    boot_clean_air();
    for (i = 0u; i < SC_WARMUP_TICKS; i++) {
        d4_tick(false);
    }
    TEST_ASSERT_EQUAL_INT(DET_STATE_NORMAL, firmware_state(&d4_fw));

    for (i = 0u; i < SC_TOUCH_TICKS; i++) {
        d4_tick(true);
    }
    TEST_ASSERT_EQUAL_INT(DET_STATE_MAINTENANCE, firmware_state(&d4_fw));
    entered_ms = sc_now;

    /* Still inhibited just before the 10 minutes. */
    while ((sc_now - entered_ms) < SC_MAINT_CHECK_MS) {
        d4_tick(false);
    }
    TEST_ASSERT_EQUAL_INT(DET_STATE_MAINTENANCE, firmware_state(&d4_fw));

    /* Over the 10 minutes (the 11th minute of the scenario): back to NORMAL. */
    while ((sc_now - entered_ms) < SC_MAINT_END_MS) {
        d4_tick(false);
    }
    TEST_ASSERT_EQUAL_INT(DET_STATE_NORMAL, firmware_state(&d4_fw));
}

/* @verifies REQ-SAF-008 */
static void test_sc16_three_consecutive_overruns_raise_fault(void)
{
    scheduler_t sched;
    uint32_t periods[1] = { 100u };
    uint32_t cycle_start = 0u;
    uint8_t cycle;
    bool due = false;
    bool overrun = false;
    bool fault = false;
    eventlog_entry_t entry;

    boot_clean_air();
    TEST_ASSERT_EQUAL_INT(GG_OK, scheduler_init(&sched, 1u, periods, 0u));
    for (cycle = 0u; cycle < 3u; cycle++) {
        TEST_ASSERT_EQUAL_INT(GG_OK, scheduler_begin_cycle(&sched, cycle_start));
        TEST_ASSERT_EQUAL_INT(GG_OK, scheduler_task_due(&sched, 0u, cycle_start, &due));
        if (due) {
            TEST_ASSERT_EQUAL_INT(GG_OK, scheduler_task_done(&sched, 0u));
        }
        cycle_start += SC_OVERRUN_CYCLE_MS;
        TEST_ASSERT_EQUAL_INT(GG_OK, scheduler_end_cycle(&sched, cycle_start, &overrun, &fault));
        TEST_ASSERT_TRUE(overrun);
        TEST_ASSERT_EQUAL_INT(GG_OK, firmware_report_timing(&d4_fw, overrun, fault, cycle_start));
    }
    TEST_ASSERT_TRUE(fault);
    d4_tick(false);
    TEST_ASSERT_EQUAL_INT(DET_STATE_FAULT, firmware_state(&d4_fw));
    /* Each overrun is in the event log (slots 2, 3 and 4 after the two boot entries). */
    TEST_ASSERT_EQUAL_INT(GG_OK, eventlog_read_slot(4u, &entry));
    TEST_ASSERT_EQUAL_INT(EVENTLOG_OVERRUN, entry.code);
}

// DEV-03: Unity runner entry point, called by the test harness; MISRA 8.6 does not apply.
// cppcheck-suppress misra-c2012-8.6
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_sc11_config_a_corrupted_uses_b_repairs_a_and_logs_it);
    RUN_TEST(test_sc12_both_copies_corrupted_uses_defaults_and_logs_it);
    RUN_TEST(test_sc14_maintenance_ends_by_itself_after_ten_minutes);
    RUN_TEST(test_sc16_three_consecutive_overruns_raise_fault);
    return UNITY_END();
}
