/**
 * @file hal_wdt_fake.c
 * @brief Watchdog model: enable flag and a count of kicks, nothing more.
 *
 * How it works
 * ------------
 * On the board, the watchdog resets the chip if the main loop stops kicking
 * it. The fake cannot reset anything, so it records what the test needs to
 * check: that the watchdog was enabled, and how many times the loop kicked
 * it. The reset itself is exercised on the bench (docs/05-verification-plan.md).
 *
 * @req REQ-SAF-005
 */
#include <stdbool.h>
#include <stdint.h>

#include "common/gg_types.h"
#include "hal/hal_wdt.h"
#include "hal/host/hal_wdt_fake.h"

/** @brief State of the fake watchdog. */
typedef struct {
    bool enabled;
    uint16_t kicks;
} wdt_fake_state_t;

static wdt_fake_state_t s_wdt_fake;

void hal_wdt_fake_reset(void)
{
    s_wdt_fake.enabled = false;
    s_wdt_fake.kicks = 0u;
}

bool hal_wdt_fake_is_enabled(void)
{
    return s_wdt_fake.enabled;
}

uint16_t hal_wdt_fake_kicks(void)
{
    return s_wdt_fake.kicks;
}

gg_status_t hal_wdt_enable(void)
{
    s_wdt_fake.enabled = true;
    return GG_OK;
}

gg_status_t hal_wdt_kick(void)
{
    s_wdt_fake.kicks++;
    return GG_OK;
}
