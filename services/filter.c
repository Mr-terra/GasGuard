/**
 * @file filter.c
 * @brief Moving average over a ring buffer of eight raw samples.
 *
 * How it works
 * ------------
 * The filter keeps the last eight raw ADC values in a ring buffer and a running
 * sum. A new sample replaces the oldest one: the oldest value is subtracted
 * from the sum, the new one is added, and the write index moves forward. The
 * average is the sum divided by the number of valid samples. Because the sum
 * is updated incrementally, each call costs a constant amount of work, and no
 * floating point is needed. A single spike is diluted by eight, so one noisy
 * sample cannot trigger an alarm on its own (REQ-FUN-002, FMEA F16).
 *
 * @req REQ-FUN-002
 */
#include <stddef.h>
#include <stdint.h>

#include "common/gg_types.h"
#include "hal/hal_adc.h"
#include "services/filter.h"

gg_status_t filter_init(filter_t *filter)
{
    gg_status_t status = GG_OK;

    if (filter == NULL) {
        status = GG_ERR_NULL;
    } else {
        for (uint8_t i = 0u; i < FILTER_WINDOW; i++) {
            filter->samples[i] = 0u;
        }
        filter->sum = 0u;
        filter->next = 0u;
        filter->count = 0u;
    }
    return status;
}

gg_status_t filter_push(filter_t *filter, uint16_t raw)
{
    gg_status_t status = GG_OK;
    uint16_t oldest;

    if (filter == NULL) {
        status = GG_ERR_NULL;
    } else if (raw > HAL_ADC_RAW_MAX) {
        status = GG_ERR_RANGE;
    } else {
        if (filter->count == FILTER_WINDOW) {
            /* Window full: the slot at 'next' holds the oldest sample. */
            oldest = filter->samples[filter->next];
            filter->sum = (uint16_t)(filter->sum - oldest);
        } else {
            filter->count++;
        }
        filter->samples[filter->next] = raw;
        filter->sum = (uint16_t)(filter->sum + raw);
        filter->next = (uint8_t)((filter->next + 1u) % FILTER_WINDOW);
    }
    return status;
}

gg_status_t filter_value(const filter_t *filter, uint16_t *out_value)
{
    gg_status_t status = GG_OK;

    if ((filter == NULL) || (out_value == NULL)) {
        status = GG_ERR_NULL;
    } else if (filter->count == 0u) {
        status = GG_ERR_RANGE;
    } else {
        *out_value = (uint16_t)(filter->sum / filter->count);
    }
    return status;
}
