/**
 * @file eventlog.c
 * @brief Event log ring in EEPROM.
 *
 * How it works
 * ------------
 * Each event is written in the slot named by the head, then the head moves on
 * by one, wrapping after 32 slots, so the newest event overwrites the oldest.
 * An entry is 9 bytes: the uptime (4 bytes, little-endian), the code, the
 * argument (2 bytes) and a CRC-16/CCITT-FALSE of the first seven bytes (2 bytes).
 * A CRC detects every single and double bit error in an entry, which an XOR
 * checksum does not. A slot that was never written reads as 0xFF, and a zeroed
 * slot has code 0: both fail the code check, so they are not read back as events.
 * Writing only on events keeps the wear of the EEPROM low (REQ-LOG-003).
 *
 * @req REQ-LOG-001 REQ-LOG-002 REQ-LOG-003
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "common/gg_types/gg_types.h"
#include "hal/eeprom/eeprom.h"
#include "services/crc16/crc16.h"
#include "services/eventlog/eventlog.h"

/** @brief Byte offsets inside one entry. */
#define EVENTLOG_OFS_UPTIME   0u
#define EVENTLOG_OFS_CODE     4u
#define EVENTLOG_OFS_ARG      5u
#define EVENTLOG_OFS_CHECK    7u
/** @brief Bytes covered by the CRC: everything before it. */
#define EVENTLOG_CHECK_COVERED EVENTLOG_OFS_CHECK

/** @brief Byte helpers for the little-endian fields. */
#define EVENTLOG_BYTE_MASK  0xFFu
#define EVENTLOG_BYTE_SHIFT 8u
#define EVENTLOG_UPTIME_SHIFT 8u
/** @brief Bytes of the uptime field. */
#define EVENTLOG_UPTIME_BYTES 4u

/** @brief Smallest and largest event code. */
#define EVENTLOG_CODE_MIN EVENTLOG_MODE_CHANGE
#define EVENTLOG_CODE_MAX EVENTLOG_OVERRUN

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
 * @brief CRC-16 of the first seven bytes of an entry.
 * @param bytes  Entry bytes, at least EVENTLOG_ENTRY_SIZE long.
 * @param crc    Receives the CRC.
 * @return GG_OK or the CRC error.
 */
static gg_status_t entry_crc(const uint8_t *bytes, uint16_t *crc)
{
    return crc16_compute(bytes, EVENTLOG_CHECK_COVERED, crc);
}

/**
 * @brief Check the CRC and the code of an entry read from EEPROM.
 * @param bytes  Entry bytes, EVENTLOG_ENTRY_SIZE long.
 * @return true when the entry is intact.
 */
static bool entry_is_intact(const uint8_t *bytes)
{
    uint16_t crc = 0u;
    bool intact = false;

    if (entry_crc(bytes, &crc) == GG_OK) {
        uint16_t stored = (uint16_t)((uint16_t)bytes[EVENTLOG_OFS_CHECK] |
                                     (uint16_t)((uint16_t)bytes[EVENTLOG_OFS_CHECK + 1u] << EVENTLOG_BYTE_SHIFT));
        intact = (stored == crc) && code_is_valid(bytes[EVENTLOG_OFS_CODE]);
    }
    return intact;
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
        uint16_t crc = 0u;

        for (i = 0u; i < EVENTLOG_UPTIME_BYTES; i++) {
            uint32_t shift = (uint32_t)i * EVENTLOG_UPTIME_SHIFT;

            bytes[EVENTLOG_OFS_UPTIME + i] = (uint8_t)((entry->uptime_s >> shift) & EVENTLOG_BYTE_MASK);
        }
        bytes[EVENTLOG_OFS_CODE] = (uint8_t)entry->code;
        bytes[EVENTLOG_OFS_ARG] = (uint8_t)(entry->arg & EVENTLOG_BYTE_MASK);
        bytes[EVENTLOG_OFS_ARG + 1u] = (uint8_t)(entry->arg >> EVENTLOG_BYTE_SHIFT);
        status = entry_crc(bytes, &crc);
        bytes[EVENTLOG_OFS_CHECK] = (uint8_t)(crc & EVENTLOG_BYTE_MASK);
        bytes[EVENTLOG_OFS_CHECK + 1u] = (uint8_t)(crc >> EVENTLOG_BYTE_SHIFT);
        addr = (uint16_t)(EVENTLOG_ADDR_ENTRIES + ((uint16_t)head * EVENTLOG_ENTRY_SIZE));
        if (status == GG_OK) {
            status = hal_eeprom_write(addr, bytes, EVENTLOG_ENTRY_SIZE);
        }
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
            if (!entry_is_intact(bytes)) {
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
