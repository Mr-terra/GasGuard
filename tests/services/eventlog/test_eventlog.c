/* Unit tests for the event log: a 111-entry ring in EEPROM, one write per event (REQ-LOG-001..003). */
#include "unity.h"
#include "common/gg_types/gg_types.h"
#include "hal/eeprom/eeprom.h"
#include "hal/eeprom/eeprom_fake.h"
#include "services/eventlog/eventlog.h"

#include <stdint.h>

/* DEV-03: Unity hooks, called by the framework; MISRA 8.6 and unusedFunction do not apply. */
// cppcheck-suppress misra-c2012-8.6
void setUp(void);
// cppcheck-suppress misra-c2012-8.6
void tearDown(void);

void setUp(void)
{
    hal_eeprom_fake_reset();
}

void tearDown(void) {}

/** @brief Build an event and append it, as the application will do. */
static gg_status_t eventlog_test_append(uint32_t uptime_s, eventlog_code_t code, uint16_t arg)
{
    eventlog_entry_t entry;

    entry.uptime_s = uptime_s;
    entry.code = code;
    entry.arg = arg;
    return eventlog_append(&entry);
}

/* @verifies REQ-LOG-001 */
static void test_eventlog_head_starts_at_zero_after_reset(void)
{
    uint8_t head = 99u;

    TEST_ASSERT_EQUAL_INT(GG_OK, eventlog_head(&head));
    TEST_ASSERT_EQUAL_UINT8(0u, head);
}

/* @verifies REQ-LOG-002 */
static void test_eventlog_append_stores_timestamp_code_and_argument(void)
{
    eventlog_entry_t entry;
    uint8_t head = 0u;

    TEST_ASSERT_EQUAL_INT(GG_OK, eventlog_test_append(10u, EVENTLOG_MODE_CHANGE, 1u));
    TEST_ASSERT_EQUAL_INT(GG_OK, eventlog_head(&head));
    TEST_ASSERT_EQUAL_UINT8(1u, head);
    TEST_ASSERT_EQUAL_INT(GG_OK, eventlog_read_slot(0u, &entry));
    TEST_ASSERT_EQUAL_UINT32(10u, entry.uptime_s);
    TEST_ASSERT_EQUAL_INT(EVENTLOG_MODE_CHANGE, entry.code);
    TEST_ASSERT_EQUAL_UINT16(1u, entry.arg);
}

/* @verifies REQ-LOG-001 */
static void test_eventlog_ring_overwrites_the_oldest_entry_after_111_appends(void)
{
    eventlog_entry_t entry;
    uint8_t i;

    /* The ring holds 111 entries: the largest that fits in the 1 KB EEPROM (REQ-LOG-001). */
    TEST_ASSERT_EQUAL_UINT32(111u, EVENTLOG_ENTRIES);
    for (i = 0u; i < 112u; i++) {
        TEST_ASSERT_EQUAL_INT(GG_OK, eventlog_test_append((uint32_t)i, EVENTLOG_FAULT, 0u));
    }
    /* Append 111 went to slot 0, which held append 0: slot 0 now holds 111. */
    TEST_ASSERT_EQUAL_INT(GG_OK, eventlog_read_slot(0u, &entry));
    TEST_ASSERT_EQUAL_UINT32(111u, entry.uptime_s);
    /* Slot 1 still holds append 1. */
    TEST_ASSERT_EQUAL_INT(GG_OK, eventlog_read_slot(1u, &entry));
    TEST_ASSERT_EQUAL_UINT32(1u, entry.uptime_s);
}

/* @verifies REQ-LOG-002 */
static void test_eventlog_unwritten_slot_is_reported_invalid(void)
{
    eventlog_entry_t entry;

    TEST_ASSERT_EQUAL_INT(GG_ERR_CRC, eventlog_read_slot(5u, &entry));
}

