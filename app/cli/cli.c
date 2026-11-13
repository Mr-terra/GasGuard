/**
 * @file cli.c
 * @brief Command line: receive a line, run `log`, print the event log as text.
 *
 * How it works
 * ------------
 * Each received byte is appended to a 16-byte line; a line end runs the
 * command. `log` reads the ring from its head, which is the oldest entry once
 * the ring has wrapped, so the lines come out in time order. Unwritten slots
 * fail their checksum and are skipped. Each line is formatted into a small
 * local buffer by hand (no printf, to keep the flash small) and sent with the
 * transmit function the caller gives. The text is handled as bytes, with the
 * ASCII codes named below.
 *
 * @req REQ-LOG-004
 */
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "app/cli/cli.h"
#include "common/gg_types/gg_types.h"
#include "services/eventlog/eventlog.h"

/** @brief Largest output line: slot, uptime, name, argument and the line end. */
#define CLI_OUT_MAX 40u

/** @brief Digits of a 32-bit decimal number. */
#define CLI_UINT32_DIGITS 10u

/** @brief ASCII codes used by the command line. */
#define CLI_ASCII_CR    13u    /* '\r' */
#define CLI_ASCII_LF    10u    /* '\n' */
#define CLI_ASCII_SPACE 32u    /* ' ' */
#define CLI_ASCII_ZERO  48u    /* '0' */
#define CLI_ASCII_D     68u    /* 'D' */
#define CLI_ASCII_E     69u    /* 'E' */
#define CLI_ASCII_N     78u    /* 'N' */
#define CLI_ASCII_G    103u    /* 'g' */
#define CLI_ASCII_L    108u    /* 'l' */
#define CLI_ASCII_O    111u    /* 'o' */

/** @brief Output line being built. */
typedef struct {
    uint8_t buf[CLI_OUT_MAX];  /**< Bytes of the line. */
    uint8_t len;               /**< Bytes in use, at most CLI_OUT_MAX. */
} cli_out_t;

/** @brief Append one byte, if there is room. */
static void out_byte(cli_out_t *out, uint8_t c)
{
    if (out->len < CLI_OUT_MAX) {
        out->buf[out->len] = c;
        out->len++;
    }
}

/** @brief Append a zero-terminated text, if there is room. */
static void out_text(cli_out_t *out, const char *text)
{
    uint8_t i = 0u;

    while (text[i] != '\0') {
        out_byte(out, (uint8_t)text[i]);
        i++;
    }
}

/** @brief Append a 32-bit decimal number, most significant digit first. */
static void out_uint32(cli_out_t *out, uint32_t value)
{
    uint8_t digits[CLI_UINT32_DIGITS];
    uint32_t remaining = value;
    uint8_t n = 0u;

    do {
        digits[n] = (uint8_t)(CLI_ASCII_ZERO + (uint8_t)(remaining % 10u));
        n++;
        remaining = remaining / 10u;
    } while ((remaining > 0u) && (n < CLI_UINT32_DIGITS));
    while (n > 0u) {
        n--;
        out_byte(out, digits[n]);
    }
}

/** @brief Send the line built so far, and empty it. */
static gg_status_t out_flush(cli_out_t *out, cli_write_fn write)
{
    gg_status_t status = write(out->buf, out->len);

    out->len = 0u;
    return status;
}

/** @brief Name of an event code, as printed in the log (REQ-LOG-004). */
static const char *code_name(eventlog_code_t code)
{
    const char *name = "UNKNOWN";

    switch (code) {
        case EVENTLOG_MODE_CHANGE:
            name = "MODE";
            break;
        case EVENTLOG_FAULT:
            name = "FAULT";
            break;
        case EVENTLOG_RESET:
            name = "RESET";
            break;
        case EVENTLOG_CONFIG:
            name = "CONFIG";
            break;
        case EVENTLOG_OVERRUN:
            name = "OVERRUN";
            break;
        case EVENTLOG_CHANNEL_FAULT:
            name = "CHANNEL";
            break;
        case EVENTLOG_DISPLAY_FAULT:
            name = "DISPLAY";
            break;
        case EVENTLOG_CHANNEL_RECOVERED:
            name = "RECOVERED";
            break;
        case EVENTLOG_BANDGAP:
            name = "BANDGAP";
            break;
        default:
            name = "UNKNOWN";
            break;
    }
    return name;
}

