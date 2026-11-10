/**
 * @file scheduler.c
 * @brief Cooperative scheduler for the main loop.
 *
 * How it works
 * ------------
 * Every task has a period and a next due time. When the main loop asks whether
 * a task is due and it is, the next due time moves one period forward, so a
 * task runs once per period even if the loop is late. A cycle is the time
 * between begin_cycle and end_cycle. The watchdog may be kicked only when all
 * tasks have reported that they ran in the cycle: a task that is stuck then
 * stops the kick, and the watchdog resets the MCU (REQ-SAF-005). Time is
 * unsigned, and differences are taken with unsigned subtraction, so the counter
 * may wrap. A cycle over 100 ms is an overrun; three in a row raise FAULT.
 *
 * @req REQ-SAF-005 REQ-SAF-008
 */
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "common/gg_types/gg_types.h"
#include "services/scheduler/scheduler.h"

/**
 * @brief Half of the 32-bit time range. A time difference below it means "not earlier":
 *        the due test stays correct when the millisecond counter wraps.
 */
#define SCHED_TIME_HALF_RANGE 0x80000000u

/**
 * @brief Check a task index against the number of tasks in use.
 * @param sched  Scheduler state, already checked to be non-NULL.
 * @param task   Index to check.
 * @return true when the index is in use.
 */
static bool task_in_range(const scheduler_t *sched, uint8_t task)
{
    return task < sched->count;
}

/**
 * @brief Set up the scheduler with a list of task periods.
 * @req REQ-SAF-005
 */
gg_status_t scheduler_init(scheduler_t *sched, uint8_t count, const uint32_t *periods, uint32_t now_ms)
{
    gg_status_t status = GG_OK;

    if ((sched == NULL) || (periods == NULL)) {
        status = GG_ERR_NULL;
    } else if ((count == 0u) || (count > SCHED_MAX_TASKS)) {
        status = GG_ERR_RANGE;
    } else {
        uint8_t i;

        sched->count = count;
        sched->cycle_start_ms = now_ms;
        sched->overruns = 0u;
        for (i = 0u; i < count; i++) {
            sched->period_ms[i] = periods[i];
            sched->next_due_ms[i] = now_ms;
            sched->done[i] = false;
        }
    }
    return status;
}

/**
 * @brief Start a cycle: clear the "ran" flags and record the start time.
 * @req REQ-SAF-005
 */
gg_status_t scheduler_begin_cycle(scheduler_t *sched, uint32_t now_ms)
{
    gg_status_t status = GG_OK;

    if (sched == NULL) {
        status = GG_ERR_NULL;
    } else {
        uint8_t i;

        sched->cycle_start_ms = now_ms;
        for (i = 0u; i < sched->count; i++) {
            sched->done[i] = false;
        }
    }
    return status;
}

/**
 * @brief Check whether a task is due, and advance its next due time if it is.
 * @req REQ-SAF-005
 */
gg_status_t scheduler_task_due(scheduler_t *sched, uint8_t task, uint32_t now_ms, bool *due)
{
    gg_status_t status = GG_OK;

    if ((sched == NULL) || (due == NULL)) {
        status = GG_ERR_NULL;
    } else if (!task_in_range(sched, task)) {
        status = GG_ERR_RANGE;
    } else {
        *due = ((now_ms - sched->next_due_ms[task]) < SCHED_TIME_HALF_RANGE);
        if (*due) {
            sched->next_due_ms[task] += sched->period_ms[task];
            /* More than one period late: run once now, not once per missed period (no burst). */
            if ((now_ms - sched->next_due_ms[task]) < SCHED_TIME_HALF_RANGE) {
                sched->next_due_ms[task] = now_ms + sched->period_ms[task];
            }
        }
    }
    return status;
}

/**
 * @brief Report that a task ran in this cycle.
 * @req REQ-SAF-005
 */
gg_status_t scheduler_task_done(scheduler_t *sched, uint8_t task)
{
    gg_status_t status = GG_OK;

    if (sched == NULL) {
        status = GG_ERR_NULL;
    } else if (!task_in_range(sched, task)) {
        status = GG_ERR_RANGE;
    } else {
        sched->done[task] = true;
    }
    return status;
}

/**
 * @brief End a cycle: measure it and count overruns.
 * @req REQ-SAF-008
 */
gg_status_t scheduler_end_cycle(scheduler_t *sched, uint32_t now_ms, bool *out_overrun, bool *out_fault)
{
    gg_status_t status = GG_OK;

    if ((sched == NULL) || (out_overrun == NULL) || (out_fault == NULL)) {
        status = GG_ERR_NULL;
    } else {
        uint32_t duration_ms = now_ms - sched->cycle_start_ms;

        *out_overrun = (duration_ms > SCHED_OVERRUN_MS);
        if (*out_overrun) {
            if (sched->overruns < SCHED_OVERRUN_FAULT_COUNT) {
                sched->overruns++;
            }
        } else {
            sched->overruns = 0u;
        }
        *out_fault = (sched->overruns >= SCHED_OVERRUN_FAULT_COUNT);
    }
    return status;
}

/**
 * @brief Tell whether the watchdog may be kicked now.
 * @req REQ-SAF-005
 */
gg_status_t scheduler_watchdog_ok(const scheduler_t *sched, bool *out_kick)
{
    gg_status_t status = GG_OK;

    if ((sched == NULL) || (out_kick == NULL)) {
        status = GG_ERR_NULL;
    } else {
        uint8_t i;
        bool all_done = true;

        for (i = 0u; i < sched->count; i++) {
            if (!sched->done[i]) {
                all_done = false;
            }
        }
        *out_kick = all_done;
    }
    return status;
}
