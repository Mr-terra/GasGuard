/* Unit tests for services/calib: raw ADC -> 0.1 %LEL, and temperature compensation. */
#include "unity.h"
#include "common/gg_types.h"
#include "hal/hal_adc.h"
#include "services/calib.h"

/* DEV-03: Unity hooks, called by the framework; MISRA 8.6 and unusedFunction do not apply. */
// cppcheck-suppress misra-c2012-8.6
void setUp(void);
// cppcheck-suppress misra-c2012-8.6
void tearDown(void);

/* Zero at raw 100, gain of 1.0 %LEL per LSB (10 units per LSB, Q8 = 2560). */
static calib_t s_calib;

// cppcheck-suppress unusedFunction
void setUp(void)
{
    TEST_ASSERT_EQUAL_INT(GG_OK, calib_init(&s_calib, 100u, 10u * CALIB_GAIN_ONE));
}

// cppcheck-suppress unusedFunction
void tearDown(void) {}

/* @verifies REQ-FUN-003 */
static void test_calib_init_rejects_null(void)
{
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, calib_init(NULL, 100u, CALIB_GAIN_ONE));
}

/* @verifies REQ-FUN-003 */
static void test_calib_init_rejects_zero_gain(void)
{
    TEST_ASSERT_EQUAL_INT(GG_ERR_RANGE, calib_init(&s_calib, 100u, 0u));
}

/* @verifies REQ-FUN-003 */
static void test_calib_zero_reading_is_zero_concentration(void)
{
    gg_conc_t conc = 99;

    TEST_ASSERT_EQUAL_INT(GG_OK, calib_to_lel(&s_calib, 100u, &conc));
    TEST_ASSERT_EQUAL_INT16(0, conc);
}

/* @verifies REQ-FUN-003 */
static void test_calib_is_linear_above_zero(void)
{
    gg_conc_t conc = 0;

    /* 150 raw is 50 LSB above zero, at 10 units per LSB: 500 units = 50 %LEL. */
    TEST_ASSERT_EQUAL_INT(GG_OK, calib_to_lel(&s_calib, 150u, &conc));
    TEST_ASSERT_EQUAL_INT16(500, conc);
}

/* @verifies REQ-FUN-003 */
static void test_calib_below_zero_reads_zero_not_negative(void)
{
    gg_conc_t conc = 99;

    /* A reading under the zero offset is sensor drift, not negative gas. */
    TEST_ASSERT_EQUAL_INT(GG_OK, calib_to_lel(&s_calib, 40u, &conc));
    TEST_ASSERT_EQUAL_INT16(0, conc);
}

/* @verifies REQ-FUN-003 */
static void test_calib_rejects_out_of_range_raw(void)
{
    gg_conc_t conc = 0;

    TEST_ASSERT_EQUAL_INT(GG_ERR_RANGE, calib_to_lel(&s_calib, HAL_ADC_RAW_MAX + 1u, &conc));
}

/* @verifies REQ-FUN-003 */
static void test_calib_to_lel_rejects_null(void)
{
    gg_conc_t conc = 0;

    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, calib_to_lel(NULL, 150u, &conc));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, calib_to_lel(&s_calib, 150u, NULL));
}

/* @verifies REQ-FUN-004 */
static void test_calib_temperature_correction_applied_when_valid(void)
{
    uint16_t corrected = 0u;

    /* Reference 500, temperature reads 520 (+20 LSB), coefficient -0.5 LSB per LSB:
     * the correction is -10 LSB, so the raw value goes from 300 to 290. */
    TEST_ASSERT_EQUAL_INT(GG_OK, calib_set_temp_compensation(&s_calib, 500u, -CALIB_GAIN_ONE / 2));
    TEST_ASSERT_EQUAL_INT(GG_OK, calib_temp_correct(&s_calib, 300u, 520u, true, &corrected));
    TEST_ASSERT_EQUAL_UINT16(290u, corrected);
}

/* @verifies REQ-FUN-004 */
static void test_calib_temperature_ignored_when_sensor_invalid(void)
{
    uint16_t corrected = 0u;

    TEST_ASSERT_EQUAL_INT(GG_OK, calib_set_temp_compensation(&s_calib, 500u, -CALIB_GAIN_ONE / 2));
    TEST_ASSERT_EQUAL_INT(GG_OK, calib_temp_correct(&s_calib, 300u, 900u, false, &corrected));
    TEST_ASSERT_EQUAL_UINT16(300u, corrected);
}

/* @verifies REQ-FUN-004 */
static void test_calib_temperature_correction_is_clamped_to_adc_range(void)
{
    uint16_t corrected = 0u;

    /* Coefficient +1 LSB per LSB, temperature 1000 above reference: 1000 + 1000 saturates. */
    TEST_ASSERT_EQUAL_INT(GG_OK, calib_set_temp_compensation(&s_calib, 0u, CALIB_GAIN_ONE));
    TEST_ASSERT_EQUAL_INT(GG_OK, calib_temp_correct(&s_calib, 1000u, 1000u, true, &corrected));
    TEST_ASSERT_EQUAL_UINT16(HAL_ADC_RAW_MAX, corrected);

    /* Coefficient -1 LSB per LSB: 10 - 900 would be negative, so it floors at 0. */
    TEST_ASSERT_EQUAL_INT(GG_OK, calib_set_temp_compensation(&s_calib, 0u, -CALIB_GAIN_ONE));
    TEST_ASSERT_EQUAL_INT(GG_OK, calib_temp_correct(&s_calib, 10u, 900u, true, &corrected));
    TEST_ASSERT_EQUAL_UINT16(0u, corrected);
}

/* @verifies REQ-FUN-004 */
static void test_calib_temp_correct_rejects_null(void)
{
    uint16_t corrected = 0u;

    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, calib_temp_correct(NULL, 300u, 520u, true, &corrected));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, calib_temp_correct(&s_calib, 300u, 520u, true, NULL));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_calib_init_rejects_null);
    RUN_TEST(test_calib_init_rejects_zero_gain);
    RUN_TEST(test_calib_zero_reading_is_zero_concentration);
    RUN_TEST(test_calib_is_linear_above_zero);
    RUN_TEST(test_calib_below_zero_reads_zero_not_negative);
    RUN_TEST(test_calib_rejects_out_of_range_raw);
    RUN_TEST(test_calib_to_lel_rejects_null);
    RUN_TEST(test_calib_temperature_correction_applied_when_valid);
    RUN_TEST(test_calib_temperature_ignored_when_sensor_invalid);
    RUN_TEST(test_calib_temperature_correction_is_clamped_to_adc_range);
    RUN_TEST(test_calib_temp_correct_rejects_null);
    return UNITY_END();
}