/** @brief Print one entry: "<slot> <uptime_s> <code> <arg>" and the line end. */
static gg_status_t print_entry(uint8_t slot, const eventlog_entry_t *entry, cli_write_fn write)
{
    cli_out_t out;

    out.len = 0u;
    out_uint32(&out, slot);
    out_byte(&out, CLI_ASCII_SPACE);
    out_uint32(&out, entry->uptime_s);
    out_byte(&out, CLI_ASCII_SPACE);
    out_text(&out, code_name(entry->code));
    out_byte(&out, CLI_ASCII_SPACE);
    out_uint32(&out, entry->arg);
    out_byte(&out, CLI_ASCII_CR);
    out_byte(&out, CLI_ASCII_LF);
    return out_flush(&out, write);
}

/** @brief Print the END line that closes a dump. */
static gg_status_t print_end(cli_write_fn write)
{
    cli_out_t end;

    end.len = 0u;
    out_byte(&end, CLI_ASCII_E);
    out_byte(&end, CLI_ASCII_N);
    out_byte(&end, CLI_ASCII_D);
    out_byte(&end, CLI_ASCII_CR);
    out_byte(&end, CLI_ASCII_LF);
    return out_flush(&end, write);
}

/**
 * @brief Take the next slot of the dump: print it, or skip it if it holds no valid entry.
 *
 * The position moves on only when the line went out, so a refused write is
 * retried by the next poll (REQ-LOG-004).
 *
 * @param cli    Parser state, with a dump in progress.
 * @param write  Transmit function.
 * @return GG_OK, or the transmit or event log error.
 */
static gg_status_t dump_next(cli_t *cli, cli_write_fn write)
{
    eventlog_entry_t entry;
    /* The ring is read from its head: head + index wraps once at the end of the ring. */
    uint8_t slot = (uint8_t)(cli->dump_head + cli->dump_index);
    gg_status_t status;

    if (slot >= EVENTLOG_ENTRIES) {
        slot = (uint8_t)(slot - EVENTLOG_ENTRIES);
    }
    status = eventlog_read_slot(slot, &entry);
    if (status == GG_OK) {
        status = print_entry(slot, &entry, write);
        if (status == GG_OK) {
            cli->dump_index++;
        }
    } else {
        /* Never written, or corrupted: not part of the log. Other errors are returned. */
        cli->dump_index++;
        if (status == GG_ERR_CRC) {
            status = GG_OK;
        }
    }
    return status;
}

/** @brief Start a dump: take the ring head now, print from the next poll on. */
static gg_status_t start_dump(cli_t *cli)
{
    gg_status_t status = eventlog_head(&cli->dump_head);

    if (status == GG_OK) {
        cli->dump_index = 0u;
        cli->dumping = true;
    }
    return status;
}

/** @brief True when the line is exactly "log". */
static bool line_is_log(const cli_t *cli)
{
    return (cli->len == 3u) && (cli->line[0] == CLI_ASCII_L) && (cli->line[1] == CLI_ASCII_O)
           && (cli->line[2] == CLI_ASCII_G);
}

/** @brief Run the command of the finished line. An empty line does nothing. */
static gg_status_t run_line(cli_t *cli, cli_write_fn write)
{
    gg_status_t status = GG_OK;
    cli_out_t err;

    if (line_is_log(cli)) {
        status = start_dump(cli);
    } else if (cli->len > 0u) {
        err.len = 0u;
        out_text(&err, "ERR\r\n");
        status = out_flush(&err, write);
    } else {
        /* Empty line: nothing to answer. */
    }
    return status;
}

gg_status_t cli_init(cli_t *cli)
{
    gg_status_t status = GG_OK;

    if (cli == NULL) {
        status = GG_ERR_NULL;
    } else {
        cli->len = 0u;
        cli->dumping = false;
        cli->dump_head = 0u;
        cli->dump_index = 0u;
    }
    return status;
}

gg_status_t cli_feed(cli_t *cli, uint8_t byte, cli_write_fn write)
{
    gg_status_t status = GG_OK;

    if ((cli == NULL) || (write == NULL)) {
        status = GG_ERR_NULL;
    } else if (byte == CLI_ASCII_CR) {
        /* Carriage returns are ignored: the line ends on the newline. */
    } else if (byte == CLI_ASCII_LF) {
        status = run_line(cli, write);
        cli->len = 0u;
    } else {
        if (cli->len < CLI_LINE_MAX) {
            cli->line[cli->len] = byte;
            cli->len++;
        }
    }
    return status;
}

gg_status_t cli_poll(cli_t *cli, cli_write_fn write)
{
    gg_status_t status = GG_OK;

    if ((cli == NULL) || (write == NULL)) {
        status = GG_ERR_NULL;
    } else if (!cli->dumping) {
        /* No dump: nothing to send. */
    } else if (cli->dump_index < EVENTLOG_ENTRIES) {
        status = dump_next(cli, write);
    } else {
        status = print_end(write);
        if (status == GG_OK) {
            cli->dumping = false;
        }
    }
    return status;
}
