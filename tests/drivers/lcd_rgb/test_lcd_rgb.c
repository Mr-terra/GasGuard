/* Unit tests for the Grove LCD RGB Backlight v4.0 driver, on the I2C fake. */
#include "unity.h"
#include "common/gg_types/gg_types.h"
#include "drivers/lcd_rgb/lcd_rgb.h"
#include "hal/i2c/i2c_fake.h"

#include <stdbool.h>
#include <stdint.h>

/* DEV-03: Unity hooks, called by the framework; MISRA 8.6 and unusedFunction do not apply. */
// cppcheck-suppress misra-c2012-8.6
void setUp(void);
// cppcheck-suppress misra-c2012-8.6
void tearDown(void);

#define LCD_TEXT_ADDR 0x3Eu
#define LCD_RGB_ADDR  0x62u

/** @brief Delays requested by the driver, in order, recorded by the test delay function. */
static uint32_t delays[16];
static uint8_t delay_count;

static void record_delay(uint32_t ms)
{
    if (delay_count < 16u) {
        delays[delay_count] = ms;
    }
    delay_count++;
}

void setUp(void)
{
    hal_i2c_fake_reset();
    (void)hal_i2c_fake_set_present(LCD_TEXT_ADDR, true);
    (void)hal_i2c_fake_set_present(LCD_RGB_ADDR, true);
    delay_count = 0u;
}

void tearDown(void) {}

/* @verifies REQ-HMI-001 */
static void test_lcd_rgb_init_returns_ok_when_both_chips_answer(void)
{
    TEST_ASSERT_EQUAL_INT(GG_OK, lcd_rgb_init(record_delay));
}

/* @verifies REQ-HMI-001 */
static void test_lcd_rgb_init_fails_when_the_text_controller_is_absent(void)
{
    (void)hal_i2c_fake_set_present(LCD_TEXT_ADDR, false);
    TEST_ASSERT_EQUAL_INT(GG_ERR_TIMEOUT, lcd_rgb_init(record_delay));
}

/* @verifies REQ-HMI-002 */
static void test_lcd_rgb_init_fails_when_the_backlight_chip_is_absent(void)
{
    (void)hal_i2c_fake_set_present(LCD_RGB_ADDR, false);
    TEST_ASSERT_EQUAL_INT(GG_ERR_TIMEOUT, lcd_rgb_init(record_delay));
}

/* @verifies REQ-HMI-001 */
static void test_lcd_rgb_init_rejects_null_delay_function(void)
{
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, lcd_rgb_init(NULL));
}

/* @verifies REQ-HMI-001 */
static void test_lcd_rgb_init_requests_the_power_up_and_command_delays(void)
{
    (void)lcd_rgb_init(record_delay);
    TEST_ASSERT_EQUAL_UINT8(4u, delay_count);
    TEST_ASSERT_EQUAL_UINT32(50u, delays[0]);
    TEST_ASSERT_EQUAL_UINT32(5u, delays[1]);
    TEST_ASSERT_EQUAL_UINT32(1u, delays[2]);
    TEST_ASSERT_EQUAL_UINT32(2u, delays[3]);
}

/* @verifies REQ-HMI-001 */
static void test_lcd_rgb_init_sends_the_text_controller_sequence(void)
{
    /* Each command is a control byte 0x80 followed by the command. */
    static const uint8_t expected[] = {
        0x80u, 0x38u, 0x80u, 0x38u, 0x80u, 0x38u, 0x80u, 0x38u,
        0x80u, 0x0Cu, 0x80u, 0x01u, 0x80u, 0x06u
    };
    uint8_t i;

    (void)lcd_rgb_init(record_delay);
    for (i = 0u; i < (uint8_t)sizeof(expected); i++) {
        TEST_ASSERT_EQUAL_UINT8(expected[i], hal_i2c_fake_log_byte(i));
    }
}

/* @verifies REQ-HMI-002 */
static void test_lcd_rgb_init_configures_the_backlight_chip(void)
{
    /* After the 14 text bytes: MODE1 = 0, LEDOUT = 0xFF, MODE2 = 0x20, then white. */
    static const uint8_t expected[] = {
        0x00u, 0x00u, 0x08u, 0xFFu, 0x01u, 0x20u,
        0x04u, 0xFFu, 0x03u, 0xFFu, 0x02u, 0xFFu
    };
    uint8_t i;

    (void)lcd_rgb_init(record_delay);
    for (i = 0u; i < (uint8_t)sizeof(expected); i++) {
        TEST_ASSERT_EQUAL_UINT8(expected[i], hal_i2c_fake_log_byte((uint8_t)(14u + i)));
    }
}

