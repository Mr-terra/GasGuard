/**
 * @file hal_gpio_fake.c
 * @brief Pin levels kept in RAM, with the direction rules of the real pins.
 *
 * How it works
 * ------------
 * Every pin has a level in a static array. The buttons and the touch sensor
 * are inputs: the code under test can read them, and the test sets them with
 * hal_gpio_fake_set_input(). The relay and the buzzer are outputs: the code
 * writes them, and the test reads them back with hal_gpio_fake_level(). A
 * switch over the enum checks the direction, so writing an input (or reading
 * an output) returns GG_ERR_RANGE, just as the board driver must.
 *
 * @req REQ-SAF-001 REQ-HMI-007
 */
#include <stdbool.h>
#include <stddef.h>

#include "common/gg_types.h"
#include "hal/hal_gpio.h"
#include "hal/host/hal_gpio_fake.h"

/** @brief Number of pins in hal_gpio_pin_t. */
#define GPIO_FAKE_PIN_COUNT 4u

/** @brief Level of every pin, indexed by hal_gpio_pin_t. */
typedef struct {
    bool level[GPIO_FAKE_PIN_COUNT];
} gpio_fake_state_t;

static gpio_fake_state_t s_gpio_fake;

/**
 * @brief Check that a pin has the expected direction.
 * @param pin         Pin to check.
 * @param want_input  true to require an input pin, false for an output pin.
 * @return GG_OK if the direction matches, GG_ERR_RANGE otherwise.
 */
static gg_status_t check_direction(hal_gpio_pin_t pin, bool want_input)
{
    gg_status_t status = GG_ERR_RANGE;

    switch (pin) {
    case HAL_GPIO_PIN_BUTTON:
    case HAL_GPIO_PIN_TOUCH:
        if (want_input) {
            status = GG_OK;
        }
        break;
    case HAL_GPIO_PIN_RELAY:
    case HAL_GPIO_PIN_BUZZER:
        if (!want_input) {
            status = GG_OK;
        }
        break;
    default:
        status = GG_ERR_RANGE;
        break;
    }
    return status;
}

void hal_gpio_fake_reset(void)
{
    uint8_t pin;

    for (pin = 0u; pin < GPIO_FAKE_PIN_COUNT; pin++) {
        s_gpio_fake.level[pin] = false;
    }
}

gg_status_t hal_gpio_fake_set_input(hal_gpio_pin_t pin, bool high)
{
    gg_status_t status = check_direction(pin, true);

    if (status == GG_OK) {
        s_gpio_fake.level[pin] = high;
    }
    return status;
}

bool hal_gpio_fake_level(hal_gpio_pin_t pin)
{
    bool high = false;

    if ((unsigned int)pin < GPIO_FAKE_PIN_COUNT) {
        high = s_gpio_fake.level[pin];
    }
    return high;
}

gg_status_t hal_gpio_init(void)
{
    hal_gpio_fake_reset();
    return GG_OK;
}

gg_status_t hal_gpio_write(hal_gpio_pin_t pin, bool high)
{
    gg_status_t status = check_direction(pin, false);

    if (status == GG_OK) {
        s_gpio_fake.level[pin] = high;
    }
    return status;
}

gg_status_t hal_gpio_read(hal_gpio_pin_t pin, bool *out_high)
{
    gg_status_t status = GG_OK;

    if (out_high == NULL) {
        status = GG_ERR_NULL;
    } else {
        status = check_direction(pin, true);
        if (status == GG_OK) {
            *out_high = s_gpio_fake.level[pin];
        }
    }
    return status;
}
