/* Unit tests for the host timer fake, which stands in for hal/timer/timer.h on the PC. */
#include "unity.h"
#include "common/gg_types/gg_types.h"
#include "hal/timer/timer.h"
#include "hal/timer/timer_fake.h"

#include <stdint.h>

/* DEV-03: Unity hooks, called by the framework; MISRA 8.6 and unusedFunction do not apply. */
// cppcheck-suppress misra-c2012-8.6
void setUp(void);
// cppcheck-suppress misra-c2012-8.6
void tearDown(void);

void setUp(void)
{
    hal_timer_fake_reset();
}

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

/* @verifies REQ-LOG-002 */
static void test_hal_timer_fake_clock_is_still_by_default(void)
{
    uint32_t first = 0u;
    uint32_t second = 0u;

    hal_timer_fake_set_ms(50u);
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_timer_get_ms(&first));
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_timer_get_ms(&second));
    TEST_ASSERT_EQUAL_UINT32(first, second);
}

/* @verifies REQ-LOG-002 */
static void test_hal_timer_fake_step_moves_the_clock_after_each_read_and_wraps(void)
{
    uint32_t ms = 0u;

    hal_timer_fake_set_ms(UINT32_MAX);
    hal_timer_fake_set_step(1u);
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_timer_get_ms(&ms));
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, ms);
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_timer_get_ms(&ms));
    TEST_ASSERT_EQUAL_UINT32(0u, ms);
    hal_timer_fake_reset();   /* The reset also stops the clock again. */
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_timer_get_ms(&ms));
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_timer_get_ms(&ms));
    TEST_ASSERT_EQUAL_UINT32(0u, ms);
}

// cppcheck-suppress misra-c2012-8.6
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_hal_timer_init_succeeds);
    RUN_TEST(test_hal_timer_fake_reports_set_uptime);
    RUN_TEST(test_hal_timer_get_ms_rejects_null);
    RUN_TEST(test_hal_timer_fake_clock_is_still_by_default);
    RUN_TEST(test_hal_timer_fake_step_moves_the_clock_after_each_read_and_wraps);
    return UNITY_END();
}
