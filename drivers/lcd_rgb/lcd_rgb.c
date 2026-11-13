/**
 * @file lcd_rgb.c
 * @brief Grove LCD RGB Backlight v4.0 driver.
 *
 * How it works
 * ------------
 * Every command is sent as an I2C write. The text controller at 0x3E takes a
 * control byte before each item: 0x80 before a command, 0x40 before a
 * character. The backlight chip at 0x62 takes a register address and a value.
 * The init sends the function-set, display, clear and entry-mode commands in a
 * fixed order, with the pauses the controller needs after some of them, then
 * configures the backlight and sets it to white. The sequence comes from the
 * Seeed Grove_LCD_RGB_Backlight library; the pauses are rounded up to whole
 * milliseconds.
 *
 * @req REQ-HMI-001 REQ-HMI-002
 */
#include <stddef.h>
#include <stdint.h>

#include "common/gg_types/gg_types.h"
#include "drivers/lcd_rgb/lcd_rgb.h"
#include "hal/i2c/i2c.h"

#define LCD_TEXT_ADDR 0x3Eu
#define LCD_RGB_ADDR  0x62u

/** @brief Control byte: the next byte is a command. */
#define LCD_CTRL_CMD  0x80u
/** @brief Control byte: the next byte is a character to display. */
#define LCD_CTRL_DATA 0x40u
/** @brief Character code of a space, written after the text to clear the rest of a row. */
#define LCD_CHAR_SPACE 0x20u

/** @brief Set DDRAM address: row 0 starts at 0x00, row 1 at 0x40 (0x80 | address). */
#define LCD_CMD_DDRAM_ROW0 0x80u
#define LCD_CMD_DDRAM_ROW1 0xC0u
/** @brief Number of rows on the display. */
#define LCD_ROWS 2u

/** @brief Pause after power-up, in milliseconds. */
#define LCD_POWER_UP_MS 50u
/** @brief Number of commands in the init sequence. */
#define LCD_INIT_CMD_COUNT 7u

/** @brief Text controller commands used by the init. */
#define LCD_CMD_FUNCTION_SET 0x38u /**< 8-bit bus, two rows, 5x8 dots. */
#define LCD_CMD_DISPLAY_ON   0x0Cu /**< Display on, cursor off, no blink. */
#define LCD_CMD_CLEAR        0x01u /**< Clear the display. */
#define LCD_CMD_ENTRY_MODE   0x06u /**< Cursor moves right, no display shift. */

/** @brief Pauses after some init commands, in milliseconds (the datasheet values, rounded up). */
#define LCD_WAIT_FUNCTION_SET_MS 5u /**< After the first function set (4.5 ms). */
#define LCD_WAIT_SHORT_MS        1u /**< After the second function set (150 us). */
#define LCD_WAIT_CLEAR_MS        2u /**< After the clear (2 ms). */

/** @brief Number of backlight register writes in the init. */
#define LCD_BACKLIGHT_INIT_COUNT 3u

/** @brief Backlight registers of the RGB chip (PCA9633 family). */
#define LCD_REG_MODE1  0x00u
#define LCD_REG_MODE2  0x01u
#define LCD_REG_BLUE   0x02u
#define LCD_REG_GREEN  0x03u
#define LCD_REG_RED    0x04u
#define LCD_REG_LEDOUT 0x08u

/** @brief Full-white backlight level, and the value that turns all LEDs on under PWM control. */
#define LCD_LEVEL_MAX 0xFFu
/** @brief MODE1 value: normal mode, not sleeping. */
#define LCD_MODE1_NORMAL 0x00u
/** @brief MODE2 value: group dimming off, output change on stop command. */
#define LCD_MODE2_NORMAL 0x20u

/** @brief One register write of the backlight chip. */
typedef struct {
    uint8_t reg;
    uint8_t value;
} lcd_rgb_reg_t;

/**
 * @brief Send one command to the text controller.
 * @param command  Command byte.
 * @return Status of the I2C write.
 */
static gg_status_t lcd_text_command(uint8_t command)
{
    uint8_t bytes[2] = { LCD_CTRL_CMD, command };

    return hal_i2c_write(LCD_TEXT_ADDR, bytes, (uint8_t)sizeof(bytes));
}

/**
 * @brief Write one register of the backlight chip.
 * @param reg    Register address.
 * @param value  Value to write.
 * @return Status of the I2C write.
 */
static gg_status_t lcd_rgb_reg(uint8_t reg, uint8_t value)
{
    uint8_t bytes[2] = { reg, value };

    return hal_i2c_write(LCD_RGB_ADDR, bytes, (uint8_t)sizeof(bytes));
}

/**
 * @brief DDRAM command that moves the cursor to column 0 of a row.
 * @param row  Row index, already checked to be below LCD_ROWS.
 * @return The set-DDRAM-address command.
 */
