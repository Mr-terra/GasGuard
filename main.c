/**
 * @file main.c
 * @brief Firmware entry point: boot, cooperative loop, watchdog.
 *
 * The bring-up program that checked the board (scan, banner) is kept in the
 * history at commit 5c578c0. This file runs the firmware.
 *
 * Boot order matters for fail-safety (F21): the GPIO driver sets the relay low
 * before anything else. Then the reset cause, the configuration and the
 * detector are started (firmware_boot), and the watchdog is enabled.
 *
 * Each pass of the loop is one scheduler cycle. Every task is either run (when
 * due) or reported as serviced; the watchdog is kicked only when all of them
 * were serviced (REQ-SAF-005).
 *
 * @req REQ-SAF-001 REQ-SAF-005 REQ-SAF-006 REQ-HMI-001 REQ-HMI-002
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <avr/interrupt.h>

#include "app/firmware/firmware.h"
#include "common/gg_types/gg_types.h"
#include "drivers/lcd_rgb/lcd_rgb.h"
#include "hal/gpio/gpio.h"
#include "hal/i2c/i2c.h"
#include "hal/reset_cause/reset_cause.h"
#include "hal/timer/timer.h"
#include "hal/uart/uart.h"
#include "hal/wdt/wdt.h"
#include "services/scheduler/scheduler.h"

/** @brief Scheduler tasks. */
#define TASK_SAMPLE  0u
#define TASK_OUTPUT  1u
#define TASK_DISPLAY 2u
#define TASK_COUNT   3u

/** @brief Periods of the tasks, in milliseconds. */
#define PERIOD_SAMPLE_MS  100u
#define PERIOD_OUTPUT_MS  100u
#define PERIOD_DISPLAY_MS 500u

/** @brief Backlight colours per state (REQ-HMI-002), in 0..255 levels. */
#define LEVEL_OFF   0x00u
#define LEVEL_FULL  0xFFu
#define LEVEL_HALF  0x80u

/** @brief Number of rows and characters of the display. */
#define DISPLAY_ROW_STATE 0u

/** @brief Scheduler periods, in the order of the task identifiers. */
static const uint32_t k_periods[TASK_COUNT] = {
    PERIOD_SAMPLE_MS, PERIOD_OUTPUT_MS, PERIOD_DISPLAY_MS
};

/** @brief Firmware state and scheduler, one instance each for the whole program. */
static firmware_t fw;
static scheduler_t sched;

/**
 * @brief Busy-wait until at least @p ms milliseconds have passed.
 * @param ms  Delay in milliseconds.
 */
static void wait_ms(uint32_t ms)
{
    uint32_t start = 0u;
    uint32_t now = 0u;

    (void)hal_timer_get_ms(&start);
    do {
        (void)hal_timer_get_ms(&now);
    } while ((now - start) < ms);
}

/**
 * @brief Text shown on the first row for a detector state.
 * @param state  Detector state.
 * @return A zero-terminated text.
 */
static const char *state_text(detector_state_t state)
{
    const char *text = "FAULT";

    switch (state) {
        case DET_STATE_WARMUP:
            text = "WARMUP";
            break;
        case DET_STATE_NORMAL:
            text = "NORMAL";
            break;
        case DET_STATE_ALARM_1:
            text = "ALARM 1";
            break;
        case DET_STATE_ALARM_2:
            text = "ALARM 2";
            break;
        case DET_STATE_FAULT:
            text = "FAULT";
            break;
        default:
            text = "FAULT";
            break;
    }
    return text;
}

/**
 * @brief Backlight colour for a detector state (REQ-HMI-002).
 *
 * NORMAL green, WARMUP and ALARM_1 yellow, ALARM_2 red, FAULT amber.
 * @param state  Detector state.
 * @param red    Receives the red level.
 * @param green  Receives the green level.
 * @param blue   Receives the blue level.
 */
static void state_colour(detector_state_t state, uint8_t *red, uint8_t *green, uint8_t *blue)
{
    *red = LEVEL_FULL;
    *green = LEVEL_OFF;
    *blue = LEVEL_OFF;
    switch (state) {
        case DET_STATE_NORMAL:
            *red = LEVEL_OFF;
            *green = LEVEL_FULL;
            break;
        case DET_STATE_WARMUP:
        case DET_STATE_ALARM_1:
            *green = LEVEL_FULL;
            break;
        case DET_STATE_ALARM_2:
            break;
        case DET_STATE_FAULT:
            *green = LEVEL_HALF;
            break;
        default:
            *green = LEVEL_HALF;
            break;
    }
}

