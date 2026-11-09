/**
 * @file eventlog.h
 * @brief Event log: a ring of 32 entries in EEPROM, written only on events.
 *
 * Each entry holds an uptime timestamp in seconds, an event code, a 16-bit
 * argument and a CRC-16 (REQ-LOG-002). The ring head (the next slot
 * to write) is stored in EEPROM, so the log survives a reset. Entries are
 * written only when an event happens, never periodically (REQ-LOG-003), to
 * limit EEPROM wear.
 *
 * @req REQ-LOG-001 REQ-LOG-002 REQ-LOG-003
 */
#ifndef EVENTLOG_H
// DEV-02: include guard, not an unused macro
// cppcheck-suppress misra-c2012-2.5
#define EVENTLOG_H

#include <stdint.h>

#include "common/gg_types/gg_types.h"

/** @brief Number of entries in the ring (REQ-LOG-001). */
#define EVENTLOG_ENTRIES 32u
/** @brief Bytes per stored entry: 4 timestamp, 1 code, 2 argument, 2 CRC-16. */
#define EVENTLOG_ENTRY_SIZE 9u
/** @brief EEPROM address of the ring head (one byte), and of the first entry. */
#define EVENTLOG_ADDR_HEAD 20u
#define EVENTLOG_ADDR_ENTRIES 21u

/** @brief Event codes (REQ-LOG-001): mode transitions, faults, resets, configuration. */
typedef enum {
    EVENTLOG_MODE_CHANGE = 1,
    EVENTLOG_FAULT = 2,
    EVENTLOG_RESET = 3,
    EVENTLOG_CONFIG = 4,
    EVENTLOG_OVERRUN = 5   /**< A scheduler cycle took longer than 100 ms (REQ-SAF-008). */
} eventlog_code_t;

/** @brief One logged event. */
typedef struct {
    uint32_t uptime_s;     /**< Seconds since boot. */
    eventlog_code_t code;  /**< What happened. */
    uint16_t arg;          /**< Event-specific argument (for example the new mode). */
} eventlog_entry_t;

/**
 * @brief Append one event at the ring head, then advance the head.
 *
 * The event is passed as a structure so that its fields cannot be swapped by
 * mistake (the three fields have different roles, not different types).
 *
 * @param entry  Event to store: uptime_s, code (one of eventlog_code_t) and arg. Must not be NULL.
 * @return GG_OK, GG_ERR_NULL, GG_ERR_RANGE for an unknown code, or the EEPROM error.
 * @req REQ-LOG-001 REQ-LOG-002 REQ-LOG-003
 */
gg_status_t eventlog_append(const eventlog_entry_t *entry);

/**
 * @brief Read the entry stored in one slot.
 * @param slot  Slot index, 0 .. EVENTLOG_ENTRIES - 1.
 * @param out   Receives the entry. Must not be NULL.
 * @return GG_OK, GG_ERR_RANGE or GG_ERR_NULL, GG_ERR_CRC if the slot was never
 *         written or its checksum is wrong, or the EEPROM error.
 * @req REQ-LOG-002
 */
gg_status_t eventlog_read_slot(uint8_t slot, eventlog_entry_t *out);

/**
 * @brief Read the ring head: the slot that the next event will use.
 * @param out_head  Receives the head, 0 .. EVENTLOG_ENTRIES - 1. Must not be NULL.
 *                  A stored value out of range is reported as 0.
 * @return GG_OK, GG_ERR_NULL, or the EEPROM error.
 * @req REQ-LOG-001
 */
gg_status_t eventlog_head(uint8_t *out_head);

#endif /* EVENTLOG_H */
