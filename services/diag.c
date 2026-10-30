/**
 * @file diag.c
 * @brief Open/short, stuck-at and bandgap diagnostics for the ADC channels.
 *
 * How it works
 * ------------
 * Three independent checks, each with its own state in diag_t.
 *
 * 1. Range: a raw value at or below 5 (open circuit: the pull-down holds the
 *    input at 0) or at or above 1018 (short circuit) is a bad sample. Five bad
 *    samples in a row latch a fault. A good sample resets the count.
 * 2. Stuck: the channel keeps a minimum and a maximum since its window started.
 *    A move of more than 1 LSB restarts the window. Ten minutes without a move,
 *    outside warm-up, latch a fault: a heater that stopped or a frozen ADC.
 * 3. Bandgap: the internal 1.1 V reference should read close to its nominal
 *    count. A reading outside +/-10 % means the ADC reference is wrong.
 *
 * Faults are latched: nothing clears them except diag_init(). That is the
 * conservative choice for a safety device.
 *
 * @req REQ-SAF-002 REQ-SAF-003 REQ-SAF-004
 */
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "common/gg_types.h"
#include "hal/hal_adc.h"
#include "services/diag.h"

gg_status_t diag_init(diag_t *diag)
{
    gg_status_t status = GG_OK;

    if (diag == NULL) {
        status = GG_ERR_NULL;
    } else {
        diag->range_count = 0u;
        diag->range_fault = false;
        diag->window_min = 0u;
        diag->window_max = 0u;
        diag->window_start_ms = 0u;
        diag->window_started = false;
        diag->stuck_fault = false;
    }
    return status;
}

gg_status_t diag_check_range(diag_t *diag, uint16_t raw)
{
    gg_status_t status = GG_OK;
    bool out_of_range;

    if (diag == NULL) {
        status = GG_ERR_NULL;
    } else if (raw > HAL_ADC_RAW_MAX) {
        status = GG_ERR_RANGE;
    } else {
        out_of_range = (raw <= DIAG_OPEN_MAX) || (raw >= DIAG_SHORT_MIN);
        if (out_of_range) {
            if (diag->range_count < DIAG_RANGE_SAMPLES) {
                diag->range_count++;
            }
            if (diag->range_count >= DIAG_RANGE_SAMPLES) {
                diag->range_fault = true;
            }
        } else {
            diag->range_count = 0u;
        }
    }
    return status;
}

/* Start a new stuck window at the current sample. */
static void stuck_window_restart(diag_t *diag, const diag_stuck_sample_t *sample)
{
    diag->window_started = true;
    diag->window_start_ms = sample->now_ms;
    diag->window_min = sample->raw;
    diag->window_max = sample->raw;
}

/* Widen the window with one sample. Restart it if the value moved, latch a fault if it did not move for too long. */
static void stuck_window_update(diag_t *diag, const diag_stuck_sample_t *sample)
{
    uint16_t new_min = (sample->raw < diag->window_min) ? sample->raw : diag->window_min;
    uint16_t new_max = (sample->raw > diag->window_max) ? sample->raw : diag->window_max;
    bool moved = ((int32_t)new_max - (int32_t)new_min) > (int32_t)DIAG_STUCK_LSB;

    if (moved) {
        stuck_window_restart(diag, sample);
    } else {
        diag->window_min = new_min;
        diag->window_max = new_max;
        if ((sample->now_ms - diag->window_start_ms) >= DIAG_STUCK_MS) {
            diag->stuck_fault = true;
        }
    }
}

gg_status_t diag_check_stuck(diag_t *diag, const diag_stuck_sample_t *sample)
{
    gg_status_t status = GG_OK;

    if ((diag == NULL) || (sample == NULL)) {
        status = GG_ERR_NULL;
    } else if (sample->raw > HAL_ADC_RAW_MAX) {
        status = GG_ERR_RANGE;
    } else if (sample->in_warmup || !diag->window_started) {
        /* No stuck decision during warm-up, or before the first sample. */
        stuck_window_restart(diag, sample);
    } else {
        stuck_window_update(diag, sample);
    }
    return status;
}

gg_status_t diag_is_faulty(const diag_t *diag, bool *out_fault)
{
    gg_status_t status = GG_OK;

    if ((diag == NULL) || (out_fault == NULL)) {
        status = GG_ERR_NULL;
    } else {
        *out_fault = diag->range_fault || diag->stuck_fault;
    }
    return status;
}

gg_status_t diag_bandgap_ok(uint16_t bandgap_raw, bool *out_ok)
{
    gg_status_t status = GG_OK;
    int32_t delta;

    if (out_ok == NULL) {
        status = GG_ERR_NULL;
    } else if (bandgap_raw > HAL_ADC_RAW_MAX) {
        status = GG_ERR_RANGE;
    } else {
        delta = (int32_t)bandgap_raw - (int32_t)DIAG_BANDGAP_NOMINAL;
        if (delta < 0) {
            delta = -delta;
        }
        /* |delta| / nominal <= tolerance, written without division or floating point. */
        *out_ok = ((delta * 100) <= ((int32_t)DIAG_BANDGAP_NOMINAL * (int32_t)DIAG_BANDGAP_TOLERANCE_PCT));
    }
    return status;
}
