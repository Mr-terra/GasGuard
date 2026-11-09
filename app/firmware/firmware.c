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
 * fault is written to the event log. The output step drives the relay from the
 * detector and the buzzer from the state.
 *
 * @req REQ-SAF-006 REQ-SAF-007 REQ-ALM-001 REQ-SAF-001
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "app/channel_select/channel_select.h"
#include "app/firmware/firmware.h"
#include "common/gg_types/gg_types.h"
#include "drivers/buzzer/buzzer.h"
#include "drivers/relay/relay.h"
#include "hal/adc/adc.h"
#include "services/config/config.h"
#include "services/eventlog/eventlog.h"

/** @brief Calibration used until a calibration is stored: one raw LSB is one unit (0.1 %LEL). */
#define FIRMWARE_ZERO_RAW 0u
#define FIRMWARE_TEMP_REF_RAW 0u
#define FIRMWARE_TEMP_COEF_Q8 0

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
static void sample_channel(firmware_t *fw, uint8_t ch, gg_conc_t *conc, bool *valid)
{
    uint16_t raw = 0u;
    uint16_t value = 0u;
    bool faulty = true;
    bool out_of_range = false;
    gg_status_t status = hal_adc_read(ch, &raw);

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
 * @brief Log the reset cause, then load the configuration and log it if the defaults are used.
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
    if ((status == GG_OK) && (source == CONFIG_SRC_DEFAULTS)) {
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

/** @brief Result of one reading of the gas channels. */
typedef struct {
    gg_conc_t max_conc;  /**< Maximum concentration of the valid channels. */
    bool any_valid;      /**< True when at least one channel is valid. */
} firmware_gas_t;

/**
 * @brief Read the gas channels and select the maximum of the valid ones.
 * @param fw   Firmware state.
 * @param gas  Receives the selection.
 * @return The status of the selection.
 */
static gg_status_t sample_gas(firmware_t *fw, firmware_gas_t *gas)
{
    gg_conc_t concs[FIRMWARE_CHANNELS];
    bool valid[FIRMWARE_CHANNELS];
    uint8_t ch;

    for (ch = 0u; ch < FIRMWARE_CHANNELS; ch++) {
        sample_channel(fw, ch, &concs[ch], &valid[ch]);
    }
    return channel_select_max(concs, valid, FIRMWARE_CHANNELS, &gas->max_conc, &gas->any_valid);
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
        input.conc = gas->max_conc;
        input.fault = !gas->any_valid;
        input.reset_requested = false;
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
 * @brief Boot: log the reset cause, load the configuration, start the detector.
 *
 * The full contract is in the header.
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
 *
 * The full contract is in the header.
 * @req REQ-ALM-001 REQ-SAF-007
 */
gg_status_t firmware_sample(firmware_t *fw, uint32_t now_ms)
{
    gg_status_t status = GG_OK;

    if (fw == NULL) {
        status = GG_ERR_NULL;
    } else {
        firmware_gas_t gas;

        status = sample_gas(fw, &gas);
        if (status == GG_OK) {
            status = sample_detector_step(fw, now_ms, &gas);
        }
        if (status == GG_OK) {
            status = sample_report_fault(fw, now_ms);
        }
    }
    return status;
}

/**
 * @brief Drive the outputs from the detector state.
 *
 * The full contract is in the header.
 * @req REQ-SAF-001 REQ-HMI-003
 */
gg_status_t firmware_output(const firmware_t *fw)
{
    gg_status_t status = GG_OK;
    bool energised = false;

    if (fw == NULL) {
        status = GG_ERR_NULL;
    } else {
        status = detector_relay_energised(&fw->det, &energised);
        if (status == GG_OK) {
            status = relay_set(energised);
        }
        if (status == GG_OK) {
            detector_state_t state = firmware_state(fw);
            bool sound = (state == DET_STATE_ALARM_1) || (state == DET_STATE_ALARM_2) ||
                         (state == DET_STATE_FAULT);

            status = buzzer_set(sound);
        }
    }
    return status;
}

/**
 * @brief Current detector state.
 *
 * The full contract is in the header.
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
 * @brief Whether the relay coil should be energised now.
 *
 * The full contract is in the header.
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