/* @verifies REQ-HMI-002 */
static void test_lcd_rgb_set_colour_writes_red_green_blue_registers(void)
{
    (void)lcd_rgb_init(record_delay);
    TEST_ASSERT_EQUAL_INT(GG_OK, lcd_rgb_set_colour(0xFFu, 0x00u, 0x00u));
    TEST_ASSERT_EQUAL_UINT8(0x04u, hal_i2c_fake_log_byte(26u));
    TEST_ASSERT_EQUAL_UINT8(0xFFu, hal_i2c_fake_log_byte(27u));
    TEST_ASSERT_EQUAL_UINT8(0x03u, hal_i2c_fake_log_byte(28u));
    TEST_ASSERT_EQUAL_UINT8(0x00u, hal_i2c_fake_log_byte(29u));
    TEST_ASSERT_EQUAL_UINT8(0x02u, hal_i2c_fake_log_byte(30u));
    TEST_ASSERT_EQUAL_UINT8(0x00u, hal_i2c_fake_log_byte(31u));
}

/* @verifies REQ-HMI-001 */
static void test_lcd_rgb_write_line_zero_sets_cursor_and_sends_characters(void)
{
    (void)lcd_rgb_init(record_delay);
    hal_i2c_fake_reset();
    (void)hal_i2c_fake_set_present(LCD_TEXT_ADDR, true);
    TEST_ASSERT_EQUAL_INT(GG_OK, lcd_rgb_write_line(0u, "AB"));
    TEST_ASSERT_EQUAL_UINT8(0x80u, hal_i2c_fake_log_byte(0u));
    TEST_ASSERT_EQUAL_UINT8(0x80u, hal_i2c_fake_log_byte(1u));
    /* One data transfer: the control byte once, then the characters. */
    TEST_ASSERT_EQUAL_UINT8(0x40u, hal_i2c_fake_log_byte(2u));
    TEST_ASSERT_EQUAL_UINT8((uint8_t)'A', hal_i2c_fake_log_byte(3u));
    TEST_ASSERT_EQUAL_UINT8((uint8_t)'B', hal_i2c_fake_log_byte(4u));
}

/* @verifies REQ-HMI-001 */
static void test_lcd_rgb_write_line_one_uses_the_second_row_address(void)
{
    hal_i2c_fake_reset();
    (void)hal_i2c_fake_set_present(LCD_TEXT_ADDR, true);
    TEST_ASSERT_EQUAL_INT(GG_OK, lcd_rgb_write_line(1u, "X"));
    TEST_ASSERT_EQUAL_UINT8(0x80u, hal_i2c_fake_log_byte(0u));
    TEST_ASSERT_EQUAL_UINT8(0xC0u, hal_i2c_fake_log_byte(1u));
}

/* @verifies REQ-HMI-001 */
static void test_lcd_rgb_write_line_pads_the_row_with_spaces(void)
{
    uint8_t i;

    hal_i2c_fake_reset();
    (void)hal_i2c_fake_set_present(LCD_TEXT_ADDR, true);
    /* "FAULT" after "WARMUP" must not leave the last "P" on the display. */
    TEST_ASSERT_EQUAL_INT(GG_OK, lcd_rgb_write_line(0u, "FAULT"));
    TEST_ASSERT_EQUAL_UINT8(LCD_LINE_LEN + 1u, hal_i2c_fake_last_len());
    /* Log: 2 bytes of cursor command, the data control byte, then the 16 characters. */
    TEST_ASSERT_EQUAL_UINT8((uint8_t)'T', hal_i2c_fake_log_byte(7u));
    for (i = 8u; i < (2u + 1u + LCD_LINE_LEN); i++) {
        TEST_ASSERT_EQUAL_UINT8((uint8_t)' ', hal_i2c_fake_log_byte(i));
    }
}

/* @verifies REQ-HMI-001 */
static void test_lcd_rgb_write_line_rejects_bad_arguments(void)
{
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, lcd_rgb_write_line(0u, NULL));
    TEST_ASSERT_EQUAL_INT(GG_ERR_RANGE, lcd_rgb_write_line(2u, "X"));
}

// DEV-03: Unity runner entry point, called by the test harness; MISRA 8.6 does not apply.
// cppcheck-suppress misra-c2012-8.6
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_lcd_rgb_init_returns_ok_when_both_chips_answer);
    RUN_TEST(test_lcd_rgb_init_fails_when_the_text_controller_is_absent);
    RUN_TEST(test_lcd_rgb_init_fails_when_the_backlight_chip_is_absent);
    RUN_TEST(test_lcd_rgb_init_rejects_null_delay_function);
    RUN_TEST(test_lcd_rgb_init_requests_the_power_up_and_command_delays);
    RUN_TEST(test_lcd_rgb_init_sends_the_text_controller_sequence);
    RUN_TEST(test_lcd_rgb_init_configures_the_backlight_chip);
    RUN_TEST(test_lcd_rgb_set_colour_writes_red_green_blue_registers);
    RUN_TEST(test_lcd_rgb_write_line_zero_sets_cursor_and_sends_characters);
    RUN_TEST(test_lcd_rgb_write_line_one_uses_the_second_row_address);
    RUN_TEST(test_lcd_rgb_write_line_pads_the_row_with_spaces);
    RUN_TEST(test_lcd_rgb_write_line_rejects_bad_arguments);
    return UNITY_END();
}
