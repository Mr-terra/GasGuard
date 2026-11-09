/**
 * @file buzzer.c
 * @brief Audible alarm output driver.
 *
 * How it works
 * ------------
 * The buzzer is on pin D6. A high level sounds it. buzzer_init() drives the
 * pin low, so the buzzer starts silent. buzzer_set() passes the requested
 * state to the GPIO HAL. The driver keeps no state: the pin is the state.
 * Timing of the alarm patterns is left to the application (app/hmi).
 *
 * @req REQ-HMI-003
 */
#include <stdbool.h>

#include "common/gg_types/gg_types.h"
#include "drivers/buzzer/buzzer.h"
#include "hal/gpio/gpio.h"

/**
 * @brief Silence the buzzer.
 *
 * The full contract is in the header.
 * @req REQ-HMI-003
 */
gg_status_t buzzer_init(void)
{
    return hal_gpio_write(HAL_GPIO_PIN_BUZZER, false);
}

/**
 * @brief Sound or silence the buzzer.
 *
 * The full contract is in the header.
 * @req REQ-HMI-003
 */
gg_status_t buzzer_set(bool on)
{
    return hal_gpio_write(HAL_GPIO_PIN_BUZZER, on);
}
