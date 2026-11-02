/**
 * @file adc_avr.c
 * @brief 10-bit ADC reading of the eight analog inputs of the ATmega328P.
 *
 * How it works
 * ------------
 * The ADC is clocked at 125 kHz (prescaler 128), which gives a conversion in
 * about 104 us. A read selects the channel in ADMUX, starts one conversion, and
 * waits for it to finish. The wait is bounded: if the ADC never completes, the
 * function returns GG_ERR_TIMEOUT instead of hanging, and the watchdog is not
 * needed to get out of it. The reference is AVcc (REFS0), so the reading depends
 * on the supply voltage: the bandgap check (REQ-SAF-004) watches for that.
 *
 * @req REQ-FUN-001 REQ-SAF-002
 */
#include <stddef.h>
#include <stdint.h>

#include <avr/io.h>

#include "common/gg_types/gg_types.h"
#include "hal/adc/adc.h"

/** @brief Upper bound on the busy-wait for one conversion (about 100 us needed). */
#define ADC_WAIT_LOOPS 10000u

gg_status_t hal_adc_read(uint8_t channel, uint16_t *out_raw)
{
    gg_status_t status = GG_OK;
    uint16_t waited = 0u;

    if (out_raw == NULL) {
        status = GG_ERR_NULL;
    } else if (channel >= HAL_ADC_CHANNEL_COUNT) {
        status = GG_ERR_RANGE;
    } else {
        ADMUX = (uint8_t)((1u << REFS0) | channel);            /* AVcc reference */
        ADCSRA = (uint8_t)((1u << ADEN) | (1u << ADPS2) | (1u << ADPS1) | (1u << ADPS0));
        ADCSRA = (uint8_t)(ADCSRA | (1u << ADSC));              /* start one conversion */

        while (((ADCSRA & (1u << ADSC)) != 0u) && (waited < ADC_WAIT_LOOPS)) {
            waited++;
        }
        if ((ADCSRA & (1u << ADSC)) != 0u) {
            status = GG_ERR_TIMEOUT;
        } else {
            *out_raw = (uint16_t)(ADC & HAL_ADC_RAW_MAX);
        }
    }
    return status;
}
