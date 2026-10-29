/* Unit tests for the host watchdog fake, which stands in for hal/hal_wdt.h on the PC. */
#include "unity.h"
#include "common/gg_types.h"
#include "hal/hal_wdt.h"
#include "hal/host/hal_wdt_fake.h"

#include <stdbool.h>

/* DEV-03: Unity hooks, called by the framework; MISRA 8.6 and unusedFunction do not apply. */
// cppcheck-suppress misra-c2012-8.6
void setUp(void);
// cppcheck-suppress misra-c2012-8.6
void tearDown(void);

void setUp(void)
{
    hal_wdt_fake_reset();
}

void tearDown(void) {}

/* @verifies REQ-SAF-005 */
static void test_hal_wdt_enable_turns_watchdog_on(void)
{
    TEST_ASSERT_FALSE(hal_wdt_fake_is_enabled());
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_wdt_enable());
    TEST_ASSERT_TRUE(hal_wdt_fake_is_enabled());
}

/* @verifies REQ-SAF-005 */
static void test_hal_wdt_kick_is_counted(void)
{
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_wdt_enable());
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_wdt_kick());
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_wdt_kick());
    TEST_ASSERT_EQUAL_UINT16(2u, hal_wdt_fake_kicks());
}

// cppcheck-suppress misra-c2012-8.6
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_hal_wdt_enable_turns_watchdog_on);
    RUN_TEST(test_hal_wdt_kick_is_counted);
    return UNITY_END();
}
