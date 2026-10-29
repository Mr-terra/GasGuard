/* Unit tests for the host EEPROM fake, which stands in for hal/hal_eeprom.h on the PC. */
#include "unity.h"
#include "common/gg_types.h"
#include "hal/hal_eeprom.h"
#include "hal/host/hal_eeprom_fake.h"

#include <stdint.h>

/* DEV-03: Unity hooks, called by the framework; MISRA 8.6 and unusedFunction do not apply. */
// cppcheck-suppress misra-c2012-8.6
void setUp(void);
// cppcheck-suppress misra-c2012-8.6
void tearDown(void);

// cppcheck-suppress unusedFunction
void setUp(void)
{
    hal_eeprom_fake_reset();
}

// cppcheck-suppress unusedFunction
void tearDown(void) {}

/* @verifies REQ-CFG-001 */
static void test_hal_eeprom_fresh_cells_read_erased_value(void)
{
    uint8_t data[2] = {0u, 0u};

    TEST_ASSERT_EQUAL_INT(GG_OK, hal_eeprom_read(0u, data, 2u));
    TEST_ASSERT_EQUAL_HEX8(0xFFu, data[0]);
    TEST_ASSERT_EQUAL_HEX8(0xFFu, data[1]);
}

/* @verifies REQ-LOG-001 */
static void test_hal_eeprom_write_then_read_round_trips(void)
{
    const uint8_t out[4] = {0x12u, 0x34u, 0x56u, 0x78u};
    uint8_t in[4] = {0u, 0u, 0u, 0u};

    TEST_ASSERT_EQUAL_INT(GG_OK, hal_eeprom_write(0x040u, out, 4u));
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_eeprom_read(0x040u, in, 4u));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(out, in, 4u);
}

/* @verifies REQ-CFG-001 */
static void test_hal_eeprom_rejects_block_past_end(void)
{
    uint8_t data[8] = {0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};

    TEST_ASSERT_EQUAL_INT(GG_ERR_RANGE, hal_eeprom_read(HAL_EEPROM_SIZE - 4u, data, 8u));
    TEST_ASSERT_EQUAL_INT(GG_ERR_RANGE, hal_eeprom_write(HAL_EEPROM_SIZE, data, 1u));
}

/* @verifies REQ-LOG-001 */
static void test_hal_eeprom_rejects_null_buffer(void)
{
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, hal_eeprom_read(0u, NULL, 1u));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, hal_eeprom_write(0u, NULL, 1u));
}

// cppcheck-suppress misra-c2012-8.6
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_hal_eeprom_fresh_cells_read_erased_value);
    RUN_TEST(test_hal_eeprom_write_then_read_round_trips);
    RUN_TEST(test_hal_eeprom_rejects_block_past_end);
    RUN_TEST(test_hal_eeprom_rejects_null_buffer);
    return UNITY_END();
}
