/**
 * @file filter.c
 * @brief Median of three, then moving average over a ring buffer of eight medians.
 *
 * How it works
 * ------------
 * Each raw sample first goes into a ring of the last three raw values, and the
 * middle one of the three (the median) is kept. A single wild sample, up or
 * down, is never the middle one, so it is removed completely; real gas lasts
 * far longer than one 100 ms sample, so it is only followed one sample later.
 * The medians then go into a ring of eight with a running sum: the oldest one
 * is subtracted, the new one added. The average is the sum divided by the
 * number of medians held. Each call costs a constant amount of work, with no
 * floating point (REQ-FUN-002, FMEA F16).
 *
 * @req REQ-FUN-002
 */
#include <stddef.h>
#include <stdint.h>

#include "common/gg_types/gg_types.h"
#include "hal/adc/adc.h"
#include "services/filter/filter.h"

/** @brief Number of raw samples held when the median is the lower of two. */
#define FILTER_MEDIAN_PAIR 2u

/* median_push() reads raw[0], raw[1] and raw[2] by index: it is written for a
 * window of exactly three. A different FILTER_MEDIAN_SIZE stops the build. */
#if FILTER_MEDIAN_SIZE != 3u
#error "median_push() is written for FILTER_MEDIAN_SIZE == 3"
#endif

gg_status_t filter_init(filter_t *filter)
{
    gg_status_t status = GG_OK;

    if (filter == NULL) {
        status = GG_ERR_NULL;
    } else {
        for (uint8_t i = 0u; i < FILTER_WINDOW; i++) {
            filter->samples[i] = 0u;
        }
        for (uint8_t i = 0u; i < FILTER_MEDIAN_SIZE; i++) {
            filter->raw[i] = 0u;
        }
        filter->raw_next = 0u;
        filter->raw_count = 0u;
        filter->sum = 0u;
        filter->next = 0u;
        filter->count = 0u;
    }
    return status;
}

/**
 * @brief Smaller of two values.
 * @param a  First value.
 * @param b  Second value.
 * @return The smaller one.
 * @req REQ-FUN-002
 */
static uint16_t min_u16(uint16_t a, uint16_t b)
{
    return (a < b) ? a : b;
}

/**
 * @brief Larger of two values.
 * @param a  First value.
 * @param b  Second value.
 * @return The larger one.
 * @req REQ-FUN-002
 */
static uint16_t max_u16(uint16_t a, uint16_t b)
{
    return (a > b) ? a : b;
}

/**
 * @brief Store a raw sample and return the median of the raw samples held.
 *
 * Three samples: the middle one. Two: the lower one. One: that sample.
 * @param filter  Filter to update.
 * @param raw     Raw sample, already checked.
 * @return The median.
 * @req REQ-FUN-002
 */
static uint16_t median_push(filter_t *filter, uint16_t raw)
{
    uint16_t median = raw;

    filter->raw[filter->raw_next] = raw;
    filter->raw_next = (uint8_t)((filter->raw_next + 1u) % FILTER_MEDIAN_SIZE);
    if (filter->raw_count < FILTER_MEDIAN_SIZE) {
        filter->raw_count++;
    }
    if (filter->raw_count == FILTER_MEDIAN_PAIR) {
        median = min_u16(filter->raw[0], filter->raw[1]);
    } else if (filter->raw_count == FILTER_MEDIAN_SIZE) {
        uint16_t a = filter->raw[0];
        uint16_t b = filter->raw[1];
        uint16_t c = filter->raw[2];

        /* The middle of three: the larger of (the smaller of a and b) and (the smaller
         * of the larger of a and b, and c). */
        median = max_u16(min_u16(a, b), min_u16(max_u16(a, b), c));
    } else {
        /* One sample: it is its own median. */
    }
    return median;
}

gg_status_t filter_push(filter_t *filter, uint16_t raw)
{
    gg_status_t status = GG_OK;
    uint16_t oldest;
    uint16_t median;

    if (filter == NULL) {
        status = GG_ERR_NULL;
    } else if (raw > HAL_ADC_RAW_MAX) {
        status = GG_ERR_RANGE;
    } else {
        median = median_push(filter, raw);
        if (filter->count == FILTER_WINDOW) {
            /* Window full: the slot at 'next' holds the oldest median. */
            oldest = filter->samples[filter->next];
            filter->sum = (uint16_t)(filter->sum - oldest);
        } else {
            filter->count++;
        }
        filter->samples[filter->next] = median;
        filter->sum = (uint16_t)(filter->sum + median);
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
