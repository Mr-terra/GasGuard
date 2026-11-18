/**
 * @file firmware.c
 * @brief Firmware application layer: the chain from the sensors to the outputs.
 *
 * How it works
 * ------------
 * Boot logs the reset cause, loads the configuration (and logs it when the
 * defaults are used), then starts the detector with its thresholds and the
 * warm-up timer. Each sampling step reads both gas channels. A channel that
 * cannot be read, or that the diagnostic marks as faulty, is invalid. A reading
 * at the rails is never used. Every in-range reading goes through the filter
 * (median of 3, then moving average of 8), even on a channel the diagnostic
 * leaves out; only valid channels go on to the calibration and the maximum,
 * which is the detector input (REQ-FUN-005). No valid channel is a fault; when every channel is only stuck
 * (calm air) that fault ends by itself as soon as a channel moves again. Every change of mode and every new
 * fault is written to the event log. During MAINTENANCE the highest reading of
 * each channel is kept; it is logged when MAINTENANCE ends, as the record of the
 * periodic gas test.
 *
 * A start counter in EEPROM counts the starts that did not run 60 s past the warm-up.
 * From the fourth one on, each start enters FAULT at once and logs it, after its
 * reset and configuration entries (REQ-SAF-011).
 *
 * The operator inputs are debounced and classified in firmware_inputs(). The
 * requests they make (ALARM_2 reset, maintenance) reach the detector at the next
 * sample, so the detector stays the only place that changes the state. The
 * output step drives the relay from the detector, and the buzzer from the
 * state and the silence; the functional test can only release the relay, never
 * energise it, so a real alarm is never hidden by a test.
 *
 * @req REQ-SAF-006 REQ-SAF-007 REQ-SAF-008 REQ-SAF-009 REQ-ALM-001 REQ-SAF-001
 * @req REQ-HMI-004 REQ-HMI-005 REQ-HMI-006 REQ-HMI-007 REQ-HMI-008 REQ-HMI-009 REQ-HMI-010
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "app/channel_select/channel_select.h"
#include "app/firmware/firmware.h"
#include "app/hmi/hmi.h"
#include "common/gg_types/gg_types.h"
#include "drivers/buzzer/buzzer.h"
#include "drivers/button/button.h"
#include "drivers/relay/relay.h"
#include "hal/adc/adc.h"
#include "hal/eeprom/eeprom.h"
#include "hal/stack/stack.h"
#include "services/config/config.h"
#include "services/eventlog/eventlog.h"

/* Temperature compensation: the input on A3 is read and checked, but the
 * coefficient is zero (REQ-FUN-004), so the correction does not move the
 * reading. Measured on the bench over two days (2026-11-15 to 17, 16 to 19 degC):
 * the gas readings moved with the time of day, not with the temperature, so no
 * coefficient could be fitted (test report). The reference and the coefficient
 * must be set together: the reference is the raw temperature of the gas zero
 * (about 512 at 25 degC), and a coefficient with a reference of 0 would shift
 * every reading (FMEA F48). */
#define FIRMWARE_TEMP_REF_RAW 0u
#define FIRMWARE_TEMP_COEF_Q8 0

/** @brief Result of one reading of the gas channels. */
typedef struct {
    gg_conc_t conc[FIRMWARE_CHANNELS];  /**< Concentration of each channel, 0 when invalid. */
    bool valid[FIRMWARE_CHANNELS];      /**< Validity of each channel. */
    gg_conc_t max_conc;  /**< Maximum concentration of the valid channels. */
    bool any_valid;      /**< True when at least one channel is valid. */
    bool all_stuck_only; /**< Every channel has a stuck fault and no other one (REQ-SAF-003). */
} firmware_gas_t;

/** @brief Bytes of the start counter in EEPROM: the count, then its complement (REQ-SAF-011). */
#define FIRMWARE_START_COUNT_BYTES 2u
/** @brief The counter stops at this value, so a long reset loop stops writing the EEPROM. */
#define FIRMWARE_START_COUNT_MAX 255u

/**
 * @brief Count this start in EEPROM and return the number of starts in a row.
 *
 * A counter whose complement byte does not match reads as 0, so a corrupted or
 * erased EEPROM never raises the reset-loop fault on its own.
 *
 * @param out_count  Receives the count, this start included, 1 .. FIRMWARE_START_COUNT_MAX.
 * @return GG_OK, or the EEPROM error.
 * @req REQ-SAF-011
 */
static gg_status_t boot_count_start(uint8_t *out_count)
{
    uint8_t bytes[FIRMWARE_START_COUNT_BYTES] = { 0u, 0u };
    uint8_t count = 0u;
    gg_status_t status = hal_eeprom_read(FIRMWARE_ADDR_START_COUNT, bytes, FIRMWARE_START_COUNT_BYTES);

    if (status == GG_OK) {
        uint8_t check = (uint8_t)~bytes[0];

        if (bytes[1] == check) {
            count = bytes[0];
        }
        if (count < FIRMWARE_START_COUNT_MAX) {
            count++;
        }
        bytes[0] = count;
        bytes[1] = (uint8_t)~count;
        status = hal_eeprom_write(FIRMWARE_ADDR_START_COUNT, bytes, FIRMWARE_START_COUNT_BYTES);
    }
    *out_count = count;
    return status;
}

