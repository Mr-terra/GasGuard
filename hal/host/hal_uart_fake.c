/**
 * @file hal_uart_fake.c
 * @brief UART with a capture buffer for transmitted bytes and a queue for received ones.
 *
 * How it works
 * ------------
 * Transmitted bytes are copied into a fixed buffer, so a test can check
 * exactly what the firmware printed. A write that does not fit is refused as a
 * whole, which mirrors the board driver: nothing is queued when the buffer is
 * full. Received bytes come from a small ring queue that the test fills with
 * hal_uart_fake_push_rx(); hal_uart_read_byte() pops them in arrival order.
 *
 * @req REQ-LOG-004
 */
#include <stddef.h>
#include <stdint.h>

#include "common/gg_types.h"
#include "hal/hal_uart.h"
#include "hal/host/hal_uart_fake.h"

/** @brief Capacity of the transmit capture, in bytes. */
#define UART_FAKE_TX_CAPACITY 16u

/** @brief Capacity of the receive queue, in bytes. */
#define UART_FAKE_RX_CAPACITY 8u

/** @brief State of the fake UART. */
typedef struct {
    uint8_t tx[UART_FAKE_TX_CAPACITY];
    uint8_t tx_count;
    uint8_t rx[UART_FAKE_RX_CAPACITY];
    uint8_t rx_head;
    uint8_t rx_count;
} uart_fake_state_t;

static uart_fake_state_t s_uart_fake;

void hal_uart_fake_reset(void)
{
    s_uart_fake.tx_count = 0u;
    s_uart_fake.rx_head = 0u;
    s_uart_fake.rx_count = 0u;
}

uint8_t hal_uart_fake_tx_count(void)
{
    return s_uart_fake.tx_count;
}

gg_status_t hal_uart_fake_tx_byte(uint8_t index, uint8_t *out_byte)
{
    gg_status_t status = GG_OK;

    if (out_byte == NULL) {
        status = GG_ERR_NULL;
    } else if (index >= s_uart_fake.tx_count) {
        status = GG_ERR_RANGE;
    } else {
        *out_byte = s_uart_fake.tx[index];
    }
    return status;
}

gg_status_t hal_uart_fake_push_rx(uint8_t byte)
{
    gg_status_t status = GG_OK;

    if (s_uart_fake.rx_count >= UART_FAKE_RX_CAPACITY) {
        status = GG_ERR_RANGE;
    } else {
        uint8_t slot = (uint8_t)((s_uart_fake.rx_head + s_uart_fake.rx_count) % UART_FAKE_RX_CAPACITY);

        s_uart_fake.rx[slot] = byte;
        s_uart_fake.rx_count++;
    }
    return status;
}

gg_status_t hal_uart_init(void)
{
    return GG_OK;
}

gg_status_t hal_uart_write(const uint8_t *data, uint8_t len)
{
    gg_status_t status = GG_OK;
    uint8_t i;

    if (data == NULL) {
        status = GG_ERR_NULL;
    } else if (len > (UART_FAKE_TX_CAPACITY - s_uart_fake.tx_count)) {
        status = GG_ERR_TIMEOUT;
    } else {
        for (i = 0u; i < len; i++) {
            s_uart_fake.tx[s_uart_fake.tx_count] = data[i];
            s_uart_fake.tx_count++;
        }
    }
    return status;
}

gg_status_t hal_uart_read_byte(uint8_t *out_byte)
{
    gg_status_t status = GG_OK;

    if (out_byte == NULL) {
        status = GG_ERR_NULL;
    } else if (s_uart_fake.rx_count == 0u) {
        status = GG_ERR_TIMEOUT;
    } else {
        *out_byte = s_uart_fake.rx[s_uart_fake.rx_head];
        s_uart_fake.rx_head = (uint8_t)((s_uart_fake.rx_head + 1u) % UART_FAKE_RX_CAPACITY);
        s_uart_fake.rx_count--;
    }
    return status;
}
