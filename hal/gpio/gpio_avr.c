/**
 * @file gpio_avr.c
 * @brief GPIO driver for the ATmega328P: buttons, touch sensor, relay and buzzer.
 *
 * How it works
 * ------------
 * The four pins sit on port D: D2 (button) and D3 (touch) are inputs, D5 (relay)
 * and D6 (buzzer) are outputs. Each pin has one bit in DDRD (direction) and one
 * in PORTD (level for outputs, pull-up for inputs) or PIND (level read for
 * inputs). hal_gpio_init() drives the relay to its de-energised level before
 * anything else, so a reset or a fault never starts with the alarm contact closed
 * (REQ-SAF-001). Writing to an input, or reading an output, returns GG_ERR_RANGE.
 *
 * @req REQ-SAF-001 REQ-HMI-007
 */
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include <avr/io.h>

#include "common/gg_types/gg_types.h"
#include "hal/gpio/gpio.h"

/* Bit of each pin inside port D. Pin mapping: docs/02-architecture.md §2. */
#define GPIO_BIT_BUTTON 2u   /* D2, active low, pulled up. */
#define GPIO_BIT_TOUCH  3u   /* D3, touch sensor output. */
#define GPIO_BIT_RELAY  5u   /* D5, relay coil: high = energised. */
#define GPIO_BIT_BUZZER 6u   /* D6, buzzer (PD6 is also OC0A, driven by the timer later). */

#define GPIO_MASK(bit) ((uint8_t)(1u << (bit)))

/* Set or clear bits of a register. The explicit cast is needed: C promotes the
 * operands to int, and -Wconversion rejects the implicit narrowing on assignment. */
#define REG_SET(reg, mask)   ((reg) = (uint8_t)((reg) | (mask)))
#define REG_CLEAR(reg, mask) ((reg) = (uint8_t)((reg) & (uint8_t)~(mask)))

/**
 * @brief Find the port D bit of a pin and whether it is an input.
 *
 * @param pin       Pin to look up.
 * @param bit       Receives the bit number. Must not be NULL.
 * @param is_input  Receives true for button and touch. Must not be NULL.
 * @return GG_OK, or GG_ERR_RANGE for an unknown pin.
 */
static gg_status_t gpio_lookup(hal_gpio_pin_t pin, uint8_t *bit, bool *is_input)
{
    gg_status_t status = GG_OK;

    switch (pin) {
        case HAL_GPIO_PIN_BUTTON:
            *bit = GPIO_BIT_BUTTON;
            *is_input = true;
            break;
        case HAL_GPIO_PIN_TOUCH:
            *bit = GPIO_BIT_TOUCH;
            *is_input = true;
            break;
        case HAL_GPIO_PIN_RELAY:
            *bit = GPIO_BIT_RELAY;
            *is_input = false;
            break;
        case HAL_GPIO_PIN_BUZZER:
            *bit = GPIO_BIT_BUZZER;
            *is_input = false;
            break;
        default:
            /* Impossible pin. DEV-08: the HAL returns the error; the caller raises the fault. */
            status = GG_ERR_RANGE;
            break;
    }
    return status;
}

/**
 * @brief Configure every pin with its direction and idle level.
 * @req REQ-SAF-001
 */
gg_status_t hal_gpio_init(void)
{
    /* Set the output levels before the directions: no glitch on the relay. */
    REG_CLEAR(PORTD, GPIO_MASK(GPIO_BIT_RELAY));     /* relay de-energised (REQ-SAF-001) */
    REG_CLEAR(PORTD, GPIO_MASK(GPIO_BIT_BUZZER));
    REG_SET(PORTD, GPIO_MASK(GPIO_BIT_BUTTON));
    REG_CLEAR(PORTD, GPIO_MASK(GPIO_BIT_TOUCH));

    REG_SET(DDRD, GPIO_MASK(GPIO_BIT_RELAY));
    REG_SET(DDRD, GPIO_MASK(GPIO_BIT_BUZZER));
    REG_CLEAR(DDRD, GPIO_MASK(GPIO_BIT_BUTTON));
    REG_CLEAR(DDRD, GPIO_MASK(GPIO_BIT_TOUCH));
    return GG_OK;
}

/**
 * @brief Set an output pin.
 * @req REQ-SAF-001
 */
gg_status_t hal_gpio_write(hal_gpio_pin_t pin, bool high)
{
    gg_status_t status;
    uint8_t bit = 0u;
    bool is_input = false;

    status = gpio_lookup(pin, &bit, &is_input);
    if ((status == GG_OK) && is_input) {
        status = GG_ERR_RANGE;
    }
    if (status == GG_OK) {
        if (high) {
            REG_SET(PORTD, GPIO_MASK(bit));
        } else {
            REG_CLEAR(PORTD, GPIO_MASK(bit));
        }
    }
    return status;
}

/**
 * @brief Read an input pin.
 * @req REQ-HMI-007
 */
gg_status_t hal_gpio_read(hal_gpio_pin_t pin, bool *out_high)
{
    gg_status_t status;
    uint8_t bit = 0u;
    bool is_input = false;

    if (out_high == NULL) {
        status = GG_ERR_NULL;
    } else {
        status = gpio_lookup(pin, &bit, &is_input);
        if ((status == GG_OK) && !is_input) {
            status = GG_ERR_RANGE;
        }
        if (status == GG_OK) {
            *out_high = ((PIND & GPIO_MASK(bit)) != 0u);
        }
    }
    return status;
}
