/* Unit tests for the host I2C fake, which stands in for hal/hal_i2c.h on the PC. */
#include "unity.h"
#include "common/gg_types.h"
#include "hal/hal_i2c.h"
#include "hal/host/hal_i2c_fake.h"

#include <stdbool.h>
#include <stdint.h>

/* DEV-03: Unity hooks, called by the framework; MISRA 8.6 and unusedFunction do not apply. */
// cppcheck-suppress misra-c2012-8.6
void setUp(void);
// cppcheck-suppress misra-c2012-8.6
void tearDown(void);

// cppcheck-suppress unusedFunction
void setUp(void)
{
    hal_i2c_fake_reset();
}

// cppcheck-suppress unusedFunction
void tearDown(void) {}

/* @verifies REQ-HMI-001 */
static void test_hal_i2c_init_succeeds(void)
{
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_i2c_init());
}

/* @verifies REQ-HMI-001 */
static void test_hal_i2c_write_to_absent_device_times_out(void)
{
    const uint8_t cmd[1] = {0x80u};

    TEST_ASSERT_EQUAL_INT(GG_ERR_TIMEOUT, hal_i2c_write(0x3Eu, cmd, 1u));
}

/* @verifies REQ-HMI-002 */
static void test_hal_i2c_write_to_present_device_is_recorded(void)
{
    const uint8_t data[2] = {0x80u, 0x28u};

    TEST_ASSERT_EQUAL_INT(GG_OK, hal_i2c_fake_set_present(0x62u, true));
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_i2c_write(0x62u, data, 2u));
    TEST_ASSERT_EQUAL_UINT8(0x62u, hal_i2c_fake_last_addr());
    TEST_ASSERT_EQUAL_UINT8(2u, hal_i2c_fake_last_len());
    TEST_ASSERT_EQUAL_UINT8(0x80u, hal_i2c_fake_last_first_byte());
}

/* @verifies REQ-HMI-001 */
static void test_hal_i2c_rejects_null_and_bad_address(void)
{
    const uint8_t cmd[1] = {0u};

    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, hal_i2c_write(0x3Eu, NULL, 1u));
    TEST_ASSERT_EQUAL_INT(GG_ERR_RANGE, hal_i2c_write(0x80u, cmd, 1u));
    TEST_ASSERT_EQUAL_INT(GG_ERR_RANGE, hal_i2c_fake_set_present(0x80u, true));
}

// cppcheck-suppress misra-c2012-8.6
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_hal_i2c_init_succeeds);
    RUN_TEST(test_hal_i2c_write_to_absent_device_times_out);
    RUN_TEST(test_hal_i2c_write_to_present_device_is_recorded);
    RUN_TEST(test_hal_i2c_rejects_null_and_bad_address);
    return UNITY_END();
}