/* @verifies REQ-LOG-002 */
static void test_eventlog_corrupted_entry_is_detected_by_its_checksum(void)
{
    eventlog_entry_t entry;
    uint8_t byte = 0u;
    uint16_t addr = EVENTLOG_ADDR_ENTRIES;

    TEST_ASSERT_EQUAL_INT(GG_OK, eventlog_test_append(7u, EVENTLOG_RESET, 3u));
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_eeprom_read(addr, &byte, 1u));
    byte = (uint8_t)(byte ^ 0x08u);
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_eeprom_write(addr, &byte, 1u));

    TEST_ASSERT_EQUAL_INT(GG_ERR_CRC, eventlog_read_slot(0u, &entry));
}

/* @verifies REQ-LOG-002 */
static void test_eventlog_head_out_of_range_is_treated_as_zero(void)
{
    uint8_t head = 0u;
    uint8_t bad = 0xFFu;

    TEST_ASSERT_EQUAL_INT(GG_OK, hal_eeprom_write(EVENTLOG_ADDR_HEAD, &bad, 1u));
    TEST_ASSERT_EQUAL_INT(GG_OK, eventlog_head(&head));
    TEST_ASSERT_EQUAL_UINT8(0u, head);
}

/* @verifies REQ-LOG-001 */
static void test_eventlog_rejects_unknown_codes_and_bad_arguments(void)
{
    eventlog_entry_t entry;

    TEST_ASSERT_EQUAL_INT(GG_ERR_RANGE, eventlog_test_append(0u, (eventlog_code_t)0, 0u));
    TEST_ASSERT_EQUAL_INT(GG_ERR_RANGE, eventlog_test_append(0u, (eventlog_code_t)99, 0u));
    TEST_ASSERT_EQUAL_INT(GG_ERR_RANGE, eventlog_test_append(0u, (eventlog_code_t)11, 0u));
    TEST_ASSERT_EQUAL_INT(GG_OK, eventlog_test_append(0u, EVENTLOG_MAINT_PEAK, 0u));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, eventlog_append(NULL));
    TEST_ASSERT_EQUAL_INT(GG_ERR_RANGE, eventlog_read_slot(EVENTLOG_ENTRIES, &entry));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, eventlog_read_slot(0u, NULL));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, eventlog_head(NULL));
}

/* @verifies REQ-LOG-003 */
static void test_eventlog_writes_only_when_an_event_is_appended(void)
{
    eventlog_entry_t entry;
    uint8_t head = 0u;
    uint32_t before = hal_eeprom_fake_write_count();

    TEST_ASSERT_EQUAL_INT(GG_OK, eventlog_head(&head));
    TEST_ASSERT_EQUAL_INT(GG_ERR_CRC, eventlog_read_slot(0u, &entry));
    TEST_ASSERT_EQUAL_UINT32(before, hal_eeprom_fake_write_count());
    TEST_ASSERT_EQUAL_INT(GG_OK, eventlog_test_append(1u, EVENTLOG_FAULT, 0u));
    /* One append = one entry write and one head write, and nothing else. */
    TEST_ASSERT_EQUAL_UINT32(before + 2u, hal_eeprom_fake_write_count());
}

/** @brief Offset of the CRC high byte in the first entry: the entry is 9 bytes, the CRC is the last two. */
#define EVENTLOG_TEST_CRC_HIGH_OFFSET 8u

/* @verifies REQ-LOG-002 */
static void test_eventlog_corrupted_crc_bytes_are_detected(void)
{
    eventlog_entry_t entry;
    uint8_t byte = 0u;
    uint16_t addr = EVENTLOG_ADDR_ENTRIES;

    /* The last byte of the first entry is the high byte of the CRC. */
    addr = (uint16_t)(addr + EVENTLOG_TEST_CRC_HIGH_OFFSET);

    TEST_ASSERT_EQUAL_INT(GG_OK, eventlog_test_append(7u, EVENTLOG_RESET, 3u));
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_eeprom_read(addr, &byte, 1u));
    byte = (uint8_t)(byte ^ 0x01u);
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_eeprom_write(addr, &byte, 1u));

    TEST_ASSERT_EQUAL_INT(GG_ERR_CRC, eventlog_read_slot(0u, &entry));
}

