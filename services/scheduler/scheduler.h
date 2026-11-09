/**
 * @file scheduler.h
 * @brief Cooperative scheduler: periodic tasks, overrun detection, watchdog kick policy.
 *
 * The main loop runs one cycle at a time: begin_cycle, then each task checks
 * whether it is due and reports that it ran, then end_cycle measures the cycle.
 * The watchdog is kicked only when every task has run in the cycle (REQ-SAF-005).
 * A cycle longer than 100 ms is an overrun; three in a row raise FAULT (REQ-SAF-008).
 *
 * @req REQ-SAF-005 REQ-SAF-008
 */
#ifndef SCHEDULER_H
// DEV-02: include guard, not an unused macro
// cppcheck-suppress misra-c2012-2.5
#define SCHEDULER_H

#include <stdbool.h>
#include <stdint.h>

#include "common/gg_types/gg_types.h"

/** @brief Largest number of periodic tasks. */
#define SCHED_MAX_TASKS 8u
/** @brief A cycle longer than this is an overrun, in milliseconds (REQ-SAF-008). */
#define SCHED_OVERRUN_MS 100u
/** @brief Consecutive overruns that raise FAULT (REQ-SAF-008). */
#define SCHED_OVERRUN_FAULT_COUNT 3u

/** @brief Scheduler state. Owned by the caller; one instance for the whole program. */
typedef struct {
    uint32_t period_ms[SCHED_MAX_TASKS];   /**< Period of each task. */
    uint32_t next_due_ms[SCHED_MAX_TASKS]; /**< Time at which each task is next due. */
    bool done[SCHED_MAX_TASKS];            /**< True when the task ran in this cycle. */
    uint8_t count;                         /**< Number of tasks in use. */
    uint32_t cycle_start_ms;               /**< Start of the current cycle. */
    uint8_t overruns;                      /**< Consecutive overruns so far. */
} scheduler_t;

/**
 * @brief Set up the scheduler with a list of task periods.
 * @param sched      Scheduler state. Must not be NULL.
 * @param count      Number of tasks, 1 .. SCHED_MAX_TASKS.
 * @param periods    Period of each task in milliseconds, count entries. Must not be NULL.
 * @param now_ms     Current time; every task is due at once.
 * @return GG_OK, GG_ERR_NULL, or GG_ERR_RANGE if count is out of range.
 * @req REQ-SAF-005
 */
gg_status_t scheduler_init(scheduler_t *sched, uint8_t count, const uint32_t *periods, uint32_t now_ms);

/**
 * @brief Start a cycle: clear the "ran" flags and record the start time.
 * @param sched   Scheduler state. Must not be NULL.
 * @param now_ms  Current time.
 * @return GG_OK or GG_ERR_NULL.
 * @req REQ-SAF-005
 */
gg_status_t scheduler_begin_cycle(scheduler_t *sched, uint32_t now_ms);

/**
 * @brief Check whether a task is due, and advance its next due time if it is.
 * @param sched   Scheduler state. Must not be NULL.
 * @param task    Task index, 0 .. count - 1.
 * @param now_ms  Current time.
 * @param due     Receives true when the task must run now. Must not be NULL.
 * @return GG_OK, GG_ERR_NULL, or GG_ERR_RANGE for a bad task index.
 * @req REQ-SAF-005
 */
gg_status_t scheduler_task_due(scheduler_t *sched, uint8_t task, uint32_t now_ms, bool *due);

/**
 * @brief Report that a task ran in this cycle.
 * @param sched  Scheduler state. Must not be NULL.
 * @param task   Task index, 0 .. count - 1.
 * @return GG_OK, GG_ERR_NULL, or GG_ERR_RANGE for a bad task index.
 * @req REQ-SAF-005
 */
gg_status_t scheduler_task_done(scheduler_t *sched, uint8_t task);

/**
 * @brief End a cycle: measure it and count overruns.
 * @param sched       Scheduler state. Must not be NULL.
 * @param now_ms      Current time.
 * @param out_overrun Receives true when this cycle took longer than SCHED_OVERRUN_MS.
 * @param out_fault   Receives true when SCHED_OVERRUN_FAULT_COUNT overruns came in a row.
 * @return GG_OK, or GG_ERR_NULL if a pointer is NULL.
 * @req REQ-SAF-008
 */
gg_status_t scheduler_end_cycle(scheduler_t *sched, uint32_t now_ms, bool *out_overrun, bool *out_fault);

/**
 * @brief Tell whether the watchdog may be kicked now: every task has run in this cycle.
 * @param sched     Scheduler state. Must not be NULL.
 * @param out_kick  Receives true when the watchdog may be kicked. Must not be NULL.
 * @return GG_OK or GG_ERR_NULL.
 * @req REQ-SAF-005
 */
// DEV-05: service/driver/app API, called by app/, main.c and tests; cppcheck sees one caller at this stage
// cppcheck-suppress misra-c2012-8.7
gg_status_t scheduler_watchdog_ok(const scheduler_t *sched, bool *out_kick);

#endif /* SCHEDULER_H */
