/**
 * @file eventlog.c
 * @brief Event log ring in EEPROM.
 *
 * How it works
 * ------------
 * Each event is written in the slot named by the head, then the head moves on
 * by one, wrapping after 32 slots, so the newest event overwrites the oldest.
 * An entry is 8 bytes: the uptime (4 bytes, little-endian), the code, the
 * argument (2 bytes) and a checksum. The checksum is the XOR of the first seven
 * bytes with a seed of 0xA5. The seed makes an erased or zeroed slot invalid,
 * so a slot that was never written is not read back as an event. Writing only
 * on events keeps the wear of the EEPROM low (REQ-LOG-003).
 *
 * @req REQ-LOG-001 REQ-LOG-002 REQ-LOG-003
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "common/gg_types/gg_types.h"
#include "hal/eeprom/eeprom.h"
#include "services/eventlog/eventlog.h"

/** @brief Byte offsets inside one entry. */
#define EVENTLOG_OFS_UPTIME   0u
#define EVENTLOG_OFS_CODE     4u
#define EVENTLOG_OFS_ARG      5u
#define EVENTLOG_OFS_CHECK    7u
/** @brief Bytes covered by the checksum. */
#define EVENTLOG_CHECK_COVERED EVENTLOG_OFS_CHECK
/** @brief Seed of the checksum; makes an erased slot invalid. */
#define EVENTLOG_CHECK_SEED 0xA5u

/** @brief Byte helpers for the little-endian fields. */
#define EVENTLOG_BYTE_MASK  0xFFu
#define EVENTLOG_BYTE_SHIFT 8u
#define EVENTLOG_UPTIME_SHIFT 8u
/** @brief Bytes of the uptime field. */
#define EVENTLOG_UPTIME_BYTES 4u

/** @brief Smallest and largest event code. */
#define EVENTLOG_CODE_MIN EVENTLOG_MODE_CHANGE
#define EVENTLOG_CODE_MAX EVENTLOG_CONFIG

/**
 * @brief Checksum of the first seven bytes of an entry.
 * @param bytes  Entry bytes.
 * @return The checksum.
 */
static uint8_t entry_checksum(const uint8_t *bytes)
{
    uint8_t check = EVENTLOG_CHECK_SEED;
    uint8_t i;

    for (i = 0u; i < EVENTLOG_CHECK_COVERED; i++) {
        check ^= bytes[i];
    }
    return check;
}

/**
 * @brief Check that a code is one of the defined event codes.
 * @param code  Code to check.
 * @return true when the code is defined.
 */
static bool code_is_valid(uint8_t code)
{
    return (code >= (uint8_t)EVENTLOG_CODE_MIN) && (code <= (uint8_t)EVENTLOG_CODE_MAX);
}

/**
 * @brief Read the ring head, with a stored value out of range reported as 0.
 * @param head  Receives the head.
 * @return The EEPROM status.
 */
static gg_status_t read_head(uint8_t *head)
{
    gg_status_t status = hal_eeprom_read(EVENTLOG_ADDR_HEAD, head, 1u);

    if ((status == GG_OK) && (*head >= EVENTLOG_ENTRIES)) {
        *head = 0u;
    }
    return status;
}

/**
 * @brief Append one event at the ring head, then advance the head.
 *
 * The full contract is in the header.
 * @req REQ-LOG-001 REQ-LOG-002 REQ-LOG-003
 */
gg_status_t eventlog_append(const eventlog_entry_t *entry)
{
    gg_status_t status = GG_OK;
    uint8_t head = 0u;

    if (entry == NULL) {
        status = GG_ERR_NULL;
    } else if (!code_is_valid((uint8_t)entry->code)) {
        status = GG_ERR_RANGE;
    } else {
        status = read_head(&head);
    }
    if (status == GG_OK) {
        uint8_t bytes[EVENTLOG_ENTRY_SIZE];
        uint8_t i;
        uint16_t addr;

        for (i = 0u; i < EVENTLOG_UPTIME_BYTES; i++) {
            uint32_t shift = (uint32_t)i * EVENTLOG_UPTIME_SHIFT;

            bytes[EVENTLOG_OFS_UPTIME + i] = (uint8_t)((entry->uptime_s >> shift) & EVENTLOG_BYTE_MASK);
        }
        bytes[EVENTLOG_OFS_CODE] = (uint8_t)entry->code;
        bytes[EVENTLOG_OFS_ARG] = (uint8_t)(entry->arg & EVENTLOG_BYTE_MASK);
        bytes[EVENTLOG_OFS_ARG + 1u] = (uint8_t)(entry->arg >> EVENTLOG_BYTE_SHIFT);
        bytes[EVENTLOG_OFS_CHECK] = entry_checksum(bytes);
        addr = (uint16_t)(EVENTLOG_ADDR_ENTRIES + ((uint16_t)head * EVENTLOG_ENTRY_SIZE));
        status = hal_eeprom_write(addr, bytes, EVENTLOG_ENTRY_SIZE);
    }
    if (status == GG_OK) {
        head = (uint8_t)((head + 1u) % EVENTLOG_ENTRIES);
        status = hal_eeprom_write(EVENTLOG_ADDR_HEAD, &head, 1u);
    }
    return status;
}

/**
 * @brief Read the entry stored in one slot.
 *
 * The full contract is in the header.
 * @req REQ-LOG-002
 */
gg_status_t eventlog_read_slot(uint8_t slot, eventlog_entry_t *out)
{
    gg_status_t status = GG_OK;

    if (out == NULL) {
        status = GG_ERR_NULL;
    } else if (slot >= EVENTLOG_ENTRIES) {
        status = GG_ERR_RANGE;
    } else {
        uint8_t bytes[EVENTLOG_ENTRY_SIZE];
        uint16_t addr = (uint16_t)(EVENTLOG_ADDR_ENTRIES + ((uint16_t)slot * EVENTLOG_ENTRY_SIZE));

        status = hal_eeprom_read(addr, bytes, EVENTLOG_ENTRY_SIZE);
        if (status == GG_OK) {
            if ((bytes[EVENTLOG_OFS_CHECK] != entry_checksum(bytes)) || !code_is_valid(bytes[EVENTLOG_OFS_CODE])) {
                status = GG_ERR_CRC;
            } else {
                uint8_t i;

                out->uptime_s = 0u;
                for (i = 0u; i < EVENTLOG_UPTIME_BYTES; i++) {
                    uint32_t shift = (uint32_t)i * EVENTLOG_UPTIME_SHIFT;

                    out->uptime_s |= (uint32_t)bytes[EVENTLOG_OFS_UPTIME + i] << shift;
                }
                out->code = (eventlog_code_t)bytes[EVENTLOG_OFS_CODE];
                out->arg = (uint16_t)((uint16_t)bytes[EVENTLOG_OFS_ARG] |
                                      (uint16_t)((uint16_t)bytes[EVENTLOG_OFS_ARG + 1u] << EVENTLOG_BYTE_SHIFT));
            }
        }
    }
    return status;
}

/**
 * @brief Read the ring head.
 *
 * The full contract is in the header.
 * @req REQ-LOG-001
 */
gg_status_t eventlog_head(uint8_t *out_head)
{
    gg_status_t status = GG_OK;

    if (out_head == NULL) {
        status = GG_ERR_NULL;
    } else {
        status = read_head(out_head);
    }
    return status;
}