static uint8_t lcd_row_command(uint8_t row)
{
    uint8_t command = LCD_CMD_DDRAM_ROW0;

    if (row == 1u) {
        command = LCD_CMD_DDRAM_ROW1;
    }
    return command;
}

/**
 * @brief Initialise the text controller and the backlight chip, backlight white.
 * @req REQ-HMI-001 REQ-HMI-002
 */
gg_status_t lcd_rgb_init(lcd_rgb_delay_fn delay_ms)
{
    /* Text controller init commands, in order: function set x4, display on, clear, entry mode. */
    static const uint8_t init_cmds[LCD_INIT_CMD_COUNT] = {
        LCD_CMD_FUNCTION_SET, LCD_CMD_FUNCTION_SET, LCD_CMD_FUNCTION_SET, LCD_CMD_FUNCTION_SET,
        LCD_CMD_DISPLAY_ON, LCD_CMD_CLEAR, LCD_CMD_ENTRY_MODE
    };
    /* Pause after each init command, in milliseconds; 0 means no pause. */
    static const uint32_t init_wait_ms[LCD_INIT_CMD_COUNT] = {
        LCD_WAIT_FUNCTION_SET_MS, LCD_WAIT_SHORT_MS, 0u, 0u, 0u, LCD_WAIT_CLEAR_MS, 0u
    };
    /* Backlight chip init: normal mode, all LEDs under PWM control, group dimming off. */
    static const lcd_rgb_reg_t backlight_init[LCD_BACKLIGHT_INIT_COUNT] = {
        { LCD_REG_MODE1, LCD_MODE1_NORMAL },
        { LCD_REG_LEDOUT, LCD_LEVEL_MAX },
        { LCD_REG_MODE2, LCD_MODE2_NORMAL }
    };
    gg_status_t status = GG_OK;

    if (delay_ms == NULL) {
        status = GG_ERR_NULL;
    } else {
        uint8_t i;

        delay_ms(LCD_POWER_UP_MS);
        for (i = 0u; (status == GG_OK) && (i < LCD_INIT_CMD_COUNT); i++) {
            status = lcd_text_command(init_cmds[i]);
            if ((status == GG_OK) && (init_wait_ms[i] > 0u)) {
                delay_ms(init_wait_ms[i]);
            }
        }
        for (i = 0u; (status == GG_OK) && (i < LCD_BACKLIGHT_INIT_COUNT); i++) {
            status = lcd_rgb_reg(backlight_init[i].reg, backlight_init[i].value);
        }
        if (status == GG_OK) {
            status = lcd_rgb_set_colour(LCD_LEVEL_MAX, LCD_LEVEL_MAX, LCD_LEVEL_MAX);
        }
    }
    return status;
}

/**
 * @brief Set the backlight colour.
 * @req REQ-HMI-002
 */
gg_status_t lcd_rgb_set_colour(uint8_t red, uint8_t green, uint8_t blue)
{
    gg_status_t status = lcd_rgb_reg(LCD_REG_RED, red);

    if (status == GG_OK) {
        status = lcd_rgb_reg(LCD_REG_GREEN, green);
    }
    if (status == GG_OK) {
        status = lcd_rgb_reg(LCD_REG_BLUE, blue);
    }
    return status;
}

/**
 * @brief Write one line of text, starting at column 0 of the given row.
 * @req REQ-HMI-001
 */
gg_status_t lcd_rgb_write_line(uint8_t row, const char *text)
{
    gg_status_t status = GG_OK;
    uint8_t len = 0u;

    if (text == NULL) {
        status = GG_ERR_NULL;
    } else if (row >= LCD_ROWS) {
        status = GG_ERR_RANGE;
    } else {
        while ((len < LCD_LINE_LEN) && (text[len] != '\0')) {
            len++;
        }
        if (text[len] != '\0') {
            status = GG_ERR_RANGE;
        } else {
            /* One transfer for the whole line: the control byte 0x40 is followed by the
             * characters, which the controller takes as data. One I2C transfer instead of
             * one per character keeps the display task short (REQ-SAF-008). The row is
             * filled with spaces: a shorter text must clear the end of the previous one. */
            uint8_t bytes[LCD_LINE_LEN + 1u];
            uint8_t i;

            bytes[0] = LCD_CTRL_DATA;
            for (i = 0u; i < LCD_LINE_LEN; i++) {
                if (i < len) {
                    bytes[i + 1u] = (uint8_t)text[i];
                } else {
                    bytes[i + 1u] = LCD_CHAR_SPACE;
                }
            }
            status = lcd_text_command(lcd_row_command(row));
            if (status == GG_OK) {
                status = hal_i2c_write(LCD_TEXT_ADDR, bytes, (uint8_t)(LCD_LINE_LEN + 1u));
            }
        }
    }
    return status;
}
