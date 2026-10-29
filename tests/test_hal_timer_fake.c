/* Unit tests for the host timer fake, which stands in for hal/hal_timer.h on the PC. */
#include "unity.h"
#include "common/gg_types.h"
#include "hal/hal_timer.h"
#include "hal/host/hal_timer_fake.h"

#include <stdint.h>

/* DEV-03: Unity hooks, called by the framework; MISRA 8.6 and unusedFunction do not apply. */
// cppcheck-suppress misra-c2012-8.6
void setUp(void);
// cppcheck-suppress misra-c2012-8.6
void tearDown(void);

// cppcheck-suppress unusedFunction
void setUp(void)
{
    hal_timer_fake_reset();
}

// cppcheck-suppress unusedFunction
void tearDown(void) {}

/* @verifies REQ-FUN-001 */
static void test_hal_timer_init_succeeds(void)
{
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_timer_init());
}

/* @verifies REQ-LOG-002 */
static void test_hal_timer_fake_reports_set_uptime(void)
{
    uint32_t ms = 0u;

    hal_timer_fake_set_ms(1234u);

    TEST_ASSERT_EQUAL_INT(GG_OK, hal_timer_get_ms(&ms));
    TEST_ASSERT_EQUAL_UINT32(1234u, ms);
}

/* @verifies REQ-LOG-002 */
static void test_hal_timer_get_ms_rejects_null(void)
{
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, hal_timer_get_ms(NULL));
}

// cppcheck-suppress misra-c2012-8.6
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_hal_timer_init_succeeds);
    RUN_TEST(test_hal_timer_fake_reports_set_uptime);
    RUN_TEST(test_hal_timer_get_ms_rejects_null);
    return UNITY_END();
}
