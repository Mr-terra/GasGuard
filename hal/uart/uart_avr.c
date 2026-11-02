/**
 * @file uart_avr.c
 * @brief UART at 38400 baud, 8N1, with an interrupt-driven transmit ring buffer.
 *
 * How it works
 * ------------
 * hal_uart_write() copies bytes into a 64-byte ring buffer and returns at once.
 * The transmit interrupt (data register empty) moves one byte per interrupt from
 * the buffer into UDR0, and switches itself off when the buffer is empty. The
 * write is all-or-nothing: if the free space is smaller than the request, nothing
 * is queued and GG_ERR_TIMEOUT is returned, as the interface promises. The buffer
 * indexes are changed under ATOMIC_BLOCK where the interrupt also touches them.
 * At 16 MHz, 38400 baud is UBRR = 25 with the normal (not doubled) speed.
 *
 * @req REQ-LOG-004
 */
#include <stddef.h>
#include <stdint.h>

#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/atomic.h>

#include "common/gg_types/gg_types.h"
#include "hal/uart/uart.h"

/** @brief Ring buffer size, a power of two so that an index wraps with a mask. */
#define UART_TX_SIZE 64u
#define UART_TX_MASK (UART_TX_SIZE - 1u)

/** @brief UBRR for 38400 baud at 16 MHz: 16e6 / (16 * 38400) - 1 = 25. */
#define UART_UBRR_38400 25u

/* Shared with the transmit interrupt, hence volatile and the v_ prefix. */
static volatile uint8_t v_tx_buf[UART_TX_SIZE];
static volatile uint8_t v_tx_head;
static volatile uint8_t v_tx_tail;
static volatile uint8_t v_tx_count;

/**
 * @brief Configure the UART at 38400 baud, 8N1.
 *
 * The full contract is in the header.
 * @req REQ-LOG-004
 */
gg_status_t hal_uart_init(void)
{
    UBRR0H = (uint8_t)(UART_UBRR_38400 / 256u);    /* high byte */
    UBRR0L = (uint8_t)(UART_UBRR_38400 % 256u);    /* low byte */
    UCSR0A = 0u;                                         /* normal speed */
    UCSR0C = (uint8_t)((1u << UCSZ01) | (1u << UCSZ00)); /* 8 data bits, no parity, 1 stop */
    UCSR0B = (uint8_t)((1u << RXEN0) | (1u << TXEN0));   /* receiver and transmitter on */
    v_tx_head = 0u;
    v_tx_tail = 0u;
    v_tx_count = 0u;
    return GG_OK;
}

/**
 * @brief Queue bytes for transmission without waiting for the line.
 *
 * The full contract is in the header.
 * @req REQ-LOG-004
 */
gg_status_t hal_uart_write(const uint8_t *data, uint8_t len)
{
    gg_status_t status = GG_OK;

    if ((data == NULL) && (len > 0u)) {
        status = GG_ERR_NULL;
    } else {
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
            /* Casts of plain variables only: the free-space test stays readable. */
            if ((unsigned int)len > (UART_TX_SIZE - (unsigned int)v_tx_count)) {
                status = GG_ERR_TIMEOUT;               /* nothing queued */
            } else {
                for (uint8_t i = 0u; i < len; i++) {
                    v_tx_buf[v_tx_head] = data[i];
                    v_tx_head = (uint8_t)((v_tx_head + 1u) & UART_TX_MASK);
                }
                v_tx_count = (uint8_t)(v_tx_count + len);
                UCSR0B = (uint8_t)(UCSR0B | (1u << UDRIE0));   /* start the interrupt */
            }
        }
    }
    return status;
}

/**
 * @brief Read one received byte, if any.
 *
 * The full contract is in the header.
 * @req REQ-LOG-004
 */
gg_status_t hal_uart_read_byte(uint8_t *out_byte)
{
    gg_status_t status = GG_OK;

    if (out_byte == NULL) {
        status = GG_ERR_NULL;
    } else if ((UCSR0A & (1u << RXC0)) == 0u) {
        status = GG_ERR_TIMEOUT;                       /* no byte waiting */
    } else {
        *out_byte = UDR0;
    }
    return status;
}

// DEV-07: avr-libc ISR() macro seen by cppcheck as an unused parameter
// cppcheck-suppress misra-c2012-2.7
// cppcheck-suppress misra-c2012-8.2
// cppcheck-suppress misra-c2012-8.4
// cppcheck-suppress misra-c2012-8.6
ISR(USART_UDRE_vect)
{
    if (v_tx_count == 0u) {
        UCSR0B = (uint8_t)(UCSR0B & (uint8_t)~(1u << UDRIE0));   /* nothing left: stop */
    } else {
        UDR0 = v_tx_buf[v_tx_tail];
        v_tx_tail = (uint8_t)((v_tx_tail + 1u) & UART_TX_MASK);
        v_tx_count--;
    }
}
