/**
 * @file hal_i2c_fake.c
 * @brief I2C bus with a table of present devices and a record of the last write.
 *
 * How it works
 * ------------
 * A device answers only if the test has declared it present, as a real chip
 * answers only when it is wired and powered. Writing to an absent address
 * times out, which is how the display driver must detect a missing LCD. Each
 * successful write is recorded (address, length and first byte), so a test can
 * check the exact command sent to the backlight or text controller.
 *
 * @req REQ-HMI-001 REQ-HMI-002
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "common/gg_types.h"
#include "hal/hal_i2c.h"
#include "hal/host/hal_i2c_fake.h"

/** @brief Highest valid 7-bit address. */
#define I2C_FAKE_ADDR_MAX 0x7Fu

/** @brief State of the fake bus. */
typedef struct {
    bool present[I2C_FAKE_ADDR_MAX + 1u];
    uint8_t last_addr;
    uint8_t last_len;
    uint8_t last_first_byte;
} i2c_fake_state_t;

static i2c_fake_state_t s_i2c_fake;

void hal_i2c_fake_reset(void)
{
    uint8_t addr;

    for (addr = 0u; addr <= I2C_FAKE_ADDR_MAX; addr++) {
        s_i2c_fake.present[addr] = false;
    }
    s_i2c_fake.last_addr = 0u;
    s_i2c_fake.last_len = 0u;
    s_i2c_fake.last_first_byte = 0u;
}

gg_status_t hal_i2c_fake_set_present(uint8_t addr7, bool present)
{
    gg_status_t status = GG_OK;

    if (addr7 > I2C_FAKE_ADDR_MAX) {
        status = GG_ERR_RANGE;
    } else {
        s_i2c_fake.present[addr7] = present;
    }
    return status;
}

uint8_t hal_i2c_fake_last_addr(void)
{
    return s_i2c_fake.last_addr;
}

uint8_t hal_i2c_fake_last_len(void)
{
    return s_i2c_fake.last_len;
}

uint8_t hal_i2c_fake_last_first_byte(void)
{
    return s_i2c_fake.last_first_byte;
}

gg_status_t hal_i2c_init(void)
{
    return GG_OK;
}

gg_status_t hal_i2c_write(uint8_t addr7, const uint8_t *data, uint8_t len)
{
    gg_status_t status = GG_OK;

    if (data == NULL) {
        status = GG_ERR_NULL;
    } else if ((addr7 > I2C_FAKE_ADDR_MAX) || (len == 0u)) {
        status = GG_ERR_RANGE;
    } else if (!s_i2c_fake.present[addr7]) {
        status = GG_ERR_TIMEOUT;
    } else {
        s_i2c_fake.last_addr = addr7;
        s_i2c_fake.last_len = len;
        s_i2c_fake.last_first_byte = data[0];
    }
    return status;
}