/* @verifies REQ-LOG-001 */
static void test_eventlog_head_wraps_to_zero_on_the_111th_append(void)
{
    uint8_t head = 0u;
    uint8_t i;

    for (i = 0u; i < (EVENTLOG_ENTRIES - 1u); i++) {
        TEST_ASSERT_EQUAL_INT(GG_OK, eventlog_test_append((uint32_t)i, EVENTLOG_FAULT, 0u));
    }
    TEST_ASSERT_EQUAL_INT(GG_OK, eventlog_head(&head));
    TEST_ASSERT_EQUAL_UINT8(EVENTLOG_ENTRIES - 1u, head);
    TEST_ASSERT_EQUAL_INT(GG_OK, eventlog_test_append(110u, EVENTLOG_FAULT, 0u));
    TEST_ASSERT_EQUAL_INT(GG_OK, eventlog_head(&head));
    TEST_ASSERT_EQUAL_UINT8(0u, head);
}

/* @verifies REQ-LOG-001 */
static void test_eventlog_first_and_last_codes_are_accepted(void)
{
    const int past_last = (int)EVENTLOG_CODE_LAST + 1;

    TEST_ASSERT_EQUAL_INT(GG_OK, eventlog_test_append(0u, EVENTLOG_MODE_CHANGE, 0u));
    TEST_ASSERT_EQUAL_INT(GG_OK, eventlog_test_append(0u, EVENTLOG_CODE_LAST, 0u));
    TEST_ASSERT_EQUAL_INT(GG_ERR_RANGE, eventlog_test_append(0u, (eventlog_code_t)past_last, 0u));
}

/* @verifies REQ-LOG-001 */
static void test_eventlog_refused_entry_writes_nothing(void)
{
    uint8_t head = 0u;
    uint32_t before = hal_eeprom_fake_write_count();

    TEST_ASSERT_EQUAL_INT(GG_ERR_RANGE, eventlog_test_append(0u, (eventlog_code_t)0, 0u));
    TEST_ASSERT_EQUAL_UINT32(before, hal_eeprom_fake_write_count());
    TEST_ASSERT_EQUAL_INT(GG_OK, eventlog_head(&head));
    TEST_ASSERT_EQUAL_UINT8(0u, head);
}

/* @verifies REQ-LOG-001 */
static void test_eventlog_failed_eeprom_write_is_reported_not_hidden(void)
{
    hal_eeprom_fake_fail_writes(true);
    TEST_ASSERT_NOT_EQUAL(GG_OK, eventlog_test_append(1u, EVENTLOG_FAULT, 0u));
    hal_eeprom_fake_fail_writes(false);
}

/* @verifies REQ-LOG-002 */
static void test_eventlog_largest_uptime_and_argument_round_trip(void)
{
    eventlog_entry_t entry;

    TEST_ASSERT_EQUAL_INT(GG_OK, eventlog_test_append(UINT32_MAX, EVENTLOG_RESET, UINT16_MAX));
    TEST_ASSERT_EQUAL_INT(GG_OK, eventlog_read_slot(0u, &entry));
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, entry.uptime_s);
    TEST_ASSERT_EQUAL_INT(EVENTLOG_RESET, entry.code);
    TEST_ASSERT_EQUAL_UINT16(UINT16_MAX, entry.arg);
}

/* @verifies REQ-LOG-002 */
static void test_eventlog_zero_uptime_and_argument_are_a_valid_entry(void)
{
    eventlog_entry_t entry;

    /* An entry written at power-up with argument 0 is genuine: it must read back
     * as valid, not be taken for a blank or damaged slot. */
    TEST_ASSERT_EQUAL_INT(GG_OK, eventlog_test_append(0u, EVENTLOG_MODE_CHANGE, 0u));
    TEST_ASSERT_EQUAL_INT(GG_OK, eventlog_read_slot(0u, &entry));
    TEST_ASSERT_EQUAL_UINT32(0u, entry.uptime_s);
    TEST_ASSERT_EQUAL_UINT16(0u, entry.arg);
}