/**
 * @brief ADC input of a gas channel (REQ-FUN-006).
 * @param ch  Channel index.
 * @return The ADC channel that reads it.
 */
static uint8_t adc_of_channel(uint8_t ch)
{
    return (uint8_t)((ch == FIRMWARE_CHANNEL_MQ5) ? FIRMWARE_ADC_MQ5 : FIRMWARE_ADC_MQ2);
}

/**
 * @brief Run the range and stuck diagnostics of one channel on a reading.
 * @param diag    Diagnostic state of the channel.
 * @param sample  Raw value, time and warm-up flag of the reading.
 * @return GG_OK, or the diagnostic error.
 * @req REQ-SAF-002 REQ-SAF-003
 */
static gg_status_t sample_diagnose(diag_t *diag, const diag_stuck_sample_t *sample)
{
    gg_status_t status = diag_check_range(diag, sample->raw);

    if (status == GG_OK) {
        /* Frozen value outside warm-up: the stuck check sets a channel fault after 10 minutes (REQ-SAF-003). */
        status = diag_check_stuck(diag, sample);
    }
    return status;
}

/**
 * @brief Filter and convert one usable reading.
 * @param fw     Firmware state.
 * @param ch     Channel index.
 * @param raw    Raw ADC value.
 * @param conc   Receives the concentration.
 * @return GG_OK, or the filter or calibration error.
 * @req REQ-FUN-002 REQ-FUN-003 REQ-FUN-004
 */
static gg_status_t sample_convert(firmware_t *fw, uint8_t ch, uint16_t raw, gg_conc_t *conc)
{
    calib_reading_t reading;
    uint16_t corrected = 0u;
    gg_status_t status = filter_push(&fw->filter[ch], raw);

    reading.raw = 0u;
    reading.temp_raw = fw->temp_raw;
    reading.temp_valid = fw->temp_valid;
    if (status == GG_OK) {
        status = filter_value(&fw->filter[ch], &reading.raw);
    }
    if (status == GG_OK) {
        status = calib_temp_correct(&fw->calib[ch], &reading, &corrected);
    }
    if (status == GG_OK) {
        status = calib_to_lel(&fw->calib[ch], corrected, conc);
    }
    return status;
}

/**
 * @brief Read the temperature input and check it is in its valid range (REQ-FUN-004).
 *
 * A failed read or a value out of range makes it invalid, and an invalid
 * temperature leaves the gas reading uncorrected. It is never a gas fault.
 *
 * @param fw  Firmware state.
 * @req REQ-FUN-004
 */
static void sample_temperature(firmware_t *fw)
{
    uint16_t raw = 0u;
    gg_status_t status = hal_adc_read(FIRMWARE_ADC_TEMP, &raw);

    fw->temp_raw = raw;
    fw->temp_valid = (status == GG_OK) && (raw >= FIRMWARE_TEMP_RAW_MIN) && (raw <= FIRMWARE_TEMP_RAW_MAX);
}

/**
 * @brief Read, check and convert one gas channel.
 *
 * Any failure (ADC error, out-of-range value, diagnostic fault) makes the channel
 * invalid; the caller then leaves it out of the maximum.
 *
 * Only an in-range reading is pushed into the filter, and the filter is never
 * restarted here: a read error or a reading at the rails is simply left out.
 * A channel left out by the diagnostic (stuck, or range recovery) keeps
 * feeding its filter, so it always comes back with a full median history.
 *
 * @param ch      Channel index.
 * @param fw      Firmware state.
 * @param now_ms  Current time.
 * @param conc    Receives the concentration when the channel is valid.
 * @param valid   Receives true when the channel can be used.
 * @return True when the read worked and is inside the rails (a clean read).
 * @req REQ-SAF-002 REQ-SAF-003 REQ-FUN-002
 */
static bool sample_channel(uint8_t ch, firmware_t *fw, uint32_t now_ms, gg_conc_t *conc, bool *valid)
{
    uint16_t raw = 0u;
    bool faulty = true;
    bool out_of_range = true;
    gg_status_t status = hal_adc_read(adc_of_channel(ch), &raw);

    fw->last_raw[ch] = raw;
    *conc = 0;
    *valid = false;
    if (status == GG_OK) {
        /* A reading at the rails is never used, even before the diagnostic sets
         * the channel fault after DIAG_RANGE_SAMPLES: a shorted input would read as
         * a high concentration and raise a latched alarm first (F02). */
        diag_stuck_sample_t sample;

        out_of_range = (raw <= DIAG_OPEN_MAX) || (raw >= DIAG_SHORT_MIN);
        sample.raw = raw;
        sample.now_ms = now_ms;
        sample.in_warmup = (now_ms < fw->warmup_end_ms);
        status = sample_diagnose(&fw->diag[ch], &sample);
    } else {
        /* A failed read counts towards a channel fault like a rail reading, so a
         * channel that never reads is set faulty and logged (REQ-SAF-002). The
         * read stays a failure: the channel is out and it is never calm air. */
        (void)diag_read_failed(&fw->diag[ch]);   /* Fails only on a NULL pointer. */
    }
    if (status == GG_OK) {
        status = diag_is_faulty(&fw->diag[ch], &faulty);
    }
    if ((status == GG_OK) && !out_of_range) {
        /* An in-range reading always feeds the filter, even while the diagnostic
         * keeps the channel out (stuck, or range recovery): it is a real reading.
         * So when the channel comes back, the median of 3 already has a history
         * and a single wild sample at that moment is still removed (FMEA F16). */
        bool converted = (sample_convert(fw, ch, raw, conc) == GG_OK);

        *valid = converted && !faulty;
        if (!*valid) {
            *conc = 0;
        }
    }
    /* A read error or a reading at the rails is not pushed and does not restart
     * the filter: after a restart the next reading would pass on alone, and a
     * bouncing contact (rail, then a wild value) could latch ALARM_2 (FMEA F16).
     * A real range fault keeps the channel out for DIAG_RECOVER_SAMPLES in-range
     * readings, which refill the filter before it is used again. */
    return (status == GG_OK) && !out_of_range;
}

