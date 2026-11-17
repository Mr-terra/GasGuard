/* Unit tests for app/channel_select: the reported concentration is the maximum of the valid channels. */
#include "unity.h"
#include "common/gg_types/gg_types.h"
#include "app/channel_select/channel_select.h"

/* DEV-03: Unity hooks, called by the framework; MISRA 8.6 and unusedFunction do not apply. */
// cppcheck-suppress misra-c2012-8.6
void setUp(void);
// cppcheck-suppress misra-c2012-8.6
void tearDown(void);

// DEV-03: Unity hooks, called by the framework; MISRA 8.6 and unusedFunction do not apply.
// cppcheck-suppress unusedFunction
void setUp(void) {}
// cppcheck-suppress unusedFunction
void tearDown(void) {}

/* @verifies REQ-FUN-005 */
static void test_channel_select_takes_the_maximum_of_valid_channels(void)
{
    const gg_conc_t concs[2] = {120, 300};
    const bool valid[2] = {true, true};
    gg_conc_t out = 0;
    bool any = false;

    TEST_ASSERT_EQUAL_INT(GG_OK, channel_select_max(concs, valid, 2u, &out, &any));
    TEST_ASSERT_EQUAL_INT16(300, out);
    TEST_ASSERT_TRUE(any);
}

/* @verifies REQ-FUN-005 */
static void test_channel_select_ignores_faulty_channel(void)
{
    /* The higher reading comes from a faulty channel and must not be reported. */
    const gg_conc_t concs[2] = {120, 900};
    const bool valid[2] = {true, false};
    gg_conc_t out = 0;
    bool any = false;

    TEST_ASSERT_EQUAL_INT(GG_OK, channel_select_max(concs, valid, 2u, &out, &any));
    TEST_ASSERT_EQUAL_INT16(120, out);
    TEST_ASSERT_TRUE(any);
}

/* @verifies REQ-FUN-005 */
static void test_channel_select_no_valid_channel_reports_none(void)
{
    const gg_conc_t concs[2] = {900, 900};
    const bool valid[2] = {false, false};
    gg_conc_t out = 99;
    bool any = true;

    TEST_ASSERT_EQUAL_INT(GG_OK, channel_select_max(concs, valid, 2u, &out, &any));
    TEST_ASSERT_FALSE(any);
    TEST_ASSERT_EQUAL_INT16(0, out);
}

/* @verifies REQ-NFR-002 */
static void test_channel_select_rejects_bad_arguments(void)
{
    const gg_conc_t concs[1] = {0};
    const bool valid[1] = {true};
    gg_conc_t out = 0;
    bool any = false;

    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, channel_select_max(NULL, valid, 1u, &out, &any));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, channel_select_max(concs, NULL, 1u, &out, &any));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, channel_select_max(concs, valid, 1u, NULL, &any));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, channel_select_max(concs, valid, 1u, &out, NULL));
    TEST_ASSERT_EQUAL_INT(GG_ERR_RANGE, channel_select_max(concs, valid, 0u, &out, &any));
}

/* @verifies REQ-FUN-005 */
static void test_channel_select_maximum_does_not_depend_on_the_channel_order(void)
{
    const gg_conc_t concs[2] = {300, 120};
    const bool valid[2] = {true, true};
    gg_conc_t out = 0;
    bool any = false;

    TEST_ASSERT_EQUAL_INT(GG_OK, channel_select_max(concs, valid, 2u, &out, &any));
    TEST_ASSERT_EQUAL_INT16(300, out);
}

/* @verifies REQ-FUN-005 */
static void test_channel_select_gas_on_the_second_channel_is_seen_when_the_first_is_faulty(void)
{
    /* The faulty first channel reads 0: the gas on the second one must still win. */
    const gg_conc_t concs[2] = {0, 260};
    const bool valid[2] = {false, true};
    gg_conc_t out = 0;
    bool any = false;

    TEST_ASSERT_EQUAL_INT(GG_OK, channel_select_max(concs, valid, 2u, &out, &any));
    TEST_ASSERT_EQUAL_INT16(260, out);
    TEST_ASSERT_TRUE(any);
}

/* @verifies REQ-FUN-005 */
static void test_channel_select_clean_air_on_a_valid_channel_is_still_a_valid_reading(void)
{
    /* 0 %LEL on a healthy channel is a real reading: "no valid channel" here
     * would raise a FAULT in clean air. */
    const gg_conc_t concs[2] = {0, 0};
    const bool valid[2] = {true, false};
    gg_conc_t out = 99;
    bool any = false;

    TEST_ASSERT_EQUAL_INT(GG_OK, channel_select_max(concs, valid, 2u, &out, &any));
    TEST_ASSERT_TRUE(any);
    TEST_ASSERT_EQUAL_INT16(0, out);
}

/* @verifies REQ-FUN-005 */
static void test_channel_select_equal_readings_and_one_channel(void)
{
    const gg_conc_t equal[2] = {250, 250};
    const bool both[2] = {true, true};
    const gg_conc_t single[1] = {GG_CONC_MAX};
    const bool one[1] = {true};
    gg_conc_t out = 0;
    bool any = false;

    TEST_ASSERT_EQUAL_INT(GG_OK, channel_select_max(equal, both, 2u, &out, &any));
    TEST_ASSERT_EQUAL_INT16(250, out);
    TEST_ASSERT_EQUAL_INT(GG_OK, channel_select_max(single, one, 1u, &out, &any));
    TEST_ASSERT_EQUAL_INT16(GG_CONC_MAX, out);
    TEST_ASSERT_TRUE(any);
}

/* @verifies REQ-FUN-005 */
static void test_channel_select_largest_value_is_kept_whole(void)
{
    const gg_conc_t concs[2] = {INT16_MAX, 1};
    const bool valid[2] = {true, true};
    gg_conc_t out = 0;
    bool any = false;

    TEST_ASSERT_EQUAL_INT(GG_OK, channel_select_max(concs, valid, 2u, &out, &any));
    TEST_ASSERT_EQUAL_INT16(INT16_MAX, out);
}

/* @verifies REQ-FUN-005 */
static void test_channel_select_rejected_call_leaves_the_outputs_unchanged(void)
{
    const gg_conc_t concs[1] = {500};
    const bool valid[1] = {true};
    gg_conc_t out = 7;
    bool any = false;

    TEST_ASSERT_EQUAL_INT(GG_ERR_RANGE, channel_select_max(concs, valid, 0u, &out, &any));
    TEST_ASSERT_EQUAL_INT16(7, out);
    TEST_ASSERT_FALSE(any);
}

// cppcheck-suppress misra-c2012-8.6
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_channel_select_takes_the_maximum_of_valid_channels);
    RUN_TEST(test_channel_select_ignores_faulty_channel);
    RUN_TEST(test_channel_select_no_valid_channel_reports_none);
    RUN_TEST(test_channel_select_rejects_bad_arguments);
    RUN_TEST(test_channel_select_maximum_does_not_depend_on_the_channel_order);
    RUN_TEST(test_channel_select_gas_on_the_second_channel_is_seen_when_the_first_is_faulty);
    RUN_TEST(test_channel_select_clean_air_on_a_valid_channel_is_still_a_valid_reading);
    RUN_TEST(test_channel_select_equal_readings_and_one_channel);
    RUN_TEST(test_channel_select_largest_value_is_kept_whole);
    RUN_TEST(test_channel_select_rejected_call_leaves_the_outputs_unchanged);
    return UNITY_END();
}
