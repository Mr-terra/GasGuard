/**
 * @file filter.h
 * @brief Moving average of the last eight raw ADC samples of one channel.
 *
 * Each gas channel owns one filter_t. The filter smooths the reading before
 * the calibration and the alarm logic see it, so a single noisy sample cannot
 * raise an alarm on its own (FMEA F16).
 *
 * @req REQ-FUN-002
 */
#ifndef FILTER_H
// DEV-02: include guard, not an unused macro
// cppcheck-suppress misra-c2012-2.5
#define FILTER_H

#include <stdint.h>

#include "common/gg_types.h"

/** @brief Number of samples averaged: the last eight (REQ-FUN-002). */
#define FILTER_WINDOW 8u

/** @brief State of one channel's filter. Owned by the caller, one per channel. */
typedef struct {
    uint16_t samples[FILTER_WINDOW]; /**< Ring buffer of raw samples. */
    uint16_t sum;                    /**< Sum of the samples currently held. */
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
// DEV-05: service API, called by app/ and tests; cppcheck sees one caller at this stage
// cppcheck-suppress misra-c2012-8.7
gg_status_t filter_init(filter_t *filter);

/**
 * @brief Add one raw sample, dropping the oldest one once the window is full.
 *
 * @param filter  Filter to update. Must not be NULL.
 * @param raw     Raw ADC value, 0 .. HAL_ADC_RAW_MAX.
 * @return GG_OK, GG_ERR_NULL if filter is NULL, or GG_ERR_RANGE if raw is out of range.
 * @req REQ-FUN-002
 */
// DEV-05: service API, called by app/ and tests; cppcheck sees one caller at this stage
// cppcheck-suppress misra-c2012-8.7
gg_status_t filter_push(filter_t *filter, uint16_t raw);

/**
 * @brief Read the average of the samples currently held.
 *
 * @param filter     Filter to read. Must not be NULL.
 * @param out_value  Receives the average, in raw ADC units. Must not be NULL.
 * @return GG_OK, GG_ERR_NULL for a NULL argument, or GG_ERR_RANGE if no
 *         sample has been pushed yet (there is no average to report).
 * @req REQ-FUN-002
 */
// DEV-05: service API, called by app/ and tests; cppcheck sees one caller at this stage
// cppcheck-suppress misra-c2012-8.7
gg_status_t filter_value(const filter_t *filter, uint16_t *out_value);

#endif /* FILTER_H */
