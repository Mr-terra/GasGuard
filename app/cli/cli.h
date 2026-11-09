/**
 * @file cli.h
 * @brief Command line over the UART: the `log` command dumps the event log (REQ-LOG-004).
 *
 * The caller feeds the received bytes one at a time, and polls the dump
 * regularly. The UART transmit buffer is small (64 bytes) and a write is
 * all-or-nothing, so the dump goes out one line per poll: a line that does not
 * fit is sent by a later poll. The caller passes the transmit function, so the
 * parser is testable on the PC.
 *
 * @req REQ-LOG-004
 */
#ifndef CLI_H
// DEV-02: include guard, not an unused macro
// cppcheck-suppress misra-c2012-2.5
#define CLI_H

#include <stdbool.h>
#include <stdint.h>

#include "common/gg_types/gg_types.h"

/** @brief Longest command accepted, without the line end. Longer lines are refused. */
#define CLI_LINE_MAX 16u

/**
 * @brief Transmit function: queue @p len bytes for the UART.
 * @param data  Bytes to send. Must not be NULL.
 * @param len   Number of bytes.
 * @return GG_OK, or an error (GG_ERR_TIMEOUT when the transmit buffer is full).
 */
typedef gg_status_t (*cli_write_fn)(const uint8_t *data, uint8_t len);

/** @brief Parser and dump state. Owned by the caller. */
typedef struct {
    uint8_t line[CLI_LINE_MAX];  /**< Bytes of the current line, not zero-terminated. */
    uint8_t len;                 /**< Number of bytes in line. */
    bool dumping;                /**< True while a log dump is in progress. */
    uint8_t dump_head;           /**< Ring head taken when the dump started. */
    uint8_t dump_index;          /**< Next position of the dump, 0 .. EVENTLOG_ENTRIES. */
} cli_t;

/**
 * @brief Empty the line buffer and stop any dump.
 * @param cli  Parser state. Must not be NULL.
 * @return GG_OK or GG_ERR_NULL.
 * @req REQ-LOG-004
 */
// DEV-05: app API, called by the firmware and tests; cppcheck sees one caller at this stage
// cppcheck-suppress misra-c2012-8.7
gg_status_t cli_init(cli_t *cli);

/**
 * @brief Take one received byte. A line end runs the command.
 *
 * `log` starts the dump; its lines come from cli_poll(). Any other line answers
 * `ERR`. Carriage returns are ignored.
 *
 * @param cli    Parser state. Must not be NULL.
 * @param byte   Received byte.
 * @param write  Transmit function. Must not be NULL.
 * @return GG_OK, GG_ERR_NULL, or the error of the answer or of the event log.
 * @req REQ-LOG-004
 */
// DEV-05: app API, called by the firmware and tests; cppcheck sees one caller at this stage
// cppcheck-suppress misra-c2012-8.7
gg_status_t cli_feed(cli_t *cli, uint8_t byte, cli_write_fn write);

/**
 * @brief Send the next step of a running dump, if the transmit buffer has room.
 *
 * One line, or one skipped slot, per call. A refused write keeps the position,
 * so the line goes out on a later call. The dump ends with the line "END".
 *
 * @param cli    Parser state. Must not be NULL.
 * @param write  Transmit function. Must not be NULL.
 * @return GG_OK when nothing is pending or the step went out, GG_ERR_NULL, or the transmit error.
 * @req REQ-LOG-004
 */
// DEV-05: app API, called by the firmware and tests; cppcheck sees one caller at this stage
// cppcheck-suppress misra-c2012-8.7
gg_status_t cli_poll(cli_t *cli, cli_write_fn write);

#endif /* CLI_H */
