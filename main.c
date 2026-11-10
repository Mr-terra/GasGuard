/**
 * @file main.c
 * @brief Firmware entry point: boot, cooperative loop, watchdog.
 *
 * This file runs the firmware; the bring-up checks are in the git history.
 *
 * Boot order matters for fail-safety (F21): the GPIO driver sets the relay low
 * before anything else. Then the reset cause, the configuration and the
 * detector are started (firmware_boot), the stack is painted (REQ-SAF-009), and
 * the watchdog is enabled.
 *
 * Each pass of the loop is one scheduler cycle. The tasks run in the order of
 * their identifiers; each is run when due and reported as serviced. The cycle
 * reports overruns to the firmware, and the watchdog is kicked only when every
 * task was serviced and the scheduler is healthy (REQ-SAF-005, REQ-SAF-008).
 *
 * @req REQ-SAF-001 REQ-SAF-005 REQ-SAF-006 REQ-SAF-008 REQ-SAF-009
 * @req REQ-HMI-001 REQ-HMI-002 REQ-HMI-003 REQ-LOG-004
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <avr/interrupt.h>

#include "app/cli/cli.h"
#include "app/firmware/firmware.h"
#include "app/hmi/hmi.h"
#include "common/gg_types/gg_types.h"
#include "drivers/lcd_rgb/lcd_rgb.h"
#include "hal/gpio/gpio.h"
#include "hal/i2c/i2c.h"
#include "hal/reset_cause/reset_cause.h"
#include "hal/stack/stack.h"
#include "hal/timer/timer.h"
#include "hal/uart/uart.h"
#include "hal/wdt/wdt.h"
#include "services/scheduler/scheduler.h"

/** @brief Scheduler tasks, in the order they run in each cycle. */
#define TASK_SAMPLE  0u
#define TASK_HMI     1u
#define TASK_OUTPUT  2u
#define TASK_DISPLAY 3u
#define TASK_HEALTH  4u
#define TASK_CLI     5u
#define TASK_COUNT   6u

/** @brief Periods of the tasks, in milliseconds, in the order of the task identifiers. */
#define PERIOD_SAMPLE_MS  100u
#define PERIOD_HMI_MS     50u
#define PERIOD_OUTPUT_MS  FIRMWARE_OUTPUT_PERIOD_MS
#define PERIOD_DISPLAY_MS 500u
#define PERIOD_HEALTH_MS  1000u
#define PERIOD_CLI_MS     10u

/** @brief Backlight levels per state (REQ-HMI-002), in 0..255 levels. */
#define LEVEL_OFF   0x00u
#define LEVEL_FULL  0xFFu
#define LEVEL_HALF  0x80u

/** @brief Display rows: state text on the first, concentration on the second. */
#define DISPLAY_ROW_STATE 0u
#define DISPLAY_ROW_CONC  1u
#define DISPLAY_BLINK_MS  500u

/** @brief Bytes read from the UART per cycle: the command line never blocks the loop. */
#define CLI_BYTES_PER_CYCLE 8u

/** @brief Period of each task, indexed by task identifier. */
static const uint32_t k_periods[TASK_COUNT] = {
    PERIOD_SAMPLE_MS, PERIOD_HMI_MS, PERIOD_OUTPUT_MS, PERIOD_DISPLAY_MS, PERIOD_HEALTH_MS, PERIOD_CLI_MS
};

/** @brief Firmware state, scheduler and command line: one instance each for the whole program. */
static firmware_t fw;
static scheduler_t sched;
static cli_t cli;

/** @brief True when the display answered at boot. Without it the display task is skipped. */
static bool display_present;

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
 * @brief Text shown on the first row for a detector state (REQ-HMI-001).
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
        case DET_STATE_MAINTENANCE:
            text = "MAINT";
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
 * NORMAL green; WARMUP, ALARM_1 and MAINTENANCE yellow; ALARM_2 red; FAULT amber.
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
        case DET_STATE_MAINTENANCE:
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

