/* Unit tests for the buzzer driver, which switches the D6 output through the GPIO HAL. */
#include "unity.h"
#include "common/gg_types/gg_types.h"
#include "drivers/buzzer/buzzer.h"
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

/* @verifies REQ-HMI-003 */
static void test_buzzer_init_returns_ok(void)
{
    TEST_ASSERT_EQUAL_INT(GG_OK, buzzer_init());
}

/* @verifies REQ-HMI-003 */
static void test_buzzer_init_silences_the_buzzer(void)
{
    /* Start with the output high, as if a previous run left the buzzer on. */
    (void)hal_gpio_write(HAL_GPIO_PIN_BUZZER, true);
    TEST_ASSERT_EQUAL_INT(GG_OK, buzzer_init());
    TEST_ASSERT_FALSE(hal_gpio_fake_level(HAL_GPIO_PIN_BUZZER));
}

/* @verifies REQ-HMI-003 */
static void test_buzzer_set_true_sounds_the_buzzer(void)
{
    TEST_ASSERT_EQUAL_INT(GG_OK, buzzer_set(true));
    TEST_ASSERT_TRUE(hal_gpio_fake_level(HAL_GPIO_PIN_BUZZER));
}

/* @verifies REQ-HMI-003 */
static void test_buzzer_set_false_silences_the_buzzer(void)
{
    TEST_ASSERT_EQUAL_INT(GG_OK, buzzer_set(true));
    TEST_ASSERT_EQUAL_INT(GG_OK, buzzer_set(false));
    TEST_ASSERT_FALSE(hal_gpio_fake_level(HAL_GPIO_PIN_BUZZER));
}

// DEV-03: Unity runner entry point, called by the test harness; MISRA 8.6 does not apply.
// cppcheck-suppress misra-c2012-8.6
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_buzzer_init_returns_ok);
    RUN_TEST(test_buzzer_init_silences_the_buzzer);
    RUN_TEST(test_buzzer_set_true_sounds_the_buzzer);
    RUN_TEST(test_buzzer_set_false_silences_the_buzzer);
    return UNITY_END();
}
