/**
 * @file cli.h
 * @brief Command line over the UART: `log` dumps the event log (REQ-LOG-004), `status`
 * prints the state and the raw inputs (REQ-LOG-005).
 *
 * The caller feeds the received bytes one at a time, and polls the dump
 * regularly. The UART transmit buffer is small (64 bytes) and a write is
 * all-or-nothing, so the dump goes out one line per poll: a line that does not
 * fit is sent by a later poll. The caller passes the transmit function, so the
 * parser is testable on the PC.
 *
 * The `status` answer is one short line, sent at once from a snapshot the
 * caller refreshes with cli_set_status(), so the parser never reads the
 * firmware state itself.
 *
 * @req REQ-LOG-004 REQ-LOG-005
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

/** @brief Snapshot printed by the `status` command (REQ-LOG-005). */
typedef struct {
    uint8_t state;        /**< Detector state, as in the MODE entries of the log. */
    gg_conc_t conc;       /**< Concentration used by the detector, 0.1 %LEL. */
    uint16_t raw_mq5;     /**< Last raw ADC value of the MQ-5 channel. */
    uint16_t raw_mq2;     /**< Last raw ADC value of the MQ-2 channel. */
    uint16_t raw_temp;    /**< Last raw ADC value of the temperature input. */
    bool temp_valid;      /**< True when the temperature reading is in its valid range. */
} cli_status_t;

/** @brief Parser and dump state. Owned by the caller. */
typedef struct {
    uint8_t line[CLI_LINE_MAX];  /**< Bytes of the current line, not zero-terminated. */
    uint8_t len;                 /**< Bytes in use, at most CLI_LINE_MAX. */
    bool dumping;                /**< True while a log dump is in progress. */
    uint8_t dump_head;           /**< Ring head taken when the dump started. */
    uint8_t dump_index;          /**< Next position of the dump, 0 .. EVENTLOG_ENTRIES. */
    cli_status_t status;         /**< Last snapshot given by cli_set_status(). */
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
 * `log` starts the dump; its lines come from cli_poll(). `status` answers at
 * once with the last snapshot. Any other line answers `ERR`. Carriage returns
 * are ignored.
 *
 * @param cli    Parser state. Must not be NULL.
 * @param byte   Received byte.
 * @param write  Transmit function. Must not be NULL.
 * @return GG_OK, GG_ERR_NULL, or the error of the answer or of the event log.
 * @req REQ-LOG-004 REQ-LOG-005
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

/**
 * @brief Keep a copy of the values the `status` command prints.
 * @param cli     Parser state. Must not be NULL.
 * @param status  Snapshot to copy. Must not be NULL.
 * @return GG_OK or GG_ERR_NULL.
 * @req REQ-LOG-005
 */
// DEV-05: app API, called by main.c only (outside the lint scope) and the tests
// cppcheck-suppress misra-c2012-8.7
gg_status_t cli_set_status(cli_t *cli, const cli_status_t *status);

#endif /* CLI_H */