/* @verifies REQ-LOG-002 */
static void test_eventlog_head_at_its_last_valid_value_is_kept(void)
{
    uint8_t head = 0u;
    uint8_t last = EVENTLOG_ENTRIES - 1u;
    uint8_t over = EVENTLOG_ENTRIES;

    TEST_ASSERT_EQUAL_INT(GG_OK, hal_eeprom_write(EVENTLOG_ADDR_HEAD, &last, 1u));
    TEST_ASSERT_EQUAL_INT(GG_OK, eventlog_head(&head));
    TEST_ASSERT_EQUAL_UINT8(EVENTLOG_ENTRIES - 1u, head);
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_eeprom_write(EVENTLOG_ADDR_HEAD, &over, 1u));
    TEST_ASSERT_EQUAL_INT(GG_OK, eventlog_head(&head));
    TEST_ASSERT_EQUAL_UINT8(0u, head);
}

/* @verifies REQ-LOG-002 */
static void test_eventlog_last_slot_is_readable(void)
{
    eventlog_entry_t entry;
    uint8_t i;

    for (i = 0u; i < EVENTLOG_ENTRIES; i++) {
        TEST_ASSERT_EQUAL_INT(GG_OK, eventlog_test_append((uint32_t)i, EVENTLOG_FAULT, (uint16_t)i));
    }
    TEST_ASSERT_EQUAL_INT(GG_OK, eventlog_read_slot(EVENTLOG_ENTRIES - 1u, &entry));
    TEST_ASSERT_EQUAL_UINT16(EVENTLOG_ENTRIES - 1u, entry.arg);
}

/* @verifies REQ-LOG-003 */
static void test_eventlog_append_at_the_ring_end_still_writes_twice(void)
{
    uint32_t before = 0u;
    uint8_t i;

    for (i = 0u; i < (EVENTLOG_ENTRIES - 1u); i++) {
        TEST_ASSERT_EQUAL_INT(GG_OK, eventlog_test_append((uint32_t)i, EVENTLOG_FAULT, 0u));
    }
    before = hal_eeprom_fake_write_count();
    TEST_ASSERT_EQUAL_INT(GG_OK, eventlog_test_append(110u, EVENTLOG_FAULT, 0u));
    TEST_ASSERT_EQUAL_UINT32(before + 2u, hal_eeprom_fake_write_count());
}

// DEV-03: Unity runner entry point, called by the test harness; MISRA 8.6 does not apply.
// cppcheck-suppress misra-c2012-8.6
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_eventlog_head_starts_at_zero_after_reset);
    RUN_TEST(test_eventlog_append_stores_timestamp_code_and_argument);
    RUN_TEST(test_eventlog_ring_overwrites_the_oldest_entry_after_111_appends);
    RUN_TEST(test_eventlog_unwritten_slot_is_reported_invalid);
    RUN_TEST(test_eventlog_corrupted_entry_is_detected_by_its_checksum);
    RUN_TEST(test_eventlog_head_out_of_range_is_treated_as_zero);
    RUN_TEST(test_eventlog_rejects_unknown_codes_and_bad_arguments);
    RUN_TEST(test_eventlog_writes_only_when_an_event_is_appended);
    RUN_TEST(test_eventlog_corrupted_crc_bytes_are_detected);
    RUN_TEST(test_eventlog_head_wraps_to_zero_on_the_111th_append);
    RUN_TEST(test_eventlog_first_and_last_codes_are_accepted);
    RUN_TEST(test_eventlog_refused_entry_writes_nothing);
    RUN_TEST(test_eventlog_failed_eeprom_write_is_reported_not_hidden);
    RUN_TEST(test_eventlog_largest_uptime_and_argument_round_trip);
    RUN_TEST(test_eventlog_zero_uptime_and_argument_are_a_valid_entry);
    RUN_TEST(test_eventlog_head_at_its_last_valid_value_is_kept);
    RUN_TEST(test_eventlog_last_slot_is_readable);
    RUN_TEST(test_eventlog_append_at_the_ring_end_still_writes_twice);
    return UNITY_END();
}
