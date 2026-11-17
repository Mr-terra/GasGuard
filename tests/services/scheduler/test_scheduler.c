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

/* @verifies REQ-SAF-005 REQ-FUN-001 */
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

/* @verifies REQ-SAF-005 */
static void test_scheduler_late_task_runs_once_not_in_a_burst(void)
{
    scheduler_t late_sched;
    uint32_t late_periods[1] = { 100u };
    bool due = false;

    TEST_ASSERT_EQUAL_INT(GG_OK, scheduler_init(&late_sched, 1u, late_periods, 0u));
    /* 3.5 late_periods late: one run now, the next one a period after this instant. */
    TEST_ASSERT_EQUAL_INT(GG_OK, scheduler_task_due(&late_sched, 0u, 350u, &due));
    TEST_ASSERT_TRUE(due);
    TEST_ASSERT_EQUAL_INT(GG_OK, scheduler_task_due(&late_sched, 0u, 351u, &due));
    TEST_ASSERT_FALSE(due);
    TEST_ASSERT_EQUAL_INT(GG_OK, scheduler_task_due(&late_sched, 0u, 450u, &due));
    TEST_ASSERT_TRUE(due);
}

// DEV-03: Unity runner entry point, called by the test harness; MISRA 8.6 does not apply.
/* @verifies REQ-SAF-005 */
static void test_scheduler_accepts_one_to_eight_tasks(void)
{
    static const uint32_t eight[SCHED_MAX_TASKS] = { 10u, 20u, 30u, 40u, 50u, 60u, 70u, 80u };
    scheduler_t other;

    TEST_ASSERT_EQUAL_INT(GG_OK, scheduler_init(&other, 1u, eight, 0u));
    TEST_ASSERT_EQUAL_INT(GG_OK, scheduler_init(&other, SCHED_MAX_TASKS, eight, 0u));
}

/* @verifies REQ-SAF-005 */
static void test_watchdog_is_not_kicked_when_no_task_has_run(void)
{
    bool kick = true;

    TEST_ASSERT_EQUAL_INT(GG_OK, scheduler_begin_cycle(&sched, 0u));
    TEST_ASSERT_EQUAL_INT(GG_OK, scheduler_watchdog_ok(&sched, &kick));
    TEST_ASSERT_FALSE(kick);
}

/* @verifies REQ-SAF-005 */
static void test_tasks_done_in_the_last_cycle_do_not_allow_a_kick_in_this_one(void)
{
    bool kick = false;

    /* Cycle 1: both tasks run, the kick is allowed. */
    TEST_ASSERT_EQUAL_INT(GG_OK, scheduler_begin_cycle(&sched, 0u));
    TEST_ASSERT_EQUAL_INT(GG_OK, scheduler_task_done(&sched, 0u));
    TEST_ASSERT_EQUAL_INT(GG_OK, scheduler_task_done(&sched, 1u));
    TEST_ASSERT_EQUAL_INT(GG_OK, scheduler_watchdog_ok(&sched, &kick));
    TEST_ASSERT_TRUE(kick);
    /* Cycle 2: task 1 hangs. Its flag from cycle 1 must not keep the watchdog quiet. */
    TEST_ASSERT_EQUAL_INT(GG_OK, scheduler_begin_cycle(&sched, 10u));
    TEST_ASSERT_EQUAL_INT(GG_OK, scheduler_task_done(&sched, 0u));
    TEST_ASSERT_EQUAL_INT(GG_OK, scheduler_watchdog_ok(&sched, &kick));
    TEST_ASSERT_FALSE(kick);
}

/* @verifies REQ-SAF-005 */
static void test_scheduler_null_arguments_are_rejected(void)
{
    bool flag = false;

    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, scheduler_begin_cycle(NULL, 0u));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, scheduler_task_due(NULL, 0u, 0u, &flag));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, scheduler_task_done(NULL, 0u));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, scheduler_watchdog_ok(NULL, &flag));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, scheduler_watchdog_ok(&sched, NULL));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, scheduler_end_cycle(NULL, 0u, &flag, &flag));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, scheduler_end_cycle(&sched, 0u, NULL, &flag));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, scheduler_end_cycle(&sched, 0u, &flag, NULL));
}

