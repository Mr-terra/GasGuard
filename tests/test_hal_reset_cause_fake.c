/* Unit tests for the host reset-cause fake, which stands in for hal/hal_reset_cause.h on the PC. */
#include "unity.h"
#include "hal/hal_reset_cause.h"
#include "hal/host/hal_reset_cause_fake.h"

/* DEV-03: Unity hooks, called by the framework; MISRA 8.6 and unusedFunction do not apply. */
// cppcheck-suppress misra-c2012-8.6
void setUp(void);
// cppcheck-suppress misra-c2012-8.6
void tearDown(void);

// cppcheck-suppress unusedFunction
void setUp(void)
{
    hal_reset_cause_fake_reset();
}

// cppcheck-suppress unusedFunction
void tearDown(void) {}

/* @verifies REQ-SAF-006 */
static void test_hal_reset_cause_defaults_to_power_on(void)
{
    TEST_ASSERT_EQUAL_INT(HAL_RESET_POWER_ON, hal_reset_cause_get());
}

/* @verifies REQ-SAF-006 */
static void test_hal_reset_cause_returns_scripted_watchdog(void)
{
    hal_reset_cause_fake_set(HAL_RESET_WATCHDOG);

    TEST_ASSERT_EQUAL_INT(HAL_RESET_WATCHDOG, hal_reset_cause_get());
}

// cppcheck-suppress misra-c2012-8.6
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_hal_reset_cause_defaults_to_power_on);
    RUN_TEST(test_hal_reset_cause_returns_scripted_watchdog);
    return UNITY_END();
}
