/* Unit tests for the host GPIO fake, which stands in for hal/hal_gpio.h on the PC. */
#include "unity.h"
#include "common/gg_types.h"
#include "hal/hal_gpio.h"
#include "hal/host/hal_gpio_fake.h"

#include <stdbool.h>

/* DEV-03: Unity hooks, called by the framework; MISRA 8.6 and unusedFunction do not apply. */
// cppcheck-suppress misra-c2012-8.6
void setUp(void);
// cppcheck-suppress misra-c2012-8.6
void tearDown(void);

void setUp(void)
{
    hal_gpio_fake_reset();
}

void tearDown(void) {}

/* @verifies REQ-SAF-001 */
static void test_hal_gpio_init_succeeds(void)
{
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_gpio_init());
}

/* @verifies REQ-SAF-001 */
static void test_hal_gpio_write_drives_output_pin(void)
{
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_gpio_write(HAL_GPIO_PIN_RELAY, true));
    TEST_ASSERT_TRUE(hal_gpio_fake_level(HAL_GPIO_PIN_RELAY));
}

/* @verifies REQ-SAF-001 */
static void test_hal_gpio_write_rejects_input_pin(void)
{
    TEST_ASSERT_EQUAL_INT(GG_ERR_RANGE, hal_gpio_write(HAL_GPIO_PIN_BUTTON, true));
}

/* @verifies REQ-HMI-007 */
static void test_hal_gpio_read_returns_scripted_input(void)
{
    bool high = false;

    TEST_ASSERT_EQUAL_INT(GG_OK, hal_gpio_fake_set_input(HAL_GPIO_PIN_BUTTON, true));
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_gpio_read(HAL_GPIO_PIN_BUTTON, &high));
    TEST_ASSERT_TRUE(high);
}

/* @verifies REQ-HMI-007 */
static void test_hal_gpio_read_rejects_output_pin(void)
{
    bool high = false;

    TEST_ASSERT_EQUAL_INT(GG_ERR_RANGE, hal_gpio_read(HAL_GPIO_PIN_RELAY, &high));
}

/* @verifies REQ-HMI-007 */
static void test_hal_gpio_read_rejects_null_output(void)
{
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, hal_gpio_read(HAL_GPIO_PIN_TOUCH, NULL));
}

// cppcheck-suppress misra-c2012-8.6
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_hal_gpio_init_succeeds);
    RUN_TEST(test_hal_gpio_write_drives_output_pin);
    RUN_TEST(test_hal_gpio_write_rejects_input_pin);
    RUN_TEST(test_hal_gpio_read_returns_scripted_input);
    RUN_TEST(test_hal_gpio_read_rejects_output_pin);
    RUN_TEST(test_hal_gpio_read_rejects_null_output);
    return UNITY_END();
}
