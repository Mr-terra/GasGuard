/**
 * @file i2c_avr.c
 * @brief I2C (TWI) master at 100 kHz for the LCD and the other bus devices.
 *
 * How it works
 * ------------
 * The TWI hardware runs the bus; the code only issues commands and checks the
 * status code in TWSR at each step. A write is START, SLA+W, one byte per DATA
 * step, then STOP. Each step waits for TWINT with a bounded loop, so a missing
 * device gives GG_ERR_TIMEOUT instead of a hang. Unexpected status codes are
 * GG_ERR_HW. At 16 MHz, TWBR = 72 with prescaler 1 gives 100 kHz.
 *
 * @req REQ-HMI-001 REQ-HMI-002
 */
#include <stddef.h>
#include <stdint.h>

#include <avr/io.h>

#include "common/gg_types/gg_types.h"
#include "hal/i2c/i2c.h"

/** @brief SCL = 16 MHz / (16 + 2 * 72 * 1) = 100 kHz. */
#define I2C_TWBR_100KHZ 72u

/** @brief Upper bound on the wait for one TWI step. */
#define I2C_WAIT_LOOPS 20000u

/** @brief TWSR status codes (prescaler bits masked off). */
#define TW_START        0x08u
#define TW_REP_START    0x10u
#define TW_MT_SLA_ACK   0x18u
#define TW_MT_DATA_ACK  0x28u

/** @brief Start or continue a TWI command and wait for it to finish. */
static gg_status_t twi_command(uint8_t control)
{
    gg_status_t status = GG_OK;
    uint16_t waited = 0u;

    TWCR = (uint8_t)(control | (1u << TWINT) | (1u << TWEN));
    while (((TWCR & (1u << TWINT)) == 0u) && (waited < I2C_WAIT_LOOPS)) {
        waited++;
    }
    if ((TWCR & (1u << TWINT)) == 0u) {
        status = GG_ERR_TIMEOUT;
    }
    return status;
}

gg_status_t hal_i2c_init(void)
{
    TWSR = 0u;                    /* prescaler 1 */
    TWBR = I2C_TWBR_100KHZ;
    TWCR = (uint8_t)(1u << TWEN); /* enable the TWI, the pins become SDA and SCL */
    return GG_OK;
}

/** @brief Send START, then check that the bus accepted it. */
static gg_status_t twi_start(void)
{
    gg_status_t status = twi_command(1u << TWSTA);
    uint8_t st = (uint8_t)(TWSR & 0xF8u);

    if ((status == GG_OK) && (st != TW_START) && (st != TW_REP_START)) {
        status = GG_ERR_HW;
    }
    return status;
}

/** @brief Send one byte and check the acknowledge. 0x20 on SLA+W means no device answered. */
static gg_status_t twi_send_byte(uint8_t byte, uint8_t expected_ack)
{
    gg_status_t status;
    uint8_t st;

    TWDR = byte;
    status = twi_command(0u);
    st = (uint8_t)(TWSR & 0xF8u);
    if ((status == GG_OK) && (st != expected_ack)) {
        status = (st == 0x20u) ? GG_ERR_TIMEOUT : GG_ERR_HW;
    }
    return status;
}

gg_status_t hal_i2c_write(uint8_t addr7, const uint8_t *data, uint8_t len)
{
    gg_status_t status = GG_OK;
    uint8_t i;

    if ((data == NULL) && (len > 0u)) {
        status = GG_ERR_NULL;
    } else if (addr7 > 0x7Fu) {
        status = GG_ERR_RANGE;
    } else {
        status = twi_start();
        if (status == GG_OK) {
            status = twi_send_byte((uint8_t)(addr7 << 1), TW_MT_SLA_ACK);   /* SLA+W */
        }
        for (i = 0u; (status == GG_OK) && (i < len); i++) {
            status = twi_send_byte(data[i], TW_MT_DATA_ACK);
        }
        /* STOP is sent even after an error, so the bus is released. */
        TWCR = (uint8_t)((1u << TWSTO) | (1u << TWINT) | (1u << TWEN));
    }
    return status;
}
