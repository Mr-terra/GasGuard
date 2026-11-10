/**
 * @file relay.c
 * @brief Alarm relay coil driver.
 *
 * How it works
 * ------------
 * The relay coil is on pin D5. A high level energises the coil, which is the
 * healthy state. relay_init() drives the pin low first, so the coil starts
 * de-energised; the alarm side is then the default. relay_set() passes the
 * requested state to the GPIO HAL, which does the register work. The driver
 * holds no state of its own: the pin is the state.
 *
 * @req REQ-SAF-001
 */
#include <stdbool.h>

#include "common/gg_types/gg_types.h"
#include "drivers/relay/relay.h"
#include "hal/gpio/gpio.h"

/**
 * @brief Set the relay coil to the de-energised state.
 * @req REQ-SAF-001
 */
gg_status_t relay_init(void)
{
    return hal_gpio_write(HAL_GPIO_PIN_RELAY, false);
}

/**
 * @brief Energise or de-energise the relay coil.
 * @req REQ-SAF-001
 */
gg_status_t relay_set(bool energised)
{
    return hal_gpio_write(HAL_GPIO_PIN_RELAY, energised);
}