/**
 * @brief Refresh the display with the current state (REQ-HMI-001, REQ-HMI-002).
 */
static void task_display(void)
{
    detector_state_t state = firmware_state(&fw);
    uint8_t red = 0u;
    uint8_t green = 0u;
    uint8_t blue = 0u;

    (void)lcd_rgb_write_line(DISPLAY_ROW_STATE, state_text(state));
    state_colour(state, &red, &green, &blue);
    (void)lcd_rgb_set_colour(red, green, blue);
}

/**
 * @brief Boot: start the peripherals, the firmware and the watchdog.
 * @return GG_OK, or the first error. On error the caller must not enter the loop.
 */
static gg_status_t boot(void)
{
    gg_status_t status = hal_gpio_init();

    if (status == GG_OK) {
        status = hal_timer_init();
    }
    if (status == GG_OK) {
        status = hal_uart_init();
    }
    if (status == GG_OK) {
        status = hal_i2c_init();
    }
    /* The UART transmit and the timer tick both run in interrupts. */
    sei();
    if (status == GG_OK) {
        status = lcd_rgb_init(wait_ms);
    }
    if (status == GG_OK) {
        status = firmware_boot(&fw, hal_reset_cause_get(), 0u);
    }
    if (status == GG_OK) {
        status = scheduler_init(&sched, TASK_COUNT, k_periods, 0u);
    }
    if (status == GG_OK) {
        status = hal_wdt_enable();
    }
    return status;
}

/**
 * @brief Run every due task, then report the cycle and kick the watchdog if it is healthy.
 * @param now_ms  Current time.
 * @return The first error of the cycle, or GG_OK.
 */
static gg_status_t cycle(uint32_t now_ms)
{
    gg_status_t status = scheduler_begin_cycle(&sched, now_ms);
    bool due = false;
    bool overrun = false;
    bool fault = false;
    bool kick = false;

    if (status == GG_OK) {
        status = scheduler_task_due(&sched, TASK_SAMPLE, now_ms, &due);
    }
    if ((status == GG_OK) && due) {
        status = firmware_sample(&fw, now_ms);
    }
    if (status == GG_OK) {
        status = scheduler_task_done(&sched, TASK_SAMPLE);
    }
    if (status == GG_OK) {
        status = scheduler_task_due(&sched, TASK_OUTPUT, now_ms, &due);
    }
    if ((status == GG_OK) && due) {
        status = firmware_output(&fw);
    }
    if (status == GG_OK) {
        status = scheduler_task_done(&sched, TASK_OUTPUT);
    }
    if (status == GG_OK) {
        status = scheduler_task_due(&sched, TASK_DISPLAY, now_ms, &due);
    }
    if ((status == GG_OK) && due) {
        task_display();
    }
    if (status == GG_OK) {
        status = scheduler_task_done(&sched, TASK_DISPLAY);
    }
    if (status == GG_OK) {
        status = scheduler_end_cycle(&sched, now_ms, &overrun, &fault);
    }
    if (status == GG_OK) {
        status = scheduler_watchdog_ok(&sched, &kick);
    }
    if ((status == GG_OK) && kick) {
        status = hal_wdt_kick();
    }
    return status;
}

/**
 * @brief Entry point. Boots, then runs the cycle forever.
 *
 * If the boot fails, the program stops without enabling the watchdog. The relay
 * stays low (set by the GPIO driver first), so a failed boot is the alarm side.
 *
 * @return Never returns.
 */
int main(void)
{
    gg_status_t status = boot();
    uint32_t now_ms = 0u;

    for (;;) {
        if (status == GG_OK) {
            status = hal_timer_get_ms(&now_ms);
        }
        if (status == GG_OK) {
            status = cycle(now_ms);
        }
        /* After an error the loop stops servicing the watchdog: it resets the MCU,
         * and the reset starts from a known state with the relay low (F21). */
    }
}