/**
 * @brief Log the reset cause, then load the configuration and log it unless copy A was used.
 * @param cfg    Receives the configuration.
 * @param cause  Reset cause.
 * @param now_ms Current time.
 * @return The first error, or GG_OK.
 */
static gg_status_t boot_load_config(config_t *cfg, hal_reset_cause_t cause, uint32_t now_ms)
{
    gg_status_t status = GG_OK;
    config_source_t source = CONFIG_SRC_DEFAULTS;
    eventlog_entry_t reset_event = { now_ms / FIRMWARE_MS_PER_S, EVENTLOG_RESET, (uint16_t)cause };

    status = eventlog_append(&reset_event);
    if (status == GG_OK) {
        status = config_load(cfg, &source);
    }
    /* Logged whenever the primary copy was not the one used: copy B or the defaults (SC-11, SC-12). */
    if ((status == GG_OK) && (source != CONFIG_SRC_COPY_A)) {
        eventlog_entry_t config_event = { now_ms / FIRMWARE_MS_PER_S, EVENTLOG_CONFIG, (uint16_t)source };

        status = eventlog_append(&config_event);
    }
    return status;
}

/**
 * @brief Start the detector with the stored thresholds and set the end of the warm-up.
 * @param fw      Firmware state.
 * @param cfg     Configuration in use.
 * @param now_ms  Start of the warm-up.
 * @return GG_OK or the detector error.
 */
static gg_status_t boot_start_detector(firmware_t *fw, const config_t *cfg, uint32_t now_ms)
{
    detector_thresholds_t thresholds;
    gg_status_t status;

    thresholds.al1 = cfg->al1;
    thresholds.al2 = cfg->al2;
    thresholds.hysteresis = cfg->hysteresis;
    status = detector_init(&fw->det, &thresholds);
    if (status == GG_OK) {
        fw->warmup_end_ms = now_ms + ((uint32_t)cfg->warmup_s * FIRMWARE_MS_PER_S);
    }
    return status;
}

/**
 * @brief Initialise the filter, the diagnostic and the calibration of each channel.
 * @param fw  Firmware state.
 * @return The first error, or GG_OK.
 * @req REQ-FUN-003
 */
static gg_status_t boot_init_channels(firmware_t *fw)
{
    /* Zero offset of each channel. Above it, one raw LSB is 0.1 %LEL. */
    static const uint16_t k_zero_raw[FIRMWARE_CHANNELS] = {
        [FIRMWARE_CHANNEL_MQ5] = FIRMWARE_ZERO_RAW_MQ5,
        [FIRMWARE_CHANNEL_MQ2] = FIRMWARE_ZERO_RAW_MQ2
    };
    calib_config_t calib_cfg;
    gg_status_t status = GG_OK;
    uint8_t ch;

    for (ch = 0u; ch < FIRMWARE_CHANNELS; ch++) {
        fw->channel_faulty[ch] = false;
        fw->last_raw[ch] = 0u;
    }
    fw->temp_raw = 0u;
    fw->temp_valid = false;

    calib_cfg.gain_q8 = CALIB_GAIN_ONE;
    calib_cfg.temp_ref_raw = FIRMWARE_TEMP_REF_RAW;
    calib_cfg.temp_coef_q8 = FIRMWARE_TEMP_COEF_Q8;
    for (ch = 0u; (status == GG_OK) && (ch < FIRMWARE_CHANNELS); ch++) {
        calib_cfg.zero_raw = k_zero_raw[ch];
        status = filter_init(&fw->filter[ch]);
        if (status == GG_OK) {
            status = diag_init(&fw->diag[ch]);
        }
        if (status == GG_OK) {
            status = calib_init(&fw->calib[ch], &calib_cfg);
        }
    }
    return status;
}

/**
 * @brief Start a new peak record when MAINTENANCE starts (REQ-LOG-001).
 * @param fw  Firmware state.
 */
static void maint_peak_clear(firmware_t *fw)
{
    uint8_t ch;

    for (ch = 0u; ch < FIRMWARE_CHANNELS; ch++) {
        fw->maint_peak[ch] = 0;
    }
}

/**
 * @brief Fix the peaks of the MAINTENANCE that just ended and queue them for the log (REQ-LOG-001).
 *
 * The log reads this copy, so a new MAINTENANCE that clears the running peaks
 * before the queue is done cannot change what is logged.
 *
 * @param fw  Firmware state.
 */