/** @brief What the display shows now, so that only the changes are sent over I2C. */
typedef struct {
    bool valid;                       /**< False until the first full refresh. */
    bool test;                        /**< The functional test was shown. */
    detector_state_t state;           /**< State shown on the first row. */
    uint8_t red;                      /**< Backlight red level shown. */
    uint8_t green;                    /**< Backlight green level shown. */
    uint8_t blue;                     /**< Backlight blue level shown. */
    char conc[HMI_LEL_TEXT_MAX];      /**< Concentration text shown on the second row. */
} display_shown_t;

static display_shown_t shown;

/**
 * @brief Compare two zero-terminated texts of at most HMI_LEL_TEXT_MAX bytes.
 * @param a  First text.
 * @param b  Second text.
 * @return true when they are equal.
 */
static bool same_text(const char *a, const char *b)
{
    uint8_t i = 0u;
    bool equal = true;

    while ((i < HMI_LEL_TEXT_MAX) && equal) {
        equal = (a[i] == b[i]);
        if ((a[i] == '\0') || (b[i] == '\0')) {
            break;
        }
        i++;
    }
    return equal;
}

/**
 * @brief Refresh the display with the current state (REQ-HMI-001, REQ-HMI-002).
 *
 * Only what changed is written: the LCD is on a slow I2C bus, and every write
 * costs time inside the cycle. A failed write is retried on the next refresh.
 * The display is not safety-critical: its errors do not stop the watchdog.
 *
 * @param now_ms  Current time, for the blink of FAULT.
 * @return GG_OK.
 */
static gg_status_t task_display(uint32_t now_ms)
{
    detector_state_t state = firmware_state(&fw);
    if (!display_present) {
        return GG_OK;   /* No display: the detector runs without it (REQ-HMI-001 is logged at boot). */
    }
    bool test = firmware_test_running(&fw);
    uint8_t red = 0u;
    uint8_t green = 0u;
    uint8_t blue = 0u;
    bool blink_off = (state == DET_STATE_FAULT) && (((now_ms / DISPLAY_BLINK_MS) % 2u) == 1u);
    char conc_text[HMI_LEL_TEXT_MAX];

    state_colour(state, &red, &green, &blue);
    if (blink_off) {
        red = LEVEL_OFF;
        green = LEVEL_OFF;
    }
    if (!shown.valid || (shown.test != test) || (shown.state != state)) {
        if (lcd_rgb_write_line(DISPLAY_ROW_STATE, test ? "TEST" : state_text(state)) == GG_OK) {
            shown.test = test;
            shown.state = state;
        }
    }
    if (!shown.valid || (shown.red != red) || (shown.green != green) || (shown.blue != blue)) {
        if (lcd_rgb_set_colour(red, green, blue) == GG_OK) {
            shown.red = red;
            shown.green = green;
            shown.blue = blue;
        }
    }
    if (hmi_format_lel(firmware_concentration(&fw), conc_text, HMI_LEL_TEXT_MAX) == GG_OK) {
        if (!shown.valid || !same_text(shown.conc, conc_text)) {
            if (lcd_rgb_write_line(DISPLAY_ROW_CONC, conc_text) == GG_OK) {
                uint8_t i;

                for (i = 0u; i < HMI_LEL_TEXT_MAX; i++) {
                    shown.conc[i] = conc_text[i];
                }
            }
        }
    }
    shown.valid = true;
    return GG_OK;
}

/**
 * @brief Read the operator inputs and run the HMI logic (REQ-HMI-004..008).
 * @param now_ms  Current time.
 * @return The first error, or GG_OK.
 */
static gg_status_t task_hmi(uint32_t now_ms)
{
    bool button_high = false;
    bool touch_high = false;
    gg_status_t status = hal_gpio_read(HAL_GPIO_PIN_BUTTON, &button_high);

    if (status == GG_OK) {
        status = hal_gpio_read(HAL_GPIO_PIN_TOUCH, &touch_high);
    }
    if (status == GG_OK) {
        status = firmware_inputs(&fw, now_ms, button_high, touch_high);
    }
    return status;
}

/**
 * @brief Read the command line from the UART, without blocking (REQ-LOG-004).
 *
 * An error of a command does not stop the watchdog: only the safety tasks do.
 * @return GG_OK.
 */
