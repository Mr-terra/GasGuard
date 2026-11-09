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
#include "services/eventlog/eventlog.h"

#include <stdbool.h>
#include <stdint.h>

/* DEV-03: Unity hooks, called by the framework; MISRA 8.6 and unusedFunction do not apply. */
// cppcheck-suppress misra-c2012-8.6
void setUp(void);
// cppcheck-suppress misra-c2012-8.6
void tearDown(void);

static firmware_t fw;

void setUp(void)
{
    hal_eeprom_fake_reset();
    hal_adc_fake_reset();
    hal_gpio_fake_reset();
    (void)hal_gpio_init();
}

void tearDown(void) {}

/** @brief Set both gas channels to a raw value that the calibration maps to a concentration. */
static void set_gas_raw(uint16_t raw)
{
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_CHANNEL_MQ5, raw));
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_fake_set_value(FIRMWARE_CHANNEL_MQ2, raw));
}

/** @brief Boot with no stored configuration, so the defaults are used. */
static void boot_defaults(void)
{
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_boot(&fw, HAL_RESET_POWER_ON, 0u));
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

/* @verifies REQ-HMI-001 */
static void test_firmware_boot_starts_in_warmup_with_the_relay_de_energised(void)
{
    bool energised = true;

    boot_defaults();
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_relay_energised(&fw, &energised));
    TEST_ASSERT_FALSE(energised);
}

/* @verifies REQ-ALM-001 */
static void test_firmware_alarm_1_after_warmup_and_three_samples(void)
{
    uint8_t i;
    eventlog_entry_t entry;

    boot_defaults();
    /* Calibration maps the raw value to 0.1 %LEL one for one: 150 raw is 150 units (15 %LEL). */
    set_gas_raw(150u);
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_sample(&fw, 130000u));
    for (i = 0u; i < 3u; i++) {
        TEST_ASSERT_EQUAL_INT(GG_OK, firmware_sample(&fw, 130000u + (uint32_t)i * 100u));
    }
    TEST_ASSERT_EQUAL_INT(DET_STATE_ALARM_1, firmware_state(&fw));
    TEST_ASSERT_EQUAL_INT(GG_OK, eventlog_read_slot(2u, &entry));
    TEST_ASSERT_EQUAL_INT(EVENTLOG_MODE_CHANGE, entry.code);
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
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_output(&fw));
    TEST_ASSERT_EQUAL_INT(GG_OK, firmware_relay_energised(&fw, &energised));
    TEST_ASSERT_FALSE(energised);
    TEST_ASSERT_FALSE(hal_gpio_fake_level(HAL_GPIO_PIN_RELAY));
    TEST_ASSERT_TRUE(hal_gpio_fake_level(HAL_GPIO_PIN_BUZZER));
    /* The new fault is logged once, after the mode change to FAULT. */
    TEST_ASSERT_EQUAL_INT(GG_OK, eventlog_read_slot(3u, &entry));
    TEST_ASSERT_EQUAL_INT(EVENTLOG_FAULT, entry.code);
}

/* @verifies REQ-SAF-007 */
static void test_firmware_null_arguments_are_rejected(void)
{
    bool energised = false;

    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, firmware_boot(NULL, HAL_RESET_POWER_ON, 0u));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, firmware_sample(NULL, 0u));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, firmware_output(NULL));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, firmware_relay_energised(&fw, NULL));
    (void)energised;
}

// DEV-03: Unity runner entry point, called by the test harness; MISRA 8.6 does not apply.
// cppcheck-suppress misra-c2012-8.6
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_firmware_boot_logs_the_reset_cause);
    RUN_TEST(test_firmware_boot_logs_when_defaults_are_used);
    RUN_TEST(test_firmware_boot_starts_in_warmup_with_the_relay_de_energised);
    RUN_TEST(test_firmware_alarm_1_after_warmup_and_three_samples);
    RUN_TEST(test_firmware_both_channels_short_raise_fault_and_drop_the_relay);
    RUN_TEST(test_firmware_null_arguments_are_rejected);
    return UNITY_END();
}
