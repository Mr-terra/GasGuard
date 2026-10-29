/**
 * @file hal_timer_fake.c
 * @brief Scriptable uptime clock for host tests.
 *
 * How it works
 * ------------
 * The fake keeps one uptime value in a static struct. The test sets it with
 * hal_timer_fake_set_ms(), and the code under test reads it through
 * hal_timer_get_ms(), exactly as on the board. Time never moves by itself, so
 * a test decides when a second has passed. That keeps timing tests
 * deterministic.
 *
 * @req REQ-LOG-002
 */
#include <stddef.h>
#include <stdint.h>

#include "common/gg_types.h"
#include "hal/hal_timer.h"
#include "hal/host/hal_timer_fake.h"

/** @brief State of the fake clock. */
typedef struct {
    uint32_t ms;
} timer_fake_state_t;

static timer_fake_state_t s_timer_fake;

void hal_timer_fake_reset(void)
{
    s_timer_fake.ms = 0u;
}

void hal_timer_fake_set_ms(uint32_t ms)
{
    s_timer_fake.ms = ms;
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
    }
    return status;
}