static void maint_peak_queue(firmware_t *fw)
{
    uint8_t ch;

    for (ch = 0u; ch < FIRMWARE_CHANNELS; ch++) {
        fw->maint_peak_logged[ch] = fw->maint_peak[ch];
    }
    fw->maint_peak_next = 0u;
}

/**
 * @brief Initialise the operator inputs: debounce, press tracking, silence, pending requests.
 * @param fw  Firmware state.
 * @return The first error, or GG_OK.
 */
static gg_status_t boot_init_inputs(firmware_t *fw)
{
    gg_status_t status = button_init(&fw->button_db);

    if (status == GG_OK) {
        status = button_init(&fw->touch_db);
    }
    if (status == GG_OK) {
        status = hmi_press_init(&fw->button_press);
    }
    if (status == GG_OK) {
        status = hmi_press_init(&fw->touch_press);
    }
    if (status == GG_OK) {
        status = hmi_silence_init(&fw->silence);
    }
    fw->reset_pending = false;
    fw->maint_pending = false;
    fw->maint_start_ms = 0u;
    maint_peak_clear(fw);
    maint_peak_queue(fw);                     /* The logged copy starts at 0 too, */
    fw->maint_peak_next = FIRMWARE_CHANNELS;  /* with nothing to log. */
    fw->test_running = false;
    fw->test_start_ms = 0u;
    fw->pending_fault = DET_FAULT_NONE;
    fw->last_conc = 0;
    fw->bandgap_next_ms = 0u;
    fw->bandgap_logged = false;
    return status;
}

/**
 * @brief Read the gas channels and select the maximum of the valid ones.
 * @param fw   Firmware state.
 * @param gas  Receives the selection.
 * @return The status of the selection.
 */
static gg_status_t sample_gas(firmware_t *fw, uint32_t now_ms, firmware_gas_t *gas)
{
    uint8_t ch;

    gas->max_conc = 0;
    gas->any_valid = false;
    gas->all_stuck_only = true;
    for (ch = 0u; ch < FIRMWARE_CHANNELS; ch++) {
        bool stuck_only = false;
        bool clean_read = sample_channel(ch, fw, now_ms, &gas->conc[ch], &gas->valid[ch]);
        /* Fails only on a NULL pointer; the default then counts as a real fault. */
        (void)diag_stuck_only(&fw->diag[ch], &stuck_only);
        /* A failed read or a reading at the rails is a real fault, never calm air (REQ-SAF-007). */
        gas->all_stuck_only = gas->all_stuck_only && stuck_only && clean_read;
    }
    return channel_select_max(gas->conc, gas->valid, FIRMWARE_CHANNELS, &gas->max_conc, &gas->any_valid);
}

/**
 * @brief Fill the detector input of one sample, and clear the requests it carries.
 *
 * A maintenance request and a system fault are given once; the next sample starts
 * without them. A reset request is given once too.
 *
 * @param fw      Firmware state.
 * @param now_ms  Current time.
 * @param gas     Selection of this sample.
 * @param in      Receives the input.
 */
static void sample_fill_input(firmware_t *fw, uint32_t now_ms, const firmware_gas_t *gas, detector_input_t *in)
{
    bool maint_time_over = (now_ms - fw->maint_start_ms) >= FIRMWARE_MAINT_MAX_MS;

    in->conc = gas->max_conc;
    /* No valid channel: a FAULT that ends by itself when all are only stuck (calm air),
     * a latched one otherwise (REQ-SAF-007). */
    in->fault = !gas->any_valid && !gas->all_stuck_only;
    in->fault_stuck = !gas->any_valid && gas->all_stuck_only;
    in->reset_requested = fw->reset_pending;
    in->maint_requested = fw->maint_pending;
    in->maint_expired = (firmware_state(fw) == DET_STATE_MAINTENANCE) && maint_time_over;
    in->system_fault = fw->pending_fault;
    fw->reset_pending = false;
    fw->maint_pending = false;
    fw->pending_fault = DET_FAULT_NONE;
}

/**
 * @brief Log each change of the fault state of a channel since the last sample (REQ-SAF-002, REQ-SAF-003).
 *
 * A channel that becomes faulty is logged (CHANNEL), and one that comes back
 * into service is logged (RECOVERED). Each change is logged once.
 *
 * @param fw      Firmware state.
 * @param now_ms  Current time.
 * @return The first error, or GG_OK.
 */
static gg_status_t sample_report_channels(firmware_t *fw, uint32_t now_ms)
{
    gg_status_t status = GG_OK;
    uint8_t ch;

    for (ch = 0u; (status == GG_OK) && (ch < FIRMWARE_CHANNELS); ch++) {
        bool faulty = false;

        status = diag_is_faulty(&fw->diag[ch], &faulty);
        if ((status == GG_OK) && (faulty != fw->channel_faulty[ch])) {
            eventlog_code_t code = EVENTLOG_CHANNEL_RECOVERED;
            eventlog_entry_t event;

            if (faulty) {
                code = EVENTLOG_CHANNEL_FAULT;
            }
            event.uptime_s = now_ms / FIRMWARE_MS_PER_S;
            event.code = code;
            event.arg = (uint16_t)ch;
            status = eventlog_append(&event);
        }
        if (status == GG_OK) {
            fw->channel_faulty[ch] = faulty;
        }
    }
    return status;
}

