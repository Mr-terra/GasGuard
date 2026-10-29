/**
 * @file hal_uart.h
 * @brief Hardware interface: UART at 38400 baud, 8N1, over the USB link.
 *
 * Used for the boot banner, the event-log dump and the CLI (REQ-LOG-004).
 * All operations are non-blocking (docs/02-architecture.md §4).
 *
 * @req REQ-LOG-004
 */
#ifndef HAL_UART_H
#define HAL_UART_H

#include <stdint.h>

#include "common/gg_types.h"

/**
 * @brief Configure the UART at 38400 baud, 8N1.
 * @return GG_OK on success.
 * @req REQ-LOG-004
 */
gg_status_t hal_uart_init(void);

/**
 * @brief Queue bytes for transmission without waiting for the line.
 *
 * @param data  Bytes to send.
 * @param len   Number of bytes.
 * @return GG_OK if all bytes were queued, GG_ERR_TIMEOUT if the transmit
 *         buffer is full (nothing queued), GG_ERR_NULL if data is NULL.
 * @req REQ-LOG-004
 */
gg_status_t hal_uart_write(const uint8_t *data, uint8_t len);

/**
 * @brief Read one received byte, if any.
 *
 * @param out_byte  Receives the byte.
 * @return GG_OK if a byte was read, GG_ERR_TIMEOUT if none is waiting,
 *         GG_ERR_NULL if out_byte is NULL.
 * @req REQ-LOG-004
 */
gg_status_t hal_uart_read_byte(uint8_t *out_byte);

#endif /* HAL_UART_H */
