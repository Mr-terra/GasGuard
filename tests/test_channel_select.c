/* Unit tests for app/channel_select: the reported concentration is the maximum of the valid channels. */
#include "unity.h"
#include "common/gg_types.h"
#include "app/channel_select.h"

/* DEV-03: Unity hooks, called by the framework; MISRA 8.6 and unusedFunction do not apply. */
// cppcheck-suppress misra-c2012-8.6
void setUp(void);
// cppcheck-suppress misra-c2012-8.6
void tearDown(void);

void setUp(void) {}
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

// cppcheck-suppress misra-c2012-8.6
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_channel_select_takes_the_maximum_of_valid_channels);
    RUN_TEST(test_channel_select_ignores_faulty_channel);
    RUN_TEST(test_channel_select_no_valid_channel_reports_none);
    RUN_TEST(test_channel_select_rejects_bad_arguments);
    return UNITY_END();
}
