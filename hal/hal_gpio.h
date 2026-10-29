/**
 * @file hal_gpio.h
 * @brief Hardware interface: digital outputs and inputs used by the board.
 *
 * Pin mapping is in docs/02-architecture.md §2 (D2 button, D3 touch,
 * D5 relay, D6 buzzer).
 *
 * @req REQ-SAF-001 REQ-HMI-007
 */
#ifndef HAL_GPIO_H
// DEV-02: include guard, not an unused macro
// cppcheck-suppress misra-c2012-2.5
#define HAL_GPIO_H

#include <stdbool.h>
#include <stdint.h>

#include "common/gg_types.h"

/** @brief Logical names of the digital pins, independent of the AVR port. */
typedef enum {
    HAL_GPIO_PIN_BUTTON = 0, /**< D2, active low, pulled up. */
    HAL_GPIO_PIN_TOUCH,      /**< D3, touch sensor input. */
    HAL_GPIO_PIN_RELAY,      /**< D5, alarm relay output. */
    HAL_GPIO_PIN_BUZZER      /**< D6, buzzer output (timer driven). */
} hal_gpio_pin_t;

/**
 * @brief Configure every pin with its direction and idle level.
 *
 * The relay is driven to its fail-safe level first (REQ-SAF-001).
 *
 * @return GG_OK on success.
 * @req REQ-SAF-001
 */
// DEV-04: HAL interface or test control API, one definition on the PC
// cppcheck-suppress misra-c2012-8.7
gg_status_t hal_gpio_init(void);

/**
 * @brief Set an output pin.
 *
 * @param pin    Pin to write; must be an output (relay or buzzer).
 * @param high   true drives the pin high, false low.
 * @return GG_OK, GG_ERR_RANGE for an unknown pin or an input pin.
 * @req REQ-SAF-001
 */
// DEV-04: HAL interface or test control API, one definition on the PC
// cppcheck-suppress misra-c2012-8.7
gg_status_t hal_gpio_write(hal_gpio_pin_t pin, bool high);

/**
 * @brief Read an input pin.
 *
 * @param pin       Pin to read; must be an input (button or touch).
 * @param out_high  Receives true if the pin is high.
 * @return GG_OK, GG_ERR_NULL, or GG_ERR_RANGE for an unknown or output pin.
 * @req REQ-HMI-007
 */
// DEV-04: HAL interface or test control API, one definition on the PC
// cppcheck-suppress misra-c2012-8.7
gg_status_t hal_gpio_read(hal_gpio_pin_t pin, bool *out_high);

#endif /* HAL_GPIO_H */
