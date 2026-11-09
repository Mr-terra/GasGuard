/* Unit tests for the cooperative scheduler: due tasks, overrun detection, watchdog kick policy. */
#include "unity.h"
#include "common/gg_types/gg_types.h"
#include "services/scheduler/scheduler.h"

#include <stdbool.h>
#include <stdint.h>

/* DEV-03: Unity hooks, called by the framework; MISRA 8.6 and unusedFunction do not apply. */
// cppcheck-suppress misra-c2012-8.6
void setUp(void);
// cppcheck-suppress misra-c2012-8.6
void tearDown(void);

static scheduler_t sched;
static const uint32_t periods[2] = { 100u, 500u };

void setUp(void)
{
    (void)scheduler_init(&sched, 2u, periods, 0u);
}

void tearDown(void) {}

/* @verifies REQ-SAF-005 */
static void test_scheduler_init_rejects_bad_arguments(void)
{
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, scheduler_init(NULL, 2u, periods, 0u));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, scheduler_init(&sched, 2u, NULL, 0u));
    TEST_ASSERT_EQUAL_INT(GG_ERR_RANGE, scheduler_init(&sched, 0u, periods, 0u));
    TEST_ASSERT_EQUAL_INT(GG_ERR_RANGE, scheduler_init(&sched, SCHED_MAX_TASKS + 1u, periods, 0u));
}

/* @verifies REQ-SAF-005 */
static void test_scheduler_task_is_due_at_its_period(void)
{
    bool due = false;

    TEST_ASSERT_EQUAL_INT(GG_OK, scheduler_begin_cycle(&sched, 0u));
    TEST_ASSERT_EQUAL_INT(GG_OK, scheduler_task_due(&sched, 0u, 0u, &due));
    TEST_ASSERT_TRUE(due);
    TEST_ASSERT_EQUAL_INT(GG_OK, scheduler_task_done(&sched, 0u));

    TEST_ASSERT_EQUAL_INT(GG_OK, scheduler_begin_cycle(&sched, 99u));
    TEST_ASSERT_EQUAL_INT(GG_OK, scheduler_task_due(&sched, 0u, 99u, &due));
    TEST_ASSERT_FALSE(due);

    TEST_ASSERT_EQUAL_INT(GG_OK, scheduler_begin_cycle(&sched, 100u));
    TEST_ASSERT_EQUAL_INT(GG_OK, scheduler_task_due(&sched, 0u, 100u, &due));
    TEST_ASSERT_TRUE(due);
}

/* @verifies REQ-SAF-005 */
static void test_scheduler_task_index_out_of_range_is_rejected(void)
{
    bool due = false;

    TEST_ASSERT_EQUAL_INT(GG_ERR_RANGE, scheduler_task_due(&sched, 2u, 0u, &due));
    TEST_ASSERT_EQUAL_INT(GG_ERR_RANGE, scheduler_task_done(&sched, 2u));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, scheduler_task_due(&sched, 0u, 0u, NULL));
}

/* @verifies REQ-SAF-005 */
static void test_watchdog_is_kicked_only_when_every_task_has_run(void)
{
    bool kick = true;

    TEST_ASSERT_EQUAL_INT(GG_OK, scheduler_begin_cycle(&sched, 0u));
    TEST_ASSERT_EQUAL_INT(GG_OK, scheduler_task_done(&sched, 0u));
    TEST_ASSERT_EQUAL_INT(GG_OK, scheduler_watchdog_ok(&sched, &kick));
    TEST_ASSERT_FALSE(kick);

    TEST_ASSERT_EQUAL_INT(GG_OK, scheduler_task_done(&sched, 1u));
    TEST_ASSERT_EQUAL_INT(GG_OK, scheduler_watchdog_ok(&sched, &kick));
    TEST_ASSERT_TRUE(kick);
}

/* @verifies REQ-SAF-008 */
static void test_cycle_of_100_ms_is_not_an_overrun(void)
{
    bool overrun = true;
    bool fault = true;

    TEST_ASSERT_EQUAL_INT(GG_OK, scheduler_begin_cycle(&sched, 0u));
    TEST_ASSERT_EQUAL_INT(GG_OK, scheduler_end_cycle(&sched, 100u, &overrun, &fault));
    TEST_ASSERT_FALSE(overrun);
    TEST_ASSERT_FALSE(fault);
}

/* @verifies REQ-SAF-008 */
static void test_cycle_over_100_ms_is_an_overrun(void)
{
    bool overrun = false;
    bool fault = true;

    TEST_ASSERT_EQUAL_INT(GG_OK, scheduler_begin_cycle(&sched, 0u));
    TEST_ASSERT_EQUAL_INT(GG_OK, scheduler_end_cycle(&sched, 101u, &overrun, &fault));
    TEST_ASSERT_TRUE(overrun);
    TEST_ASSERT_FALSE(fault);
}

/* @verifies REQ-SAF-008 */
static void test_three_consecutive_overruns_raise_fault(void)
{
    bool overrun = false;
    bool fault = false;
    uint8_t i;

    for (i = 0u; i < 3u; i++) {
        TEST_ASSERT_EQUAL_INT(GG_OK, scheduler_begin_cycle(&sched, 0u));
        TEST_ASSERT_EQUAL_INT(GG_OK, scheduler_end_cycle(&sched, 200u, &overrun, &fault));
        TEST_ASSERT_TRUE(overrun);
    }
    TEST_ASSERT_TRUE(fault);
}

/* @verifies REQ-SAF-008 */
static void test_a_normal_cycle_resets_the_overrun_count(void)
{
    bool overrun = false;
    bool fault = false;

    TEST_ASSERT_EQUAL_INT(GG_OK, scheduler_begin_cycle(&sched, 0u));
    TEST_ASSERT_EQUAL_INT(GG_OK, scheduler_end_cycle(&sched, 200u, &overrun, &fault));
    TEST_ASSERT_EQUAL_INT(GG_OK, scheduler_begin_cycle(&sched, 0u));
    TEST_ASSERT_EQUAL_INT(GG_OK, scheduler_end_cycle(&sched, 50u, &overrun, &fault));
    TEST_ASSERT_EQUAL_INT(GG_OK, scheduler_begin_cycle(&sched, 0u));
    TEST_ASSERT_EQUAL_INT(GG_OK, scheduler_end_cycle(&sched, 200u, &overrun, &fault));
    TEST_ASSERT_EQUAL_INT(GG_OK, scheduler_begin_cycle(&sched, 0u));
    TEST_ASSERT_EQUAL_INT(GG_OK, scheduler_end_cycle(&sched, 200u, &overrun, &fault));
    TEST_ASSERT_FALSE(fault);
}

// DEV-03: Unity runner entry point, called by the test harness; MISRA 8.6 does not apply.
// cppcheck-suppress misra-c2012-8.6
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_scheduler_init_rejects_bad_arguments);
    RUN_TEST(test_scheduler_task_is_due_at_its_period);
    RUN_TEST(test_scheduler_task_index_out_of_range_is_rejected);
    RUN_TEST(test_watchdog_is_kicked_only_when_every_task_has_run);
    RUN_TEST(test_cycle_of_100_ms_is_not_an_overrun);
    RUN_TEST(test_cycle_over_100_ms_is_an_overrun);
    RUN_TEST(test_three_consecutive_overruns_raise_fault);
    RUN_TEST(test_a_normal_cycle_resets_the_overrun_count);
    return UNITY_END();
}