/* @verifies REQ-SAF-005 */
static void test_scheduler_task_stays_on_time_across_the_clock_wrap(void)
{
    scheduler_t wrap_sched;
    const uint32_t wrap_periods[1] = { 100u };
    const uint32_t start_ms = UINT32_MAX - 49u;   /* 50 ms before the wrap. */
    bool due = false;

    TEST_ASSERT_EQUAL_INT(GG_OK, scheduler_init(&wrap_sched, 1u, wrap_periods, start_ms));
    TEST_ASSERT_EQUAL_INT(GG_OK, scheduler_task_due(&wrap_sched, 0u, start_ms, &due));
    TEST_ASSERT_TRUE(due);
    /* Next due 100 ms later: 49 after the wrap. */
    TEST_ASSERT_EQUAL_INT(GG_OK, scheduler_task_due(&wrap_sched, 0u, 49u, &due));
    TEST_ASSERT_FALSE(due);
    TEST_ASSERT_EQUAL_INT(GG_OK, scheduler_task_due(&wrap_sched, 0u, 50u, &due));
    TEST_ASSERT_TRUE(due);
}

/* @verifies REQ-SAF-008 */
static void test_two_overruns_in_a_row_are_not_yet_a_fault(void)
{
    bool overrun = false;
    bool fault = true;
    uint8_t i;

    for (i = 0u; i < (SCHED_OVERRUN_FAULT_COUNT - 1u); i++) {
        TEST_ASSERT_EQUAL_INT(GG_OK, scheduler_begin_cycle(&sched, 0u));
        TEST_ASSERT_EQUAL_INT(GG_OK, scheduler_end_cycle(&sched, SCHED_OVERRUN_MS + 1u, &overrun, &fault));
        TEST_ASSERT_TRUE(overrun);
        TEST_ASSERT_FALSE(fault);
    }
}

/* @verifies REQ-SAF-008 */
static void test_overruns_past_three_keep_the_fault_and_never_wrap_the_count(void)
{
    bool overrun = false;
    bool fault = false;
    uint16_t i;

    /* 300 overruns in a row: an 8-bit count that wrapped would clear the fault. */
    for (i = 0u; i < 300u; i++) {
        TEST_ASSERT_EQUAL_INT(GG_OK, scheduler_begin_cycle(&sched, 0u));
        TEST_ASSERT_EQUAL_INT(GG_OK, scheduler_end_cycle(&sched, 200u, &overrun, &fault));
        if (i >= (SCHED_OVERRUN_FAULT_COUNT - 1u)) {
            TEST_ASSERT_TRUE(fault);
        }
    }
}

/* @verifies REQ-SAF-008 */
static void test_overrun_is_measured_across_the_clock_wrap(void)
{
    bool overrun = false;
    bool fault = true;

    /* Starts 50 ms before the wrap, ends 51 ms after it: 101 ms long. */
    TEST_ASSERT_EQUAL_INT(GG_OK, scheduler_begin_cycle(&sched, UINT32_MAX - 49u));
    TEST_ASSERT_EQUAL_INT(GG_OK, scheduler_end_cycle(&sched, 51u, &overrun, &fault));
    TEST_ASSERT_TRUE(overrun);
    /* Starts 50 ms before the wrap, ends 50 ms after it: 100 ms, not an overrun. */
    TEST_ASSERT_EQUAL_INT(GG_OK, scheduler_begin_cycle(&sched, UINT32_MAX - 49u));
    TEST_ASSERT_EQUAL_INT(GG_OK, scheduler_end_cycle(&sched, 50u, &overrun, &fault));
    TEST_ASSERT_FALSE(overrun);
}

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
    RUN_TEST(test_scheduler_late_task_runs_once_not_in_a_burst);
    RUN_TEST(test_scheduler_accepts_one_to_eight_tasks);
    RUN_TEST(test_watchdog_is_not_kicked_when_no_task_has_run);
    RUN_TEST(test_tasks_done_in_the_last_cycle_do_not_allow_a_kick_in_this_one);
    RUN_TEST(test_scheduler_null_arguments_are_rejected);
    RUN_TEST(test_scheduler_task_stays_on_time_across_the_clock_wrap);
    RUN_TEST(test_two_overruns_in_a_row_are_not_yet_a_fault);
    RUN_TEST(test_overruns_past_three_keep_the_fault_and_never_wrap_the_count);
    RUN_TEST(test_overrun_is_measured_across_the_clock_wrap);
    return UNITY_END();
}
