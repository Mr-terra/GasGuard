/**
 * @file hal_gpio_fake.h
 * @brief Host-only control API of the GPIO fake (PC tests only, never on AVR).
 */
#ifndef HAL_GPIO_FAKE_H
// DEV-02: include guard, not an unused macro
// cppcheck-suppress misra-c2012-2.5
#define HAL_GPIO_FAKE_H

#include <stdbool.h>

#include "common/gg_types.h"
#include "hal/hal_gpio.h"

/**
 * @brief Drive every pin low.
 * @req REQ-SAF-001
 */
void hal_gpio_fake_reset(void);

/**
 * @brief Script the level an input pin reports to hal_gpio_read().
 * @param pin   An input pin (button or touch).
 * @param high  Level to report.
 * @return GG_OK, or GG_ERR_RANGE if pin is not an input.
 * @req REQ-HMI-007
 */
// DEV-04: HAL interface or test control API, one definition on the PC
// cppcheck-suppress misra-c2012-8.7
gg_status_t hal_gpio_fake_set_input(hal_gpio_pin_t pin, bool high);

/**
 * @brief Read back the level last driven or scripted on a pin.
 * @param pin  Any pin.
 * @return true if the pin is high; false otherwise, including for unknown pins.
 * @req REQ-SAF-001
 */
// DEV-04: HAL interface or test control API, one definition on the PC
// cppcheck-suppress misra-c2012-8.7
bool hal_gpio_fake_level(hal_gpio_pin_t pin);

#endif /* HAL_GPIO_FAKE_H */
