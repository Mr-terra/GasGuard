/**
 * @file wdt_avr.c
 * @brief Hardware watchdog for the ATmega328P, with a 500 ms timeout.
 *
 * How it works
 * ------------
 * The watchdog is a separate timer that resets the MCU when it expires. The
 * firmware enables it once, then restarts ("kicks") it at the end of each main
 * loop pass. If the loop stops, the kicks stop, and after 500 ms the MCU resets
 * (REQ-SAF-005). The kick comes last in the loop, after every task has run, so a
 * blocked task is detected even if the rest of the loop still spins. The avr-libc
 * macros take care of the timed-sequence rules of the WDT registers.
 *
 * @req REQ-SAF-005
 */
#include <stdint.h>

#include <avr/io.h>
#include <avr/wdt.h>

#include "common/gg_types/gg_types.h"
#include "hal/wdt/wdt.h"

gg_status_t hal_wdt_enable(void)
{
    wdt_enable(WDTO_500MS);
    return GG_OK;
}

gg_status_t hal_wdt_kick(void)
{
    wdt_reset();
    return GG_OK;
}
