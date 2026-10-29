/**
 * @file hal_reset_cause_fake.c
 * @brief Reset cause held in RAM, set by the test.
 *
 * How it works
 * ------------
 * On the board, the cause is read from the MCU status register once at boot.
 * The fake keeps one value, which the test sets to the cause it wants to
 * simulate (for example a watchdog reset). The boot code then logs and
 * displays it exactly as it would on hardware. The default is a power-on
 * reset, the normal situation after a fresh start.
 *
 * @req REQ-SAF-006
 */
#include "hal/hal_reset_cause.h"
#include "hal/host/hal_reset_cause_fake.h"

static hal_reset_cause_t s_reset_cause_fake = HAL_RESET_POWER_ON;

void hal_reset_cause_fake_reset(void)
{
    s_reset_cause_fake = HAL_RESET_POWER_ON;
}

void hal_reset_cause_fake_set(hal_reset_cause_t cause)
{
    s_reset_cause_fake = cause;
}

hal_reset_cause_t hal_reset_cause_get(void)
{
    return s_reset_cause_fake;
}
