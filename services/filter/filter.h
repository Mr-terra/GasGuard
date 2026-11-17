/**
 * @file filter.h
 * @brief Median of the last three raw ADC samples, then moving average of the
 *        last eight medians, for one channel.
 *
 * Each gas channel owns one filter_t. The filter smooths the reading before
 * the calibration and the alarm logic see it. The median removes a single
 * spike completely, and the average smooths the rest, so a single noisy
 * sample cannot raise an alarm (FMEA F16).
 *
 * @req REQ-FUN-002
 */
#ifndef FILTER_H
// DEV-02: include guard, not an unused macro
// cppcheck-suppress misra-c2012-2.5
#define FILTER_H

#include <stdint.h>

#include "common/gg_types/gg_types.h"

/** @brief Number of medians averaged: the last eight (REQ-FUN-002). */
#define FILTER_WINDOW 8u

/** @brief Number of raw samples of each median: the last three (REQ-FUN-002). */
#define FILTER_MEDIAN_SIZE 3u

/** @brief State of one channel's filter. Owned by the caller, one per channel. */
typedef struct {
    uint16_t raw[FILTER_MEDIAN_SIZE]; /**< Ring buffer of the last raw samples, for the median. */
    uint8_t raw_next;                 /**< Index where the next raw sample is written. */
    uint8_t raw_count;                /**< Number of raw samples held, up to FILTER_MEDIAN_SIZE. */
    uint16_t samples[FILTER_WINDOW]; /**< Ring buffer of medians. */
    uint16_t sum;                    /**< Running sum of the window, updated per sample. */
    uint8_t next;                    /**< Index where the next sample is written. */
    uint8_t count;                   /**< Number of valid samples, up to FILTER_WINDOW. */
} filter_t;

/**
 * @brief Clear a filter so that it holds no samples.
 *
 * @param filter  Filter to initialise. Must not be NULL.
 * @return GG_OK, or GG_ERR_NULL if filter is NULL.
 * @req REQ-FUN-002
 */
gg_status_t filter_init(filter_t *filter);

/**
 * @brief Add one raw sample: take the median of the last three raw samples and
 *        add it to the average, dropping the oldest median once the window is full.
 *
 * Until three raw samples are held, the "median" is the only sample, then the
 * lower of the two. So the first sample after filter_init() is passed on as it
 * is, and a wild one there is not removed: call filter_init() only where that
 * sample cannot raise an alarm (the firmware does it at boot, before the warm-up).
 *
 * @param filter  Filter to update. Must not be NULL.
 * @param raw     Raw ADC value, 0 .. HAL_ADC_RAW_MAX.
 * @return GG_OK, GG_ERR_NULL if filter is NULL, or GG_ERR_RANGE if raw is out of range.
 * @req REQ-FUN-002
 */
gg_status_t filter_push(filter_t *filter, uint16_t raw);

/**
 * @brief Read the average of the medians currently held.
 *
 * @param filter     Filter to read. Must not be NULL.
 * @param out_value  Receives the average, in raw ADC units. Must not be NULL.
 * @return GG_OK, GG_ERR_NULL for a NULL argument, or GG_ERR_RANGE if no
 *         sample has been pushed yet (there is no average to report).
 * @req REQ-FUN-002
 */
gg_status_t filter_value(const filter_t *filter, uint16_t *out_value);

#endif /* FILTER_H */
