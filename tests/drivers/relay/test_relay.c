/* Unit tests for the alarm relay driver, which drives the D5 output through the GPIO HAL. */
#include "unity.h"
#include "common/gg_types/gg_types.h"
#include "drivers/relay/relay.h"
#include "hal/gpio/gpio.h"
#include "hal/gpio/gpio_fake.h"

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
static void test_relay_init_returns_ok(void)
{
    TEST_ASSERT_EQUAL_INT(GG_OK, relay_init());
}

/* @verifies REQ-SAF-001 */
static void test_relay_init_de_energises_the_coil(void)
{
    /* Start with the coil driven high, as if a previous run left it energised. */
    (void)hal_gpio_write(HAL_GPIO_PIN_RELAY, true);
    TEST_ASSERT_EQUAL_INT(GG_OK, relay_init());
    TEST_ASSERT_FALSE(hal_gpio_fake_level(HAL_GPIO_PIN_RELAY));
}

/* @verifies REQ-SAF-001 */
static void test_relay_set_true_energises_the_coil(void)
{
    TEST_ASSERT_EQUAL_INT(GG_OK, relay_set(true));
    TEST_ASSERT_TRUE(hal_gpio_fake_level(HAL_GPIO_PIN_RELAY));
}

/* @verifies REQ-SAF-001 */
static void test_relay_set_false_de_energises_the_coil(void)
{
    TEST_ASSERT_EQUAL_INT(GG_OK, relay_set(true));
    TEST_ASSERT_EQUAL_INT(GG_OK, relay_set(false));
    TEST_ASSERT_FALSE(hal_gpio_fake_level(HAL_GPIO_PIN_RELAY));
}

// DEV-03: Unity runner entry point, called by the test harness; MISRA 8.6 does not apply.
// cppcheck-suppress misra-c2012-8.6
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_relay_init_returns_ok);
    RUN_TEST(test_relay_init_de_energises_the_coil);
    RUN_TEST(test_relay_set_true_energises_the_coil);
    RUN_TEST(test_relay_set_false_de_energises_the_coil);
    return UNITY_END();
}