/**
 * @brief Keep the highest reading of each channel during MAINTENANCE (REQ-LOG-001).
 *
 * The peaks restart from 0 when MAINTENANCE starts from NORMAL, not when the
 * touch asks for it: a touch in the samples right after an end would wipe the
 * peaks still waiting to be logged. The sample that ends MAINTENANCE still
 * counts. When it ends, the peaks are fixed and queued for the log. A stuck-only
 * FAULT inside MAINTENANCE queues the peaks so far, and the final exit queues
 * them again with the later readings.
 *
 * @param fw      Firmware state.
 * @param gas     Selection of this sample.
 * @param before  State before this sample: MAINTENANCE tracks and queues, NORMAL into
 *                MAINTENANCE clears, any other state does nothing.
 */
static void maint_peak_track(firmware_t *fw, const firmware_gas_t *gas, detector_state_t before)
{
    bool in_maintenance = (firmware_state(fw) == DET_STATE_MAINTENANCE);

    if (before == DET_STATE_MAINTENANCE) {
        uint8_t ch;

        for (ch = 0u; ch < FIRMWARE_CHANNELS; ch++) {
            if (gas->valid[ch] && (gas->conc[ch] > fw->maint_peak[ch])) {
                fw->maint_peak[ch] = gas->conc[ch];
            }
        }
        if (!in_maintenance) {
            maint_peak_queue(fw);
        }
    } else if ((before == DET_STATE_NORMAL) && in_maintenance) {
        maint_peak_clear(fw);
    } else {
        /* Outside MAINTENANCE, or back to it after a stuck-only FAULT: the peaks go on. */
    }
}

/**
 * @brief Log the next queued peak: one entry per sample, at most (REQ-LOG-001).
 *
 * One entry per sample keeps the EEPROM time of a sample to one write besides the
 * MODE, FAULT and CHANNEL entries, within the 100 ms cycle in normal use
 * (REQ-SAF-008, FMEA F47). The
 * write error is ignored on purpose: a PEAK entry is a record, and its failure
 * must never stop the sample that drives the relay (FMEA F47, as F44).
 *
 * @param fw      Firmware state.
 * @param now_ms  Current time.
 */
static void maint_peak_log_next(firmware_t *fw, uint32_t now_ms)
{
    if (fw->maint_peak_next < FIRMWARE_CHANNELS) {
        uint8_t ch = fw->maint_peak_next;
        /* The peak starts at 0 and only grows, so it is never negative. */
        uint16_t peak = (uint16_t)fw->maint_peak_logged[ch];
        eventlog_entry_t event;

        if (peak > EVENTLOG_PEAK_VALUE_MASK) {
            peak = EVENTLOG_PEAK_VALUE_MASK;
        }
        event.uptime_s = now_ms / FIRMWARE_MS_PER_S;
        event.code = EVENTLOG_MAINT_PEAK;
        event.arg = (uint16_t)(((uint16_t)ch << EVENTLOG_PEAK_CHANNEL_SHIFT) | peak);
        (void)eventlog_append(&event);
        fw->maint_peak_next++;
    }
}

/**
 * @brief Run the detector on one sample, log a change of mode and end the silence it may carry.
 * @param fw      Firmware state.
 * @param now_ms  Current time.
 * @param gas     Selection of this sample; no valid channel is a fault.
 * @return The first error, or GG_OK.
 */
static gg_status_t sample_detector_step(firmware_t *fw, uint32_t now_ms, const firmware_gas_t *gas)
{
    gg_status_t status = GG_OK;
    detector_state_t before = firmware_state(fw);
    detector_state_t after;
    detector_input_t input;

    if ((before == DET_STATE_WARMUP) && (now_ms >= fw->warmup_end_ms)) {
        status = detector_end_warmup(&fw->det);
    }
    if (status == GG_OK) {
        sample_fill_input(fw, now_ms, gas, &input);
        status = detector_step(&fw->det, &input);
    }
    if (status == GG_OK) {
        after = firmware_state(fw);
        if (after != before) {
            eventlog_entry_t event = { now_ms / FIRMWARE_MS_PER_S, EVENTLOG_MODE_CHANGE, (uint16_t)after };

            /* A silence belongs to the alarm it was given for: any change of state ends it,
             * so a later alarm sounds again (REQ-HMI-004). */
            status = hmi_silence_init(&fw->silence);
            if (status == GG_OK) {
                status = eventlog_append(&event);
            }
        }
    }
    return status;
}

/**
 * @brief Log a fault that started since the last sample.
 * @param fw      Firmware state.
 * @param now_ms  Current time.
 * @return The first error, or GG_OK.
 */
static gg_status_t sample_report_fault(firmware_t *fw, uint32_t now_ms)
{
    gg_status_t status;
    bool new_fault = false;
    detector_fault_code_t fault_code = DET_FAULT_NONE;

    status = detector_take_new_fault(&fw->det, &new_fault, &fault_code);
    /* The reset-loop fault was logged at boot, before anything else could fail (REQ-SAF-011). */
    if ((status == GG_OK) && new_fault && (fault_code != DET_FAULT_RESET_LOOP)) {
        eventlog_entry_t event = { now_ms / FIRMWARE_MS_PER_S, EVENTLOG_FAULT, (uint16_t)fault_code };

        status = eventlog_append(&event);
    }
    return status;
}

