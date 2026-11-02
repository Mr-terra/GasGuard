/**
 * @file reset_cause_avr.c
 * @brief Reads why the MCU last reset, once, at boot.
 *
 * How it works
 * ------------
 * The MCU status register MCUSR keeps one flag per reset source. The first call
 * reads the register, clears it so that the next reset starts from a clean state,
 * and stores the answer. Exactly one flag set gives that cause. No flag or
 * several flags is reported as HAL_RESET_UNKNOWN: the reset cannot be explained,
 * which is treated as suspicious (REQ-SAF-006). Later calls return the stored
 * answer, because the register was already cleared.
 *
 * @req REQ-SAF-006
 */
#include <stdbool.h>
#include <stdint.h>

#include <avr/io.h>

#include "hal/reset_cause/reset_cause.h"

hal_reset_cause_t hal_reset_cause_get(void)
{
    /* Cached result of the first read of MCUSR (the register is cleared by that read). */
    static bool read_done = false;
    static hal_reset_cause_t cached = HAL_RESET_UNKNOWN;
    hal_reset_cause_t cause = HAL_RESET_UNKNOWN;

    if (!read_done) {
        uint8_t flags = MCUSR;
        uint8_t set = 0u;
        MCUSR = 0u;              /* clear the flags so the next reset is clean */

        if ((flags & (uint8_t)(1u << PORF)) != 0u) { set++; cause = HAL_RESET_POWER_ON; }
        if ((flags & (uint8_t)(1u << EXTRF)) != 0u) { set++; cause = HAL_RESET_EXTERNAL; }
        if ((flags & (uint8_t)(1u << BORF)) != 0u) { set++; cause = HAL_RESET_BROWN_OUT; }
        if ((flags & (uint8_t)(1u << WDRF)) != 0u) { set++; cause = HAL_RESET_WATCHDOG; }

        cached = (set == 1u) ? cause : HAL_RESET_UNKNOWN;
        read_done = true;
    }
    return cached;
}
