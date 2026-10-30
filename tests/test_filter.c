/* Unit tests for services/filter: 8-sample moving average of raw ADC values. */
#include "unity.h"
#include "common/gg_types.h"
#include "hal/hal_adc.h"
#include "services/filter.h"

/* DEV-03: Unity hooks, called by the framework; MISRA 8.6 and unusedFunction do not apply. */
// cppcheck-suppress misra-c2012-8.6
void setUp(void);
// cppcheck-suppress misra-c2012-8.6
void tearDown(void);

static filter_t s_filter;

void setUp(void)
{
    TEST_ASSERT_EQUAL_INT(GG_OK, filter_init(&s_filter));
}

void tearDown(void) {}

/* @verifies REQ-FUN-002 */
static void test_filter_init_rejects_null(void)
{
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, filter_init(NULL));
}

/* @verifies REQ-FUN-002 */
static void test_filter_empty_has_no_value(void)
{
    uint16_t value = 0u;

    TEST_ASSERT_EQUAL_INT(GG_ERR_RANGE, filter_value(&s_filter, &value));
}

/* @verifies REQ-FUN-002 */
static void test_filter_single_sample_is_its_own_average(void)
{
    uint16_t value = 0u;

    TEST_ASSERT_EQUAL_INT(GG_OK, filter_push(&s_filter, 300u));
    TEST_ASSERT_EQUAL_INT(GG_OK, filter_value(&s_filter, &value));
    TEST_ASSERT_EQUAL_UINT16(300u, value);
}

/* @verifies REQ-FUN-002 */
static void test_filter_averages_the_window(void)
{
    uint16_t value = 0u;
    uint8_t i;

    /* Four samples of 100 and four of 300: the mean of the window is 200. */
    for (i = 0u; i < 4u; i++) {
        TEST_ASSERT_EQUAL_INT(GG_OK, filter_push(&s_filter, 100u));
    }
    for (i = 0u; i < 4u; i++) {
        TEST_ASSERT_EQUAL_INT(GG_OK, filter_push(&s_filter, 300u));
    }
    TEST_ASSERT_EQUAL_INT(GG_OK, filter_value(&s_filter, &value));
    TEST_ASSERT_EQUAL_UINT16(200u, value);
}

/* @verifies REQ-FUN-002 */
static void test_filter_window_forgets_samples_older_than_eight(void)
{
    uint16_t value = 0u;
    uint8_t i;

    /* One sample of 800 followed by eight of 100: the 800 has left the window. */
    TEST_ASSERT_EQUAL_INT(GG_OK, filter_push(&s_filter, 800u));
    for (i = 0u; i < FILTER_WINDOW; i++) {
        TEST_ASSERT_EQUAL_INT(GG_OK, filter_push(&s_filter, 100u));
    }
    TEST_ASSERT_EQUAL_INT(GG_OK, filter_value(&s_filter, &value));
    TEST_ASSERT_EQUAL_UINT16(100u, value);
}

/* @verifies REQ-FUN-002 */
static void test_filter_attenuates_a_single_spike(void)
{
    uint16_t value = 0u;
    uint8_t i;

    /* Seven samples of 100 and one spike of 900: the average is (700 + 900) / 8 = 200. */
    for (i = 0u; i < 7u; i++) {
        TEST_ASSERT_EQUAL_INT(GG_OK, filter_push(&s_filter, 100u));
    }
    TEST_ASSERT_EQUAL_INT(GG_OK, filter_push(&s_filter, 900u));
    TEST_ASSERT_EQUAL_INT(GG_OK, filter_value(&s_filter, &value));
    TEST_ASSERT_EQUAL_UINT16(200u, value);
}

/* @verifies REQ-FUN-002 */
static void test_filter_push_rejects_out_of_range_sample(void)
{
    TEST_ASSERT_EQUAL_INT(GG_ERR_RANGE, filter_push(&s_filter, HAL_ADC_RAW_MAX + 1u));
}

/* @verifies REQ-FUN-002 */
static void test_filter_push_rejects_null_filter(void)
{
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, filter_push(NULL, 100u));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_filter_init_rejects_null);
    RUN_TEST(test_filter_empty_has_no_value);
    RUN_TEST(test_filter_single_sample_is_its_own_average);
    RUN_TEST(test_filter_averages_the_window);
    RUN_TEST(test_filter_window_forgets_samples_older_than_eight);
    RUN_TEST(test_filter_attenuates_a_single_spike);
    RUN_TEST(test_filter_push_rejects_out_of_range_sample);
    RUN_TEST(test_filter_push_rejects_null_filter);
    return UNITY_END();
}
