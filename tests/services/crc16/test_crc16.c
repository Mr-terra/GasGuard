/* Unit tests for the CRC-16/CCITT-FALSE used to protect the configuration copies (REQ-CFG-001). */
#include "unity.h"
#include "common/gg_types/gg_types.h"
#include "services/crc16/crc16.h"

#include <stdint.h>

/* DEV-03: Unity hooks, called by the framework; MISRA 8.6 and unusedFunction do not apply. */
// cppcheck-suppress misra-c2012-8.6
void setUp(void);
// cppcheck-suppress misra-c2012-8.6
void tearDown(void);

void setUp(void) {}

void tearDown(void) {}

/* @verifies REQ-CFG-001 */
static void test_crc16_check_vector_123456789(void)
{
    static const uint8_t data[] = { '1', '2', '3', '4', '5', '6', '7', '8', '9' };
    uint16_t crc = 0u;

    TEST_ASSERT_EQUAL_INT(GG_OK, crc16_compute(data, (uint16_t)sizeof(data), &crc));
    TEST_ASSERT_EQUAL_HEX16(0x29B1u, crc);
}

/* @verifies REQ-CFG-001 */
static void test_crc16_empty_input_gives_initial_value(void)
{
    uint16_t crc = 0u;

    TEST_ASSERT_EQUAL_INT(GG_OK, crc16_compute(NULL, 0u, &crc));
    TEST_ASSERT_EQUAL_HEX16(0xFFFFu, crc);
}

/* @verifies REQ-CFG-001 */
static void test_crc16_single_changed_byte_changes_the_crc(void)
{
    static const uint8_t first[] = { 0x10u, 0x20u, 0x30u };
    static const uint8_t second[] = { 0x10u, 0x21u, 0x30u };
    uint16_t crc_first = 0u;
    uint16_t crc_second = 0u;

    TEST_ASSERT_EQUAL_INT(GG_OK, crc16_compute(first, 3u, &crc_first));
    TEST_ASSERT_EQUAL_INT(GG_OK, crc16_compute(second, 3u, &crc_second));
    TEST_ASSERT_NOT_EQUAL(crc_first, crc_second);
}

/* @verifies REQ-CFG-001 */
static void test_crc16_rejects_null_output(void)
{
    static const uint8_t data[] = { 0x00u };

    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, crc16_compute(data, 1u, NULL));
}

/* @verifies REQ-CFG-001 */
static void test_crc16_rejects_null_data_with_nonzero_length(void)
{
    uint16_t crc = 0u;

    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, crc16_compute(NULL, 1u, &crc));
}

/* @verifies REQ-CFG-001 */
static void test_crc16_same_data_gives_the_same_crc(void)
{
    static const uint8_t data[] = { 0x64u, 0x00u, 0xFAu, 0x00u, 0x14u, 0x00u, 0x78u, 0x00u };
    uint16_t first = 0u;
    uint16_t second = 1u;

    /* A good copy must always check good: no false corruption on a healthy EEPROM. */
    TEST_ASSERT_EQUAL_INT(GG_OK, crc16_compute(data, (uint16_t)sizeof(data), &first));
    TEST_ASSERT_EQUAL_INT(GG_OK, crc16_compute(data, (uint16_t)sizeof(data), &second));
    TEST_ASSERT_EQUAL_HEX16(first, second);
}

/* @verifies REQ-CFG-001 */
static void test_crc16_every_single_bit_error_is_detected(void)
{
    uint8_t data[8] = { 0x64u, 0x00u, 0xFAu, 0x00u, 0x14u, 0x00u, 0x78u, 0x00u };
    uint16_t good = 0u;
    uint16_t bad = 0u;
    uint8_t byte;
    uint8_t bit;

    TEST_ASSERT_EQUAL_INT(GG_OK, crc16_compute(data, 8u, &good));
    for (byte = 0u; byte < 8u; byte++) {
        for (bit = 0u; bit < 8u; bit++) {
            data[byte] = (uint8_t)(data[byte] ^ (uint8_t)(1u << bit));
            TEST_ASSERT_EQUAL_INT(GG_OK, crc16_compute(data, 8u, &bad));
            TEST_ASSERT_NOT_EQUAL(good, bad);
            data[byte] = (uint8_t)(data[byte] ^ (uint8_t)(1u << bit));
        }
    }
}

/* @verifies REQ-CFG-001 */
static void test_crc16_erased_eeprom_does_not_check_good(void)
{
    static const uint8_t erased[8] = { 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu };
    uint16_t crc = 0u;

    /* A never-written copy reads 0xFF everywhere, its CRC bytes too (0xFFFF):
     * the CRC of the eight 0xFF bytes must differ, or blank cells would pass. */
    TEST_ASSERT_EQUAL_INT(GG_OK, crc16_compute(erased, 8u, &crc));
    TEST_ASSERT_NOT_EQUAL(0xFFFFu, crc);
}

/* @verifies REQ-CFG-001 */
static void test_crc16_zero_length_ignores_the_data(void)
{
    static const uint8_t data[] = { 0x55u };
    uint16_t crc = 0u;

    TEST_ASSERT_EQUAL_INT(GG_OK, crc16_compute(data, 0u, &crc));
    TEST_ASSERT_EQUAL_HEX16(0xFFFFu, crc);
}

// DEV-03: Unity runner entry point, called by the test harness; MISRA 8.6 does not apply.
// cppcheck-suppress misra-c2012-8.6
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_crc16_check_vector_123456789);
    RUN_TEST(test_crc16_empty_input_gives_initial_value);
    RUN_TEST(test_crc16_single_changed_byte_changes_the_crc);
    RUN_TEST(test_crc16_rejects_null_output);
    RUN_TEST(test_crc16_rejects_null_data_with_nonzero_length);
    RUN_TEST(test_crc16_same_data_gives_the_same_crc);
    RUN_TEST(test_crc16_every_single_bit_error_is_detected);
    RUN_TEST(test_crc16_erased_eeprom_does_not_check_good);
    RUN_TEST(test_crc16_zero_length_ignores_the_data);
    return UNITY_END();
}
