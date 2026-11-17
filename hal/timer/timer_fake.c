/**
 * @file timer_fake.c
 * @brief Scriptable uptime clock for host tests.
 *
 * How it works
 * ------------
 * The fake keeps one uptime value in a static struct. The test sets it with
 * hal_timer_fake_set_ms(), and the code under test reads it through
 * hal_timer_get_ms(), exactly as on the board. Time never moves by itself, so
 * a test decides when a second has passed. That keeps timing tests
 * deterministic. For code that waits in a loop on the clock, a test can set a
 * step that is added after each read, so the loop ends.
 *
 * @req REQ-LOG-002
 */
#include <stddef.h>
#include <stdint.h>

#include "common/gg_types/gg_types.h"
#include "hal/timer/timer.h"
#include "hal/timer/timer_fake.h"

typedef struct {
    uint32_t ms;       /**< Uptime the next read reports. */
    uint32_t step_ms;  /**< Added after each read; 0 keeps the clock still. */
} timer_fake_state_t;

static timer_fake_state_t s_timer_fake;

void hal_timer_fake_reset(void)
{
    s_timer_fake.ms = 0u;
    s_timer_fake.step_ms = 0u;
}

void hal_timer_fake_set_ms(uint32_t ms)
{
    s_timer_fake.ms = ms;
}

void hal_timer_fake_set_step(uint32_t step_ms)
{
    s_timer_fake.step_ms = step_ms;
}

gg_status_t hal_timer_init(void)
{
    return GG_OK;
}

gg_status_t hal_timer_get_ms(uint32_t *out_ms)
{
    gg_status_t status = GG_OK;

    if (out_ms == NULL) {
        status = GG_ERR_NULL;
    } else {
        *out_ms = s_timer_fake.ms;
        s_timer_fake.ms += s_timer_fake.step_ms;
    }
    return status;
}