static gg_status_t task_cli(void)
{
    uint8_t byte = 0u;
    uint8_t n;

    for (n = 0u; n < CLI_BYTES_PER_CYCLE; n++) {
        if (hal_uart_read_byte(&byte) == GG_OK) {
            (void)cli_feed(&cli, byte, hal_uart_write);
        } else {
            break;   /* The rest waits for the next cycle. */
        }
    }
    /* One step of a running dump per cycle: the transmit buffer is only 64 bytes (REQ-LOG-004). */
    (void)cli_poll(&cli, hal_uart_write);
    return GG_OK;
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
        /* The display is not safety-critical: a missing display is logged, and the boot goes on. */
        display_present = (lcd_rgb_init(wait_ms) == GG_OK);
    }
    if (status == GG_OK) {
        status = hal_stack_paint();
    }
    if (status == GG_OK) {
        status = firmware_boot(&fw, hal_reset_cause_get(), 0u);
    }
    if (status == GG_OK) {
        status = firmware_report_display(&fw, display_present, 0u);
    }
    if (status == GG_OK) {
        status = cli_init(&cli);
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
 * @brief Run one task body, identified by its task number.
 * @param task    Task identifier.
 * @param now_ms  Current time.
 * @return The status of the task.
 */
static gg_status_t run_task_body(uint8_t task, uint32_t now_ms)
{
    gg_status_t status = GG_ERR_RANGE;

    switch (task) {
        case TASK_SAMPLE:
            status = firmware_sample(&fw, now_ms);
            break;
        case TASK_HMI:
            status = task_hmi(now_ms);
            break;
        case TASK_OUTPUT:
            status = firmware_output(&fw, now_ms);
            break;
        case TASK_DISPLAY:
            status = task_display(now_ms);
            break;
        case TASK_HEALTH:
            status = firmware_health(&fw, now_ms);
            break;
        case TASK_CLI:
            status = task_cli();
            break;
        default:
            status = GG_ERR_RANGE;   /* Unknown task: the cycle stops, the watchdog is not kicked. */
            break;
    }
    return status;
}

/**
 * @brief Run one task if it is due, then report it as serviced.
 * @param task    Task identifier.
 * @param now_ms  Current time.
 * @return The first error, or GG_OK.
 */
static gg_status_t run_task(uint8_t task, uint32_t now_ms)
{
    bool due = false;
    gg_status_t status = scheduler_task_due(&sched, task, now_ms, &due);

    if ((status == GG_OK) && due) {
        status = run_task_body(task, now_ms);
    }
    if (status == GG_OK) {
        status = scheduler_task_done(&sched, task);
    }
    return status;
}

/**
 * @brief Run every task of one cycle, report the cycle, and kick the watchdog if it is healthy.
 * @param now_ms  Current time.
 * @return The first error of the cycle, or GG_OK.
 */
static gg_status_t cycle(uint32_t now_ms)
{
    gg_status_t status = scheduler_begin_cycle(&sched, now_ms);
    bool overrun = false;
    bool fault = false;
    bool kick = false;
    uint8_t task;

    for (task = 0u; (status == GG_OK) && (task < TASK_COUNT); task++) {
        status = run_task(task, now_ms);
    }
    if (status == GG_OK) {
        status = scheduler_end_cycle(&sched, now_ms, &overrun, &fault);
    }
    if (status == GG_OK) {
        status = firmware_report_timing(&fw, overrun, fault, now_ms);
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

    if (status != GG_OK) {
        /* The watchdog is not armed yet: arm it, and do not service it. The MCU resets,
         * and the boot is tried again from a known state with the relay low (F21). */
        (void)hal_wdt_enable();
        for (;;) {
            /* Wait for the watchdog reset. */
        }
    }
    for (;;) {
        if (status == GG_OK) {
            status = hal_timer_get_ms(&now_ms);
        }
        if (status == GG_OK) {
            status = cycle(now_ms);
        }
        /* After a cycle error the loop stops servicing the watchdog: it resets the MCU
         * (the watchdog is armed at the end of the boot), and the reset starts from a
         * known state with the relay low (F21). */
    }
}
