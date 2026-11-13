/**
 * @file lcd_rgb.h
 * @brief Grove LCD RGB Backlight v4.0: two lines of text and a colour backlight.
 *
 * The module has two I2C chips: the text controller at 0x3E (two rows of 16
 * characters) and the RGB backlight chip at 0x62. Both must answer for the
 * init to succeed; a missing chip is reported as GG_ERR_TIMEOUT, never hidden.
 *
 * The init needs pauses between some commands. The caller passes the delay
 * function: on the board it waits with the timer, in the host tests it only
 * records the requests.
 *
 * @req REQ-HMI-001 REQ-HMI-002
 */
#ifndef LCD_RGB_H
// DEV-02: include guard, not an unused macro
// cppcheck-suppress misra-c2012-2.5
#define LCD_RGB_H

#include <stdint.h>

#include "common/gg_types/gg_types.h"

/** @brief Characters per row: the display has two rows of this length. */
#define LCD_LINE_LEN 16u

/** @brief Delay function supplied by the caller, in milliseconds. */
typedef void (*lcd_rgb_delay_fn)(uint32_t ms);

/**
 * @brief Initialise the text controller and the backlight chip, backlight white.
 * @param delay_ms  Function that waits for the given number of milliseconds. Must not be NULL.
 * @return GG_OK, GG_ERR_NULL if delay_ms is NULL, or GG_ERR_TIMEOUT if a chip does not answer.
 * @req REQ-HMI-001 REQ-HMI-002
 */
// DEV-05: service/driver/app API, called by app/, main.c and tests; cppcheck sees one caller at this stage
// cppcheck-suppress misra-c2012-8.7
gg_status_t lcd_rgb_init(lcd_rgb_delay_fn delay_ms);

/**
 * @brief Set the backlight colour.
 * @param red    Red level, 0 to 255.
 * @param green  Green level, 0 to 255.
 * @param blue   Blue level, 0 to 255.
 * @return GG_OK, or GG_ERR_TIMEOUT if the backlight chip does not answer.
 * @req REQ-HMI-002
 */
gg_status_t lcd_rgb_set_colour(uint8_t red, uint8_t green, uint8_t blue);

/**
 * @brief Write one line of text, starting at column 0 of the given row.
 *
 * The rest of the row is filled with spaces, so a shorter text never leaves
 * characters of the previous one on the display.
 *
 * @param row   0 for the first line, 1 for the second.
 * @param text  Zero-terminated text of at most LCD_LINE_LEN characters. Must not be NULL.
 * @return GG_OK, GG_ERR_NULL, GG_ERR_RANGE (bad row or text too long), or GG_ERR_TIMEOUT.
 * @req REQ-HMI-001
 */
// DEV-05: service/driver/app API, called by app/, main.c and tests; cppcheck sees one caller at this stage
// cppcheck-suppress misra-c2012-8.7
gg_status_t lcd_rgb_write_line(uint8_t row, const char *text);

#endif /* LCD_RGB_H */
