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

/* Gain of 10 units (1 %LEL) per LSB: 10 x 256 in Q8. */
#define CALIB_TEST_GAIN_10 2560u

static calib_t calib_test_state;

/* Zero at raw 100, no temperature term. */
static const calib_config_t calib_test_config = {100u, CALIB_TEST_GAIN_10, 0u, 0};

// cppcheck-suppress unusedFunction
void setUp(void)
{
    TEST_ASSERT_EQUAL_INT(GG_OK, calib_init(&calib_test_state, &calib_test_config));
}

// cppcheck-suppress unusedFunction
void tearDown(void) {}

/* @verifies REQ-FUN-003 */
static void test_calib_init_rejects_null(void)
{
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, calib_init(NULL, &calib_test_config));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, calib_init(&calib_test_state, NULL));
}

/* @verifies REQ-FUN-003 */
static void test_calib_init_rejects_zero_gain(void)
{
    calib_config_t bad = {100u, 0u, 0u, 0};

    TEST_ASSERT_EQUAL_INT(GG_ERR_RANGE, calib_init(&calib_test_state, &bad));
}

/* @verifies REQ-FUN-003 */
static void test_calib_zero_reading_is_zero_concentration(void)
{
    gg_conc_t conc = 99;

    TEST_ASSERT_EQUAL_INT(GG_OK, calib_to_lel(&calib_test_state, 100u, &conc));
    TEST_ASSERT_EQUAL_INT16(0, conc);
}

/* @verifies REQ-FUN-003 */
static void test_calib_is_linear_above_zero(void)
{
    gg_conc_t conc = 0;

    /* 150 raw is 50 LSB above zero, at 10 units per LSB: 500 units = 50 %LEL. */
    TEST_ASSERT_EQUAL_INT(GG_OK, calib_to_lel(&calib_test_state, 150u, &conc));
    TEST_ASSERT_EQUAL_INT16(500, conc);
}

/* @verifies REQ-FUN-003 */
static void test_calib_below_zero_reads_zero_not_negative(void)
{
    gg_conc_t conc = 99;

    /* A reading under the zero offset is sensor drift, not negative gas. */
    TEST_ASSERT_EQUAL_INT(GG_OK, calib_to_lel(&calib_test_state, 40u, &conc));
    TEST_ASSERT_EQUAL_INT16(0, conc);
}

/* @verifies REQ-FUN-003 */
static void test_calib_rejects_out_of_range_raw(void)
{
    gg_conc_t conc = 0;

    TEST_ASSERT_EQUAL_INT(GG_ERR_RANGE, calib_to_lel(&calib_test_state, HAL_ADC_RAW_MAX + 1u, &conc));
}

/* @verifies REQ-FUN-003 */
static void test_calib_to_lel_rejects_null(void)
{
    gg_conc_t conc = 0;

    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, calib_to_lel(NULL, 150u, &conc));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, calib_to_lel(&calib_test_state, 150u, NULL));
}

/* @verifies REQ-FUN-004 */
static void test_calib_temperature_correction_applied_when_valid(void)
{
    /* Reference 500, temperature reads 520 (+20 LSB), coefficient -0.5 LSB per LSB:
     * the correction is -10 LSB, so the raw value goes from 300 to 290. */
    calib_config_t cfg = {100u, CALIB_TEST_GAIN_10, 500u, -CALIB_GAIN_ONE / 2};
    calib_reading_t reading = {300u, 520u, true};
    uint16_t corrected = 0u;

    TEST_ASSERT_EQUAL_INT(GG_OK, calib_init(&calib_test_state, &cfg));
    TEST_ASSERT_EQUAL_INT(GG_OK, calib_temp_correct(&calib_test_state, &reading, &corrected));
    TEST_ASSERT_EQUAL_UINT16(290u, corrected);
}

/* @verifies REQ-FUN-004 */
static void test_calib_temperature_ignored_when_sensor_invalid(void)
{
    calib_config_t cfg = {100u, CALIB_TEST_GAIN_10, 500u, -CALIB_GAIN_ONE / 2};
    calib_reading_t reading = {300u, 900u, false};
    uint16_t corrected = 0u;

    TEST_ASSERT_EQUAL_INT(GG_OK, calib_init(&calib_test_state, &cfg));
    TEST_ASSERT_EQUAL_INT(GG_OK, calib_temp_correct(&calib_test_state, &reading, &corrected));
    TEST_ASSERT_EQUAL_UINT16(300u, corrected);
}

/* @verifies REQ-FUN-004 */
static void test_calib_temperature_correction_is_clamped_to_adc_range(void)
{
    calib_config_t up = {100u, CALIB_TEST_GAIN_10, 0u, CALIB_GAIN_ONE};
    calib_config_t down = {100u, CALIB_TEST_GAIN_10, 0u, -CALIB_GAIN_ONE};
    calib_reading_t high = {1000u, 1000u, true};
    calib_reading_t low = {10u, 900u, true};
    uint16_t corrected = 0u;

    /* Coefficient +1 LSB per LSB, temperature 1000 above reference: saturates at the top. */
    TEST_ASSERT_EQUAL_INT(GG_OK, calib_init(&calib_test_state, &up));
    TEST_ASSERT_EQUAL_INT(GG_OK, calib_temp_correct(&calib_test_state, &high, &corrected));
    TEST_ASSERT_EQUAL_UINT16(HAL_ADC_RAW_MAX, corrected);

    /* Coefficient -1 LSB per LSB: 10 - 900 would be negative, so it floors at 0. */
    TEST_ASSERT_EQUAL_INT(GG_OK, calib_init(&calib_test_state, &down));
    TEST_ASSERT_EQUAL_INT(GG_OK, calib_temp_correct(&calib_test_state, &low, &corrected));
    TEST_ASSERT_EQUAL_UINT16(0u, corrected);
}

/* @verifies REQ-FUN-004 */
static void test_calib_temp_correct_rejects_null(void)
{
    calib_reading_t reading = {300u, 520u, true};
    uint16_t corrected = 0u;

    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, calib_temp_correct(NULL, &reading, &corrected));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, calib_temp_correct(&calib_test_state, NULL, &corrected));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, calib_temp_correct(&calib_test_state, &reading, NULL));
}

// cppcheck-suppress misra-c2012-8.6
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
