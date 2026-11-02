/**
 * @file timer_avr.c
 * @brief 1 ms tick for the ATmega328P, from Timer2 in CTC mode.
 *
 * How it works
 * ------------
 * Timer2 counts CPU clock / 64 = 250 kHz. In CTC mode it resets when it reaches
 * OCR2A, so with OCR2A = 249 a compare interrupt fires every 250 counts, that is
 * every 1 ms exactly. The interrupt adds 1 to a 32-bit counter. Reading the
 * counter needs ATOMIC_BLOCK: a 32-bit value takes four byte reads, and an
 * interrupt in the middle would give a torn value (R15). Timer2 is used, not
 * Timer0, because Timer0 drives the buzzer PWM on PD6 (OC0A).
 *
 * @req REQ-FUN-001 REQ-LOG-002
 */
#include <stddef.h>
#include <stdint.h>

#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/atomic.h>

#include "common/gg_types/gg_types.h"
#include "hal/timer/timer.h"

/** @brief Compare value for a 1 ms period: 16 MHz / 64 / 1000 - 1 = 249. */
#define TIMER_OCR2A_1MS 249u

/** @brief Milliseconds since init. Written by the interrupt, read by the main loop. */
static volatile uint32_t v_ms_ticks;

gg_status_t hal_timer_init(void)
{
    v_ms_ticks = 0u;
    TCCR2A = (uint8_t)(1u << WGM21);              /* CTC mode: clear on compare match */
    OCR2A = TIMER_OCR2A_1MS;
    TCCR2B = (uint8_t)(1u << CS22);               /* prescaler 64 */
    TIMSK2 = (uint8_t)(TIMSK2 | (1u << OCIE2A));  /* interrupt on compare match */
    return GG_OK;
}

gg_status_t hal_timer_get_ms(uint32_t *out_ms)
{
    gg_status_t status = GG_OK;

    if (out_ms == NULL) {
        status = GG_ERR_NULL;
    } else {
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
            *out_ms = v_ms_ticks;
        }
    }
    return status;
}

// DEV-07: avr-libc ISR() macro seen by cppcheck as an unused parameter
// cppcheck-suppress misra-c2012-2.7
// cppcheck-suppress misra-c2012-8.2
// cppcheck-suppress misra-c2012-8.4
// cppcheck-suppress misra-c2012-8.6
ISR(TIMER2_COMPA_vect)
{
    v_ms_ticks++;
}
