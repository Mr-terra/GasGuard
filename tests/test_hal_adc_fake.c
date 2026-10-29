/* Unit tests for the host ADC fake, which stands in for hal/hal_adc.h on the PC. */
#include "unity.h"
#include "common/gg_types.h"
#include "hal/hal_adc.h"
#include "hal/host/hal_adc_fake.h"

#include <stdint.h>

/* Called by Unity around every test; prototypes keep -Wmissing-prototypes quiet. */
void setUp(void);
void tearDown(void);

void setUp(void)
{
    hal_adc_fake_reset();
}

void tearDown(void) {}

/* @verifies REQ-FUN-001 */
static void test_hal_adc_fake_returns_configured_value(void)
{
    uint16_t raw = 0u;

    hal_adc_fake_set_value(2u, 512u);

    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_read(2u, &raw));
    TEST_ASSERT_EQUAL_UINT16(512u, raw);
}

/* @verifies REQ-FUN-001 */
static void test_hal_adc_fake_unset_channel_reads_zero(void)
{
    uint16_t raw = 99u;

    TEST_ASSERT_EQUAL_INT(GG_OK, hal_adc_read(0u, &raw));
    TEST_ASSERT_EQUAL_UINT16(0u, raw);
}

/* @verifies REQ-FUN-001 */
static void test_hal_adc_read_rejects_null_output(void)
{
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, hal_adc_read(0u, NULL));
}

/* @verifies REQ-FUN-001 */
static void test_hal_adc_read_rejects_channel_out_of_range(void)
{
    uint16_t raw = 0u;

    TEST_ASSERT_EQUAL_INT(GG_ERR_RANGE, hal_adc_read(HAL_ADC_CHANNEL_COUNT, &raw));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_hal_adc_fake_returns_configured_value);
    RUN_TEST(test_hal_adc_fake_unset_channel_reads_zero);
    RUN_TEST(test_hal_adc_read_rejects_null_output);
    RUN_TEST(test_hal_adc_read_rejects_channel_out_of_range);
    return UNITY_END();
}