/**
 * @brief Act on a push-button event: acknowledge, reset request or functional test.
 *
 * A short press silences an alarm (REQ-HMI-004). A long press in ALARM_2 asks for
 * the latch reset (REQ-HMI-005); in NORMAL it starts the functional test (REQ-HMI-006).
 * Other states ignore a long press.
 *
 * @param fw      Firmware state.
 * @param event   Event of the push button.
 * @param now_ms  Current time.
 * @return GG_OK or the error of the silence.
 */
static gg_status_t act_button(hmi_press_event_t event, firmware_t *fw, uint32_t now_ms)
{
    detector_state_t state = firmware_state(fw);
    gg_status_t status = GG_OK;

    if (event == HMI_PRESS_SHORT) {
        status = hmi_acknowledge(state, &fw->silence, now_ms);
    } else if (event == HMI_PRESS_LONG) {
        if (state == DET_STATE_ALARM_2) {
            fw->reset_pending = true;
        } else if (state == DET_STATE_NORMAL) {
            fw->test_running = true;
            fw->test_start_ms = now_ms;
        } else {
            /* No long-press action in the other states. */
        }
    } else {
        /* No event: nothing to do. */
    }
    return status;
}

/**
 * @brief A touch of 3 s in NORMAL asks for MAINTENANCE (REQ-HMI-008).
 *
 * The detector still refuses it when the gas is at or above AL1 - hysteresis; the request
 * is then dropped at the next sample.
 * @param fw      Firmware state.
 * @param event   Event of the touch sensor.
 * @param now_ms  Current time.
 */
static void act_touch(hmi_press_event_t event, firmware_t *fw, uint32_t now_ms)
{
    if ((event == HMI_PRESS_LONG) && (firmware_state(fw) == DET_STATE_NORMAL)) {
        fw->maint_pending = true;
        fw->maint_start_ms = now_ms;
    }
}

/**
 * @brief End the functional test when its time is over (REQ-HMI-006).
 * @param fw      Firmware state.
 * @param now_ms  Current time.
 */
static void expire_test(firmware_t *fw, uint32_t now_ms)
{
    if (fw->test_running && ((now_ms - fw->test_start_ms) >= HMI_TEST_TOTAL_MS)) {
        fw->test_running = false;
    }
}

/** @brief Output levels for one instant. */
typedef struct {
    bool relay;    /**< Relay coil level: true = energised. */
    bool buzzer;   /**< Buzzer level: true = sounding. */
} firmware_levels_t;

/**
 * @brief Relay and buzzer levels for this instant (REQ-SAF-001, REQ-HMI-003, REQ-HMI-006).
 *
 * The detector decides first. The buzzer adds the test beep while the test runs.
 * The test can only release the relay, never energise it.
 *
 * @param fw      Firmware state.
 * @param now_ms  Current time.
 * @param levels  Receives the levels.
 * @return GG_OK, or the detector error.
 */
static gg_status_t output_levels(const firmware_t *fw, uint32_t now_ms, firmware_levels_t *levels)
{
    gg_status_t status = detector_relay_energised(&fw->det, &levels->relay);
    hmi_test_out_t test;
    bool running = false;

    if (status == GG_OK) {
        status = hmi_buzzer_on(firmware_state(fw), &fw->silence, now_ms, &levels->buzzer);
    }
    if ((status == GG_OK) && fw->test_running) {
        status = hmi_test_outputs(now_ms - fw->test_start_ms, &test, &running);
        levels->relay = levels->relay && test.relay_energised;
        levels->buzzer = levels->buzzer || test.buzzer;
    }
    return status;
}

/**
 * @brief Boot: count the start, log the reset cause, load the configuration, start the detector.
 * @req REQ-SAF-006 REQ-CFG-002 REQ-SAF-011
 */
gg_status_t firmware_boot(firmware_t *fw, hal_reset_cause_t cause, uint32_t now_ms)
{
    gg_status_t status = GG_OK;

    if (fw == NULL) {
        status = GG_ERR_NULL;
    } else {
        config_t cfg;
        uint8_t starts = 0u;

        status = boot_count_start(&starts);
        fw->start_ms = now_ms;
        fw->start_count_cleared = false;
        if (status == GG_OK) {
            status = boot_load_config(&cfg, cause, now_ms);
        }
        if (status == GG_OK) {
            status = boot_start_detector(fw, &cfg, now_ms);
        }
        if (status == GG_OK) {
            status = boot_init_channels(fw);
        }
        if (status == GG_OK) {
            status = boot_init_inputs(fw);
        }
        if (status == GG_OK) {
            status = relay_init();
        }
        if (status == GG_OK) {
            status = buzzer_init();
        }
        if ((status == GG_OK) && (starts > FIRMWARE_SHORT_STARTS_MAX)) {
            /* Reset loop (REQ-SAF-011): FAULT at the first sample, the relay stays off.
             * The fault is logged here at each such start, so the log shows it even if
             * the start fails later. */
            eventlog_entry_t loop_event = { now_ms / FIRMWARE_MS_PER_S, EVENTLOG_FAULT,
                                            (uint16_t)DET_FAULT_RESET_LOOP };

            fw->pending_fault = DET_FAULT_RESET_LOOP;
            status = eventlog_append(&loop_event);
        }
    }
    return status;
}

