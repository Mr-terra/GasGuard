/* Unit tests for the event log: a 32-entry ring in EEPROM, one write per event (REQ-LOG-001..003). */
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

    TEST_ASSERT_EQUAL_INT(GG_OK, eventlog_append(10u, EVENTLOG_MODE_CHANGE, 1u));
    TEST_ASSERT_EQUAL_INT(GG_OK, eventlog_head(&head));
    TEST_ASSERT_EQUAL_UINT8(1u, head);
    TEST_ASSERT_EQUAL_INT(GG_OK, eventlog_read_slot(0u, &entry));
    TEST_ASSERT_EQUAL_UINT32(10u, entry.uptime_s);
    TEST_ASSERT_EQUAL_INT(EVENTLOG_MODE_CHANGE, entry.code);
    TEST_ASSERT_EQUAL_UINT16(1u, entry.arg);
}

/* @verifies REQ-LOG-001 */
static void test_eventlog_ring_overwrites_the_oldest_entry_after_32_appends(void)
{
    eventlog_entry_t entry;
    uint8_t i;

    for (i = 0u; i < 33u; i++) {
        TEST_ASSERT_EQUAL_INT(GG_OK, eventlog_append((uint32_t)i, EVENTLOG_FAULT, 0u));
    }
    /* Append 32 went to slot 0, which held append 0: slot 0 now holds 32. */
    TEST_ASSERT_EQUAL_INT(GG_OK, eventlog_read_slot(0u, &entry));
    TEST_ASSERT_EQUAL_UINT32(32u, entry.uptime_s);
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

    TEST_ASSERT_EQUAL_INT(GG_OK, eventlog_append(7u, EVENTLOG_RESET, 3u));
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

    TEST_ASSERT_EQUAL_INT(GG_ERR_RANGE, eventlog_append(0u, (eventlog_code_t)0, 0u));
    TEST_ASSERT_EQUAL_INT(GG_ERR_RANGE, eventlog_append(0u, (eventlog_code_t)99, 0u));
    TEST_ASSERT_EQUAL_INT(GG_ERR_RANGE, eventlog_read_slot(EVENTLOG_ENTRIES, &entry));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, eventlog_read_slot(0u, NULL));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, eventlog_head(NULL));
}

// DEV-03: Unity runner entry point, called by the test harness; MISRA 8.6 does not apply.
// cppcheck-suppress misra-c2012-8.6
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_eventlog_head_starts_at_zero_after_reset);
    RUN_TEST(test_eventlog_append_stores_timestamp_code_and_argument);
    RUN_TEST(test_eventlog_ring_overwrites_the_oldest_entry_after_32_appends);
    RUN_TEST(test_eventlog_unwritten_slot_is_reported_invalid);
    RUN_TEST(test_eventlog_corrupted_entry_is_detected_by_its_checksum);
    RUN_TEST(test_eventlog_head_out_of_range_is_treated_as_zero);
    RUN_TEST(test_eventlog_rejects_unknown_codes_and_bad_arguments);
    return UNITY_END();
}
