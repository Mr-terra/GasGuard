/**
 * @file hal_uart_fake.h
 * @brief Host-only control API of the UART fake (PC tests only, never on AVR).
 */
#ifndef HAL_UART_FAKE_H
// DEV-02: include guard, not an unused macro
// cppcheck-suppress misra-c2012-2.5
#define HAL_UART_FAKE_H

#include <stdint.h>

#include "common/gg_types.h"

/**
 * @brief Empty both the transmit capture and the receive queue.
 * @req REQ-LOG-004
 */
// DEV-04: HAL interface or test control API, one definition on the PC
// cppcheck-suppress misra-c2012-8.7
void hal_uart_fake_reset(void);

/**
 * @brief Number of bytes the code under test has transmitted since the reset.
 * @req REQ-LOG-004
 */
// DEV-04: HAL interface or test control API, one definition on the PC
// cppcheck-suppress misra-c2012-8.7
uint8_t hal_uart_fake_tx_count(void);

/**
 * @brief Read one transmitted byte, by its position.
 * @param index     Position, 0 .. hal_uart_fake_tx_count() - 1.
 * @param out_byte  Receives the byte.
 * @return GG_OK, GG_ERR_NULL, or GG_ERR_RANGE if index is past the end.
 * @req REQ-LOG-004
 */
// DEV-04: HAL interface or test control API, one definition on the PC
// cppcheck-suppress misra-c2012-8.7
gg_status_t hal_uart_fake_tx_byte(uint8_t index, uint8_t *out_byte);

/**
 * @brief Queue a byte as if it had arrived on the line.
 * @param byte  Received byte.
 * @return GG_OK, or GG_ERR_RANGE if the receive queue is full.
 * @req REQ-LOG-004
 */
// DEV-04: HAL interface or test control API, one definition on the PC
// cppcheck-suppress misra-c2012-8.7
gg_status_t hal_uart_fake_push_rx(uint8_t byte);

#endif /* HAL_UART_FAKE_H */
