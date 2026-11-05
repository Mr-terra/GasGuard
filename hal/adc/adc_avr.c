/**
 * @file adc_avr.c
 * @brief 10-bit ADC reading of the eight analog inputs of the ATmega328P.
 *
 * How it works
 * ------------
 * The ADC is clocked at 125 kHz (prescaler 128), which gives a conversion in
 * about 104 us. A read selects the channel in ADMUX, runs one conversion that is
 * thrown away, then runs a second one that is kept: the first conversion after
 * a channel change is inaccurate (datasheet). Each conversion waits in a bounded
 * loop, so a stuck ADC returns GG_ERR_TIMEOUT instead of hanging. The reference
 * is AVcc (REFS0), so the reading depends on the supply voltage: the bandgap
 * check (REQ-SAF-004) watches for that.
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

/** @brief MUX bits that select the internal 1.1 V bandgap reference (datasheet table 24-4). */
#define ADC_MUX_BANDGAP 0x0Eu

/**
 * @brief Run one conversion on the selected channel and wait for it.
 *
 * @param out_raw  Receives the 10-bit result. Must not be NULL.
 * @return GG_OK, or GG_ERR_TIMEOUT if the conversion does not finish in time.
 */
static gg_status_t adc_convert(uint16_t *out_raw)
{
    gg_status_t status = GG_OK;
    uint16_t waited = 0u;

    ADCSRA = (uint8_t)(ADCSRA | (1u << ADSC));              /* start one conversion */
    while (((ADCSRA & (1u << ADSC)) != 0u) && (waited < ADC_WAIT_LOOPS)) {
        waited++;
    }
    if ((ADCSRA & (1u << ADSC)) != 0u) {
        status = GG_ERR_TIMEOUT;
    } else {
        *out_raw = (uint16_t)(ADC & HAL_ADC_RAW_MAX);
    }
    return status;
}

/**
 * @brief Read one analog channel as a raw 10-bit value.
 *
 * @param channel  Channel index, 0 .. HAL_ADC_CHANNEL_COUNT - 1.
 * @param out_raw  Receives the raw value. Must not be NULL.
 * @return GG_OK, GG_ERR_NULL, GG_ERR_RANGE for a bad channel, or GG_ERR_TIMEOUT.
 * @req REQ-FUN-001
 */
/**
 * @brief Select a multiplexer input against AVcc and read it, discarding the first conversion.
 * @param mux_bits  Value for the MUX bits of ADMUX: a channel 0..7, or ADC_MUX_BANDGAP.
 * @param out_raw   Receives the raw value. Already checked to be non-NULL.
 * @return GG_OK, or GG_ERR_TIMEOUT if a conversion does not finish.
 */
static gg_status_t adc_read_mux(uint8_t mux_bits, uint16_t *out_raw)
{
    gg_status_t status = GG_OK;
    uint16_t discarded = 0u;

    ADMUX = (uint8_t)((1u << REFS0) | mux_bits);           /* AVcc reference */
    ADCSRA = (uint8_t)((1u << ADEN) | (1u << ADPS2) | (1u << ADPS1) | (1u << ADPS0));
    status = adc_convert(&discarded);                      /* first one: discarded */
    if (status == GG_OK) {
        status = adc_convert(out_raw);
    }
    return status;
}

gg_status_t hal_adc_read(uint8_t channel, uint16_t *out_raw)
{
    gg_status_t status = GG_OK;

    if (out_raw == NULL) {
        status = GG_ERR_NULL;
    } else if (channel >= HAL_ADC_CHANNEL_COUNT) {
        status = GG_ERR_RANGE;
    } else {
        status = adc_read_mux(channel, out_raw);
    }
    return status;
}

gg_status_t hal_adc_read_bandgap(uint16_t *out_raw)
{
    gg_status_t status = GG_OK;

    if (out_raw == NULL) {
        status = GG_ERR_NULL;
    } else {
        status = adc_read_mux(ADC_MUX_BANDGAP, out_raw);
    }
    return status;
}