/**
 * @brief One sampling step.
 * @req REQ-ALM-001 REQ-SAF-007 REQ-LOG-001 REQ-FUN-004
 */
gg_status_t firmware_sample(firmware_t *fw, uint32_t now_ms)
{
    gg_status_t status = GG_OK;

    if (fw == NULL) {
        status = GG_ERR_NULL;
    } else {
        firmware_gas_t gas;

        /* The temperature is read first, so this sample's gas reading uses it. */
        sample_temperature(fw);
        status = sample_gas(fw, now_ms, &gas);
        if (status == GG_OK) {
            status = sample_report_channels(fw, now_ms);
        }
        if (status == GG_OK) {
            detector_state_t before = firmware_state(fw);

            /* A peak queued by an earlier sample: written before this one can queue more. */
            maint_peak_log_next(fw, now_ms);
            fw->last_conc = gas.max_conc;
            status = sample_detector_step(fw, now_ms, &gas);
            maint_peak_track(fw, &gas, before);
        }
        if (status == GG_OK) {
            status = sample_report_fault(fw, now_ms);
        }
    }
    return status;
}

/**
 * @brief Debounce both inputs and run their press tracking.
 * @param fw          Firmware state.
 * @param now_ms      Current time.
 * @param button_raw  Raw button level, true when high: the button module drives high when pressed.
 * @param touch_raw   Raw touch level, true when touched.
 * @return The first error, or GG_OK.
 */
static gg_status_t inputs_update(firmware_t *fw, uint32_t now_ms, bool button_raw, bool touch_raw)
{
    bool button_pressed = false;
    bool touch_pressed = false;
    hmi_press_event_t button_event = HMI_PRESS_NONE;
    hmi_press_event_t touch_event = HMI_PRESS_NONE;
    gg_status_t status = button_step(&fw->button_db, button_raw, now_ms, &button_pressed);

    if (status == GG_OK) {
        status = button_step(&fw->touch_db, touch_raw, now_ms, &touch_pressed);
    }
    if (status == GG_OK) {
        status = hmi_press_step(&fw->button_press, button_pressed, now_ms, &button_event);
    }
    if (status == GG_OK) {
        status = hmi_press_step(&fw->touch_press, touch_pressed, now_ms, &touch_event);
    }
    if (status == GG_OK) {
        status = act_button(button_event, fw, now_ms);
    }
    if (status == GG_OK) {
        act_touch(touch_event, fw, now_ms);
        expire_test(fw, now_ms);
    }
    return status;
}

/**
 * @brief Operator inputs.
 * @req REQ-HMI-004 REQ-HMI-005 REQ-HMI-006 REQ-HMI-007 REQ-HMI-008
 */
gg_status_t firmware_inputs(firmware_t *fw, uint32_t now_ms, bool button_raw, bool touch_raw)
{
    gg_status_t status = GG_OK;

    if (fw == NULL) {
        status = GG_ERR_NULL;
    } else {
        status = inputs_update(fw, now_ms, button_raw, touch_raw);
    }
    return status;
}

/**
 * @brief Drive the outputs from the detector state.
 * @req REQ-SAF-001 REQ-HMI-003 REQ-HMI-006
 */
gg_status_t firmware_output(const firmware_t *fw, uint32_t now_ms)
{
    gg_status_t status = GG_OK;
    firmware_levels_t levels;

    if (fw == NULL) {
        status = GG_ERR_NULL;
    } else {
        status = output_levels(fw, now_ms, &levels);
        if (status == GG_OK) {
            status = relay_set(levels.relay);
        }
        if (status == GG_OK) {
            status = buzzer_set(levels.buzzer);
        }
    }
    return status;
}

/**
 * @brief Stack check: a fault when the free stack is low, or when it cannot be measured.
 * @param fw  Firmware state.
 * @req REQ-SAF-009
 */
static void health_stack(firmware_t *fw)
{
    uint16_t free_bytes = 0u;

    /* A stack that cannot be measured is treated as a low stack (fail-safe). */
    if ((hal_stack_free_bytes(&free_bytes) != GG_OK) || (free_bytes < FIRMWARE_STACK_MIN_FREE)) {
        fw->pending_fault = DET_FAULT_STACK;
    }
}

/**
 * @brief Measure the bandgap when it is due; a fault when it is out of tolerance (REQ-SAF-004).
 *
 * The first bad value since boot is logged with its raw reading (0 if the read
 * failed), so a fault seen on the board can be diagnosed. Later ones are not: the
 * log is written on events, not once a minute (REQ-LOG-003).
 *
 * @param fw      Firmware state.
 * @param now_ms  Current time.
 * @req REQ-SAF-004 REQ-LOG-003
 */
static void health_bandgap(firmware_t *fw, uint32_t now_ms)
{
    uint16_t raw = 0u;
    bool ok = false;

    if (now_ms >= fw->bandgap_next_ms) {
        fw->bandgap_next_ms = now_ms + FIRMWARE_BANDGAP_PERIOD_MS;
        /* An ADC error, or a reference out of tolerance, means the readings cannot be trusted. */
        if ((hal_adc_read_bandgap(&raw) != GG_OK) || (diag_bandgap_ok(raw, &ok) != GG_OK) || !ok) {
            fw->pending_fault = DET_FAULT_INPUT;
            if (!fw->bandgap_logged) {
                eventlog_entry_t event = { now_ms / FIRMWARE_MS_PER_S, EVENTLOG_BANDGAP, raw };

                fw->bandgap_logged = true;
                /* Diagnostic entry only: a failed write must not stop the cycle, or the
                 * watchdog reset would clear the pending fault before it latches (F44). */
                (void)eventlog_append(&event);
            }
        }
    }
}

