/**
 * @file detector.c
 * @brief Alarm state machine: one switch on the state, one helper per state.
 *
 * How it works
 * ------------
 * Each sample updates three counters: samples at or above AL1, samples at or
 * above AL2, and samples below AL1 - hysteresis. The state then changes only
 * when a counter reaches its limit (3 samples to enter an alarm, 10 to clear
 * ALARM_1). A single spike therefore never alarms, and a reading that
 * oscillates inside the hysteresis band never chatters. ALARM_2 has no
 * automatic exit: only an operator reset, with the gas below the clear level,
 * returns to NORMAL. A fault input always wins: it latches FAULT from any state.
 * The relay is energised only in the healthy states, so every path to an alarm
 * or a fault releases it.
 *
 * @req REQ-ALM-001 REQ-ALM-002 REQ-ALM-003 REQ-ALM-004 REQ-ALM-005 REQ-ALM-006
 * @req REQ-SAF-001 REQ-SAF-007
 */
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "app/detector.h"
#include "common/gg_types.h"

/** @brief Concentration below which ALARM_1 may clear: al1 - hysteresis, in 0.1 %LEL. */
static int32_t clear_level(const detector_t *det)
{
    return (int32_t)det->thresholds.al1 - (int32_t)det->thresholds.hysteresis;
}

static void reset_counters(detector_t *det)
{
    det->above_al1 = 0u;
    det->above_al2 = 0u;
    det->below_clear = 0u;
}

/* Counters saturate at 255 so that a long alarm cannot wrap them back to zero. */
static void update_counters(detector_t *det, gg_conc_t conc)
{
    if ((int32_t)conc >= (int32_t)det->thresholds.al1) {
        if (det->above_al1 < UINT8_MAX) {
            det->above_al1++;
        }
    } else {
        det->above_al1 = 0u;
    }

    if ((int32_t)conc >= (int32_t)det->thresholds.al2) {
        if (det->above_al2 < UINT8_MAX) {
            det->above_al2++;
        }
    } else {
        det->above_al2 = 0u;
    }

    if ((int32_t)conc < clear_level(det)) {
        if (det->below_clear < UINT8_MAX) {
            det->below_clear++;
        }
    } else {
        det->below_clear = 0u;
    }
}

static void step_normal(detector_t *det)
{
    if (det->above_al1 >= DETECTOR_ENTER_SAMPLES) {
        det->state = DET_STATE_ALARM_1;
    }
}

static void step_alarm_1(detector_t *det)
{
    if (det->above_al2 >= DETECTOR_ENTER_SAMPLES) {
        det->state = DET_STATE_ALARM_2;
    } else if (det->below_clear >= DETECTOR_CLEAR_SAMPLES) {
        det->state = DET_STATE_NORMAL;
    } else {
        /* Still in ALARM_1: nothing to change. */
    }
}

/* REQ-ALM-004: the latch opens only on an operator request, and only with the gas below the clear level. */
static void step_alarm_2(detector_t *det, const detector_input_t *in)
{
    if (in->reset_requested && ((int32_t)in->conc < clear_level(det))) {
        det->state = DET_STATE_NORMAL;
    }
}

gg_status_t detector_thresholds_check(const detector_thresholds_t *thresholds)
{
    gg_status_t status = GG_OK;

    if (thresholds == NULL) {
        status = GG_ERR_NULL;
    } else if ((thresholds->al1 <= 0) || (thresholds->al1 >= thresholds->al2)
               || (thresholds->al2 > DETECTOR_AL2_MAX)
               || (thresholds->hysteresis < 0) || (thresholds->hysteresis >= thresholds->al1)) {
        status = GG_ERR_RANGE;
    } else {
        status = GG_OK;
    }
    return status;
}

gg_status_t detector_init(detector_t *det, const detector_thresholds_t *thresholds)
{
    gg_status_t status;

    if (det == NULL) {
        status = GG_ERR_NULL;
    } else {
        status = detector_thresholds_check(thresholds);
        if (status == GG_OK) {
            det->thresholds = *thresholds;
            det->state = DET_STATE_WARMUP;
            det->fault_code = DET_FAULT_NONE;
            reset_counters(det);
        }
    }
    return status;
}

gg_status_t detector_end_warmup(detector_t *det)
{
    gg_status_t status = GG_OK;

    if (det == NULL) {
        status = GG_ERR_NULL;
    } else if (det->state == DET_STATE_WARMUP) {
        det->state = DET_STATE_NORMAL;
        reset_counters(det);
    } else {
        /* Already past warm-up, or latched in FAULT: nothing to do. */
    }
    return status;
}

/* Latch FAULT and record why. The first reason is kept: a later one does not overwrite it.
 * Minimal version for Day 2: the event log that stores the reason arrives on Day 4. */
static void diag_raise(detector_t *det, detector_fault_code_t code)
{
    if (det->state != DET_STATE_FAULT) {
        det->state = DET_STATE_FAULT;
        det->fault_code = code;
    }
}

/* Dispatch to the handler of the current state. One switch, every enum value listed. */
static void step_state(detector_t *det, const detector_input_t *in)
{
    switch (det->state) {
        case DET_STATE_WARMUP:
            break;
        case DET_STATE_NORMAL:
            step_normal(det);
            break;
        case DET_STATE_ALARM_1:
            step_alarm_1(det);
            break;
        case DET_STATE_ALARM_2:
            step_alarm_2(det, in);
            break;
        case DET_STATE_FAULT:
            break;                /* Handled in detector_step(); listed for -Wswitch-enum. */
        default:
            /* Impossible state: fail safe. TODO(Day 4): also log it in the event log. */
            diag_raise(det, DET_FAULT_INTERNAL);
            break;
    }
}

gg_status_t detector_step(detector_t *det, const detector_input_t *in)
{
    gg_status_t status = GG_OK;

    if ((det == NULL) || (in == NULL)) {
        status = GG_ERR_NULL;
    } else if (in->fault) {
        diag_raise(det, DET_FAULT_INPUT);
    } else if (det->state == DET_STATE_FAULT) {
        /* Latched: only a new initialisation clears it. */
    } else {
        if (det->state == DET_STATE_WARMUP) {
            reset_counters(det);      /* REQ-ALM-006: no alarm counting during warm-up. */
        } else {
            update_counters(det, in->conc);
        }
        step_state(det, in);
    }
    return status;
}

gg_status_t detector_state(const detector_t *det, detector_state_t *state)
{
    gg_status_t status = GG_OK;

    if ((det == NULL) || (state == NULL)) {
        status = GG_ERR_NULL;
    } else {
        *state = det->state;
    }
    return status;
}

gg_status_t detector_fault_code(const detector_t *det, detector_fault_code_t *code)
{
    gg_status_t status = GG_OK;

    if ((det == NULL) || (code == NULL)) {
        status = GG_ERR_NULL;
    } else {
        *code = det->fault_code;
    }
    return status;
}

gg_status_t detector_relay_energised(const detector_t *det, bool *energised)
{
    gg_status_t status = GG_OK;

    if ((det == NULL) || (energised == NULL)) {
        status = GG_ERR_NULL;
    } else {
        *energised = (det->state == DET_STATE_WARMUP) || (det->state == DET_STATE_NORMAL);
    }
    return status;
}
