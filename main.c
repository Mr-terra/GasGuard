/**
 * @file main.c
 * @brief Bring-up program for the first board test (branch day-3-bringup).
 *
 * It is not the firmware. It only checks the hardware:
 *  - the UART prints a banner, so the serial console can be read;
 *  - the I2C bus is scanned, and every answering address is printed, so the
 *    LCD controller (0x3E) and its RGB backlight (0x62 or 0x30) can be found;
 *  - every second it prints "alive", so the board is seen to run.
 *
 * The full boot sequence, scheduler and watchdog come on Day 4 of PLAN.md.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <avr/interrupt.h>

#include "common/gg_types/gg_types.h"
#include "drivers/lcd_rgb/lcd_rgb.h"
#include "hal/i2c/i2c.h"
#include "hal/timer/timer.h"
#include "hal/uart/uart.h"

/** @brief First and last 7-bit address probed. 0x00..0x07 and 0x78..0x7F are reserved. */
#define BRINGUP_ADDR_FIRST 0x08u
#define BRINGUP_ADDR_LAST  0x77u

/** @brief Green backlight level for the NORMAL state (REQ-HMI-002). */
#define BRINGUP_GREEN_LEVEL 0xFFu

/** @brief The LCD controller (text) and its RGB backlight, which must both answer. */
#define BRINGUP_LCD_TEXT_ADDR 0x3Eu
#define BRINGUP_LCD_RGB_ADDR  0x62u

/** @brief Period of the "alive" line, in milliseconds. */
#define BRINGUP_TICK_MS 1000u

/** @brief How many times a UART write is retried while the transmit buffer is full. */
#define BRINGUP_TX_RETRIES 1000u

/** @brief Shift and mask that split a byte into its high and low nibble. */
#define BRINGUP_NIBBLE_SHIFT 4u
#define BRINGUP_NIBBLE_MASK  0x0Fu

/** @brief Positions in the output line "0xHH\r\n" (the terminating NUL is the last one). */
#define BRINGUP_HEX_HI_POS 2u
#define BRINGUP_HEX_LO_POS 3u
#define BRINGUP_LINE_LEN   6u
#define BRINGUP_LINE_CR    4u
#define BRINGUP_LINE_NUL   5u

/** @brief Hexadecimal digits, indexed by a value 0..15. */
static const char k_hex_digits[] = "0123456789ABCDEF";

/**
 * @brief Send a text, retrying while the transmit buffer is full.
 * @param text  Zero-terminated text, shorter than 255 bytes.
 */
static void send_text(const char *text)
{
    uint8_t len = 0u;
    uint16_t retries = 0u;
    gg_status_t status = GG_ERR_TIMEOUT;

    while (text[len] != '\0') {
        len++;
    }
    while ((status != GG_OK) && (retries < BRINGUP_TX_RETRIES)) {
        status = hal_uart_write((const uint8_t *)text, len);
        retries++;
    }
}

/**
 * @brief Send one byte as two hexadecimal characters, then a line break.
 * @param value  Byte to print.
 */
static void send_hex_line(uint8_t value)
{
    char line[BRINGUP_LINE_LEN];

    line[0] = '0';
    line[1] = 'x';
    line[BRINGUP_HEX_HI_POS] = k_hex_digits[(uint8_t)(value >> BRINGUP_NIBBLE_SHIFT)];
    line[BRINGUP_HEX_LO_POS] = k_hex_digits[(uint8_t)(value & BRINGUP_NIBBLE_MASK)];
    line[BRINGUP_LINE_CR] = '\r';
    line[BRINGUP_LINE_NUL] = '\0';
    send_text(line);
    send_text("\n");
}

/**
 * @brief Busy-wait until at least @p ms milliseconds have passed.
 * @param ms  Delay in milliseconds.
 */
static void wait_ms(uint32_t ms)
{
    uint32_t start = 0u;
    uint32_t now = 0u;

    /* The timer is initialised before any delay, and &start / &now are never NULL,
     * so the status is not checked here. */
    (void)hal_timer_get_ms(&start);
    do {
        (void)hal_timer_get_ms(&now);
    } while ((now - start) < ms);
}

/**
 * @brief Probe every 7-bit address, print the ones that answer, and check the LCD.
 *
 * A write with zero data bytes sends START, the address and STOP. A device
 * that is present acknowledges the address and the call returns GG_OK.
 * The scan fails unless both the LCD controller and its RGB backlight answer.
 * @return GG_OK if both LCD addresses answered, GG_ERR_HW otherwise.
 */
static gg_status_t scan_i2c(void)
{
    uint8_t addr = 0u;
    bool text_found = false;
    bool rgb_found = false;
    gg_status_t status = GG_OK;

    send_text("I2C scan\n");
    for (addr = BRINGUP_ADDR_FIRST; addr <= BRINGUP_ADDR_LAST; addr++) {
        if (hal_i2c_write(addr, NULL, 0u) == GG_OK) {
            send_hex_line(addr);
            if (addr == BRINGUP_LCD_TEXT_ADDR) {
                text_found = true;
            }
            if (addr == BRINGUP_LCD_RGB_ADDR) {
                rgb_found = true;
            }
        }
    }
    send_text("scan done\n");
    if ((!text_found) || (!rgb_found)) {
        send_text("ERROR: LCD not found (expected 0x3E and 0x62)\n");
        status = GG_ERR_HW;
    }
    return status;
}

/**
 * @brief Initialise the LCD, show "GasGuard" and set the backlight to green (NORMAL).
 * @return GG_OK, or the first error from the LCD driver.
 */
static gg_status_t show_on_lcd(void)
{
    gg_status_t status = lcd_rgb_init(wait_ms);

    if (status == GG_OK) {
        status = lcd_rgb_write_line(0u, "GasGuard");
    }
    if (status == GG_OK) {
        status = lcd_rgb_write_line(1u, "bring-up OK");
    }
    if (status == GG_OK) {
        status = lcd_rgb_set_colour(0x00u, BRINGUP_GREEN_LEVEL, 0x00u);
    }
    return status;
}

/**
 * @brief Initialise the peripherals, then run the scan and the heartbeat forever.
 *
 * Each init call sits in its own statement, so none depends on the short-circuit
 * evaluation of a condition (R11). This file is bring-up only, not a requirement.
 * @return Never returns.
 */
int main(void)
{
    gg_status_t status = hal_timer_init();

    if (status == GG_OK) {
        status = hal_uart_init();
    }
    if (status == GG_OK) {
        status = hal_i2c_init();
    }
    /* The UART transmit and the timer tick both run in interrupts. */
    sei();

    if (status != GG_OK) {
        send_text("init failed\n");
    } else {
        send_text("GasGuard bring-up\n");
        status = scan_i2c();
    }
    if (status == GG_OK) {
        status = show_on_lcd();
        if (status != GG_OK) {
            send_text("ERROR: LCD init failed\n");
        }
    }
    /* On any failure the program stops here: it no longer reports "alive". */
    for (;;) {
        wait_ms(BRINGUP_TICK_MS);
        if (status == GG_OK) {
            send_text("alive\n");
        } else {
            send_text("halted: fix the wiring and reset\n");
        }
    }
}