/**
 * @brief 60 s after the end of the warm-up, clear the start counter: the next start is a normal one (REQ-SAF-011).
 *
 * The warm-up does not count, so a start that hangs during or right after it is
 * still counted as short. A failed write is tried again at the next health check;
 * it never stops the cycle.
 *
 * @param fw      Firmware state.
 * @param now_ms  Current time.
 * @req REQ-SAF-011
 */
static void health_start_count(firmware_t *fw, uint32_t now_ms)
{
    /* Differences from the start, so the comparison stays right when the timer wraps. */
    uint32_t warmup_ms = fw->warmup_end_ms - fw->start_ms;

    if (!fw->start_count_cleared && ((now_ms - fw->start_ms) >= (warmup_ms + FIRMWARE_STABLE_RUN_MS))) {
        uint8_t bytes[FIRMWARE_START_COUNT_BYTES];

        bytes[0] = 0u;
        bytes[1] = (uint8_t)~bytes[0];
        if (hal_eeprom_write(FIRMWARE_ADDR_START_COUNT, bytes, FIRMWARE_START_COUNT_BYTES) == GG_OK) {
            fw->start_count_cleared = true;
        }
    }
}

gg_status_t firmware_health(firmware_t *fw, uint32_t now_ms)
{
    gg_status_t status = GG_OK;

    if (fw == NULL) {
        status = GG_ERR_NULL;
    } else {
        health_stack(fw);
        health_bandgap(fw, now_ms);
        health_start_count(fw, now_ms);
    }
    return status;
}

/**
 * @brief Scheduler cycle report.
 * @req REQ-SAF-008
 */
gg_status_t firmware_report_timing(firmware_t *fw, bool overrun, bool fault, uint32_t now_ms)
{
    gg_status_t status = GG_OK;

    if (fw == NULL) {
        status = GG_ERR_NULL;
    } else {
        if (overrun) {
            eventlog_entry_t event = { now_ms / FIRMWARE_MS_PER_S, EVENTLOG_OVERRUN, 0u };

            status = eventlog_append(&event);
        }
        if ((status == GG_OK) && fault) {
            fw->pending_fault = DET_FAULT_TIMING;
        }
    }
    return status;
}

/**
 * @brief Snapshot of the `status` command.
 * @req REQ-LOG-005
 */
gg_status_t firmware_status(const firmware_t *fw, cli_status_t *out)
{
    gg_status_t status = GG_OK;

    if ((fw == NULL) || (out == NULL)) {
        status = GG_ERR_NULL;
    } else {
        out->state = (uint8_t)firmware_state(fw);
        out->conc = fw->last_conc;
        out->raw_mq5 = fw->last_raw[FIRMWARE_CHANNEL_MQ5];
        out->raw_mq2 = fw->last_raw[FIRMWARE_CHANNEL_MQ2];
        out->raw_temp = fw->temp_raw;
        out->temp_valid = fw->temp_valid;
    }
    return status;
}

/**
 * @brief Current detector state.
 * @req REQ-ALM-001
 */
detector_state_t firmware_state(const firmware_t *fw)
{
    detector_state_t state = DET_STATE_FAULT;

    if (fw != NULL) {
        if (detector_state(&fw->det, &state) != GG_OK) {
            state = DET_STATE_FAULT;
        }
    }
    return state;
}

/**
 * @brief Log a display that did not answer at boot.
 * @req REQ-HMI-001
 */
gg_status_t firmware_report_display(const firmware_t *fw, bool present, uint32_t now_ms)
{
    gg_status_t status = GG_OK;

    if (fw == NULL) {
        status = GG_ERR_NULL;
    } else if (!present) {
        eventlog_entry_t event = { now_ms / FIRMWARE_MS_PER_S, EVENTLOG_DISPLAY_FAULT, 0u };

        status = eventlog_append(&event);
    } else {
        /* Present: nothing to log. */
    }
    return status;
}

/**
 * @brief Concentration of the last sample.
 * @req REQ-HMI-001
 */
gg_conc_t firmware_concentration(const firmware_t *fw)
{
    gg_conc_t conc = 0;

    if (fw != NULL) {
        conc = fw->last_conc;
    }
    return conc;
}

/**
 * @brief Whether the functional test is running.
 * @req REQ-HMI-006
 */
bool firmware_test_running(const firmware_t *fw)
{
    bool running = false;

    if (fw != NULL) {
        running = fw->test_running;
    }
    return running;
}

/**
 * @brief Whether the relay coil should be energised now.
 * @req REQ-SAF-001
 */
gg_status_t firmware_relay_energised(const firmware_t *fw, bool *energised)
{
    gg_status_t status = GG_OK;

    if ((fw == NULL) || (energised == NULL)) {
        status = GG_ERR_NULL;
    } else {
        status = detector_relay_energised(&fw->det, energised);
    }
    return status;
}
