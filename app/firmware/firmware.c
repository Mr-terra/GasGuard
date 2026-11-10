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
 * at the rails is never used. Valid channels go through the moving average and
 * the calibration; the maximum of the valid channels is the detector input
 * (REQ-FUN-005). No valid channel is a fault. Every change of mode and every new
 * fault is written to the event log.
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
#include "hal/stack/stack.h"
#include "services/config/config.h"
#include "services/eventlog/eventlog.h"

/** @brief Calibration used until a calibration is stored: one raw LSB is one unit (0.1 %LEL). */
#define FIRMWARE_ZERO_RAW 0u
/* Temperature compensation is off: the temperature input (A3) is not read, so the
 * coefficient is zero and calib_temp_correct() is not called. */
#define FIRMWARE_TEMP_REF_RAW 0u
#define FIRMWARE_TEMP_COEF_Q8 0

/** @brief Result of one reading of the gas channels. */
typedef struct {
    gg_conc_t max_conc;  /**< Maximum concentration of the valid channels. */
    bool any_valid;      /**< True when at least one channel is valid. */
} firmware_gas_t;

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
 * @brief Read, check and convert one gas channel.
 *
 * Any failure (ADC error, out-of-range value, diagnostic fault) makes the channel
 * invalid; the caller then leaves it out of the maximum.
 *
 * @param fw     Firmware state.
 * @param ch     Channel index.
 * @param conc   Receives the concentration when the channel is valid.
 * @param valid  Receives true when the channel can be used.
 */
static void sample_channel(uint8_t ch, firmware_t *fw, uint32_t now_ms, gg_conc_t *conc, bool *valid)
{
    uint16_t raw = 0u;
    uint16_t value = 0u;
    bool faulty = true;
    bool out_of_range = false;
    gg_status_t status = hal_adc_read(adc_of_channel(ch), &raw);

    *conc = 0;
    *valid = false;
    if (status == GG_OK) {
        /* A reading at the rails is never used, even before the diagnostic latches
         * the channel fault after DIAG_RANGE_SAMPLES: a shorted input would read as
         * a high concentration and raise a latched alarm first (F02). */
        out_of_range = (raw <= DIAG_OPEN_MAX) || (raw >= DIAG_SHORT_MIN);
        status = diag_check_range(&fw->diag[ch], raw);
    }
    if (status == GG_OK) {
        /* Frozen value outside warm-up: the stuck check latches a fault after 10 minutes (REQ-SAF-003). */
        diag_stuck_sample_t stuck;

        stuck.raw = raw;
        stuck.now_ms = now_ms;
        stuck.in_warmup = (now_ms < fw->warmup_end_ms);
        status = diag_check_stuck(&fw->diag[ch], &stuck);
    }
    if (status == GG_OK) {
        status = diag_is_faulty(&fw->diag[ch], &faulty);
    }
    if ((status == GG_OK) && !faulty && !out_of_range) {
        status = filter_push(&fw->filter[ch], raw);
        if (status == GG_OK) {
            status = filter_value(&fw->filter[ch], &value);
        }
        if (status == GG_OK) {
            status = calib_to_lel(&fw->calib[ch], value, conc);
        }
        *valid = (status == GG_OK);
    }
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
 */
static gg_status_t boot_init_channels(firmware_t *fw)
{
    calib_config_t calib_cfg;
    gg_status_t status = GG_OK;
    uint8_t ch;

    for (ch = 0u; ch < FIRMWARE_CHANNELS; ch++) {
        fw->channel_faulty[ch] = false;
    }

    calib_cfg.zero_raw = FIRMWARE_ZERO_RAW;
    calib_cfg.gain_q8 = CALIB_GAIN_ONE;
    calib_cfg.temp_ref_raw = FIRMWARE_TEMP_REF_RAW;
    calib_cfg.temp_coef_q8 = FIRMWARE_TEMP_COEF_Q8;
    for (ch = 0u; (status == GG_OK) && (ch < FIRMWARE_CHANNELS); ch++) {
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
    fw->test_running = false;
    fw->test_start_ms = 0u;
    fw->pending_fault = DET_FAULT_NONE;
    fw->last_conc = 0;
    fw->bandgap_next_ms = 0u;
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
    gg_conc_t concs[FIRMWARE_CHANNELS];
    bool valid[FIRMWARE_CHANNELS];
    uint8_t ch;

    for (ch = 0u; ch < FIRMWARE_CHANNELS; ch++) {
        sample_channel(ch, fw, now_ms, &concs[ch], &valid[ch]);
    }
    return channel_select_max(concs, valid, FIRMWARE_CHANNELS, &gas->max_conc, &gas->any_valid);
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
    in->fault = !gas->any_valid;
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
 * @brief Run the detector on one sample and log a change of mode.
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

            status = eventlog_append(&event);
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
    if ((status == GG_OK) && new_fault) {
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
 * @brief Boot: log the reset cause, load the configuration, start the detector.
 * @req REQ-SAF-006 REQ-CFG-002
 */
gg_status_t firmware_boot(firmware_t *fw, hal_reset_cause_t cause, uint32_t now_ms)
{
    gg_status_t status = GG_OK;

    if (fw == NULL) {
        status = GG_ERR_NULL;
    } else {
        config_t cfg;

        status = boot_load_config(&cfg, cause, now_ms);
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
    }
    return status;
}

/**
 * @brief One sampling step.
 * @req REQ-ALM-001 REQ-SAF-007
 */
gg_status_t firmware_sample(firmware_t *fw, uint32_t now_ms)
{
    gg_status_t status = GG_OK;

    if (fw == NULL) {
        status = GG_ERR_NULL;
    } else {
        firmware_gas_t gas;

        status = sample_gas(fw, now_ms, &gas);
        if (status == GG_OK) {
            status = sample_report_channels(fw, now_ms);
        }
        if (status == GG_OK) {
            fw->last_conc = gas.max_conc;
            status = sample_detector_step(fw, now_ms, &gas);
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
 * @param button_raw  Raw button level, true when high (active low).
 * @param touch_raw   Raw touch level, true when touched.
 * @return The first error, or GG_OK.
 */
static gg_status_t inputs_update(firmware_t *fw, uint32_t now_ms, bool button_raw, bool touch_raw)
{
    bool button_pressed = false;
    bool touch_pressed = false;
    hmi_press_event_t button_event = HMI_PRESS_NONE;
    hmi_press_event_t touch_event = HMI_PRESS_NONE;
    gg_status_t status = button_step(&fw->button_db, !button_raw, now_ms, &button_pressed);

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
 * @param fw      Firmware state.
 * @param now_ms  Current time.
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
