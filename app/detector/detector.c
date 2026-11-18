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
 * One exception: a FAULT whose only cause is stuck channels (calm air) ends by
 * itself as soon as a channel is in use again, back to the previous state or
 * straight to the alarm when the gas is above a threshold (REQ-SAF-007).
 * The relay is energised only in the healthy states (WARMUP, NORMAL) and in
 * MAINTENANCE, which holds the last healthy state, so every path to an alarm
 * or a fault releases it.
 *
 * @req REQ-ALM-001 REQ-ALM-002 REQ-ALM-003 REQ-ALM-004 REQ-ALM-005 REQ-ALM-006
 * @req REQ-SAF-001 REQ-SAF-007 REQ-HMI-008 REQ-HMI-009
 */
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "app/detector/detector.h"
#include "common/gg_types/gg_types.h"

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

/* REQ-HMI-008: a touch of 3 s in NORMAL starts MAINTENANCE, unless the gas is at or
 * above the ALARM_1 clear level (AL1 - hysteresis): the request is then dropped. A
 * dip of less than the hysteresis below AL1 is still refused; a dip below the clear
 * level is the accepted residual risk (FMEA F36). An alarm level starts its alarm:
 * AL2 is checked first, so a sudden large leak reaches ALARM_2 after three samples
 * (REQ-ALM-002), not one sample later through ALARM_1. */
static void step_normal(detector_t *det, const detector_input_t *in)
{
    bool gas_present = (int32_t)in->conc >= clear_level(det);

    if (in->maint_requested && !gas_present) {
        det->state = DET_STATE_MAINTENANCE;
    } else if (det->above_al2 >= DETECTOR_ENTER_SAMPLES) {
        det->state = DET_STATE_ALARM_2;
    } else if (det->above_al1 >= DETECTOR_ENTER_SAMPLES) {
        det->state = DET_STATE_ALARM_1;
    } else {
        /* Below the alarm level and no request: the state does not change. */
    }
}

/* REQ-HMI-009: alarms are neither counted nor output in MAINTENANCE. REQ-HMI-010: the timer ends it. */
static void step_maintenance(detector_t *det, const detector_input_t *in)
{
    if (in->maint_expired) {
        det->state = DET_STATE_NORMAL;
        reset_counters(det);
    }
}

static void step_alarm_1(detector_t *det)
{
    if (det->above_al2 >= DETECTOR_ENTER_SAMPLES) {
        det->state = DET_STATE_ALARM_2;
    } else if (det->below_clear >= DETECTOR_CLEAR_SAMPLES) {
        det->state = DET_STATE_NORMAL;
    } else {
        /* Still above the clear level: ALARM_1 is held (hysteresis). */
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
            det->state_before_fault = DET_STATE_WARMUP;
            det->fault_new = false;
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
    } else if ((det->state == DET_STATE_FAULT) && (det->state_before_fault == DET_STATE_WARMUP)) {
        /* A stuck-only FAULT that ends after the warm-up returns to NORMAL, not WARMUP. */
        det->state_before_fault = DET_STATE_NORMAL;
    } else {
        /* Already past warm-up, or in FAULT: nothing to do. */
    }
    return status;
}

/* Enter FAULT and record why. The first reason is kept, except that a real fault
 * replaces a stuck-only one, so it latches (REQ-SAF-007). The reason is read by the
 * caller with detector_take_new_fault() and written to the event log. */
static void diag_raise(detector_t *det, detector_fault_code_t code)
{
    if (det->state != DET_STATE_FAULT) {
        det->state_before_fault = det->state;
        det->state = DET_STATE_FAULT;
        det->fault_code = code;
        det->fault_new = true;
    } else if ((det->fault_code == DET_FAULT_STUCK) && (code != DET_FAULT_STUCK)) {
        det->fault_code = code;
        det->fault_new = true;
    } else {
        /* Already in FAULT for this or an earlier reason: nothing changes. */
    }
}

/* State after a stuck-only FAULT. A channel that moves may do so because gas came:
 * a filtered value already at or above a threshold goes straight to its alarm (for
 * example a channel stuck at a high level). The start of a leak usually gives a
 * filtered value still below AL1: the FAULT ends into the previous state and the
 * alarm follows with the usual 3-sample confirmation (the filter keeps its history,
 * FMEA F16). Otherwise the previous state comes back, an ALARM_2 latch included
 * (REQ-ALM-004), and a MAINTENANCE whose 10 minutes are over ends (REQ-HMI-010). */
static detector_state_t state_after_stuck(const detector_t *det, const detector_input_t *in)
{
    detector_state_t next = det->state_before_fault;

    if ((next == DET_STATE_ALARM_2) || (in->conc >= det->thresholds.al2)) {
        next = DET_STATE_ALARM_2;
    } else if (in->conc >= det->thresholds.al1) {
        next = DET_STATE_ALARM_1;
    } else if ((next == DET_STATE_MAINTENANCE) && in->maint_expired) {
        next = DET_STATE_NORMAL;
    } else {
        /* Back to the state held before the FAULT. */
    }
    return next;
}

/* In FAULT with no fault input this sample. A stuck-only FAULT ends: a channel is in
 * use again (REQ-SAF-007). Any other FAULT stays latched until a restart. */
static void step_fault(detector_t *det, const detector_input_t *in)
{
    if (det->fault_code == DET_FAULT_STUCK) {
        det->state = state_after_stuck(det, in);
        det->fault_code = DET_FAULT_NONE;
        reset_counters(det);
    }
}

gg_status_t detector_take_new_fault(detector_t *det, bool *out_new, detector_fault_code_t *out_code)
{
    gg_status_t status = GG_OK;

    if ((det == NULL) || (out_new == NULL) || (out_code == NULL)) {
        status = GG_ERR_NULL;
    } else {
        *out_new = det->fault_new;
        *out_code = det->fault_new ? det->fault_code : DET_FAULT_NONE;
        det->fault_new = false;
    }
    return status;
}

/* Dispatch to the handler of the current state. One switch, every enum value listed. */
static void step_state(detector_t *det, const detector_input_t *in)
{
    switch (det->state) {
        case DET_STATE_WARMUP:
            break;
        case DET_STATE_NORMAL:
            step_normal(det, in);
            break;
        case DET_STATE_ALARM_1:
            step_alarm_1(det);
            break;
        case DET_STATE_ALARM_2:
            step_alarm_2(det, in);
            break;
        case DET_STATE_FAULT:
            break;                /* Handled in detector_step(); listed for -Wswitch-enum. */
        case DET_STATE_MAINTENANCE:
            step_maintenance(det, in);
            break;
        default:
            /* Impossible state: fail safe. The main loop takes this fault with
             * detector_take_new_fault() and writes it to the event log. */
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
    } else if (in->system_fault != DET_FAULT_NONE) {
        diag_raise(det, in->system_fault);   /* REQ-SAF-008, REQ-SAF-009: the reason is kept. */
    } else if (in->fault_stuck) {
        diag_raise(det, DET_FAULT_STUCK);    /* REQ-SAF-003: calm air, ends by itself. */
    } else if (det->state == DET_STATE_FAULT) {
        step_fault(det, in);
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
        /* MAINTENANCE holds the last healthy state (REQ-HMI-009): it is entered from NORMAL,
         * or returned to after a stuck-only FAULT that started in it. */
        *energised = (det->state == DET_STATE_WARMUP) || (det->state == DET_STATE_NORMAL)
                     || (det->state == DET_STATE_MAINTENANCE);
    }
    return status;
}
