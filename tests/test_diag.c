/* Unit tests for services/diag: open/short, stuck-at and bandgap diagnostics. */
#include "unity.h"
#include "common/gg_types.h"
#include "hal/hal_adc.h"
#include "services/diag.h"

/* DEV-03: Unity hooks, called by the framework; MISRA 8.6 and unusedFunction do not apply. */
// cppcheck-suppress misra-c2012-8.6
void setUp(void);
// cppcheck-suppress misra-c2012-8.6
void tearDown(void);

static diag_t s_diag;

void setUp(void)
{
    TEST_ASSERT_EQUAL_INT(GG_OK, diag_init(&s_diag));
}

void tearDown(void) {}

/* Feed n range checks of the same raw value. Timing is not involved here. */
static void feed_range(uint16_t raw, uint8_t n)
{
    uint8_t i;

    for (i = 0u; i < n; i++) {
        TEST_ASSERT_EQUAL_INT(GG_OK, diag_check_range(&s_diag, raw));
    }
}

/* Build the sample struct for the stuck check, then run it on the channel under test. */
static gg_status_t check_stuck(uint16_t raw, uint32_t now_ms, bool in_warmup)
{
    diag_stuck_sample_t sample = {raw, now_ms, in_warmup};

    return diag_check_stuck(&s_diag, &sample);
}

static bool is_faulty(void)
{
    bool faulty = true;

    TEST_ASSERT_EQUAL_INT(GG_OK, diag_is_faulty(&s_diag, &faulty));
    return faulty;
}

/* @verifies REQ-SAF-002 */
static void test_diag_init_rejects_null(void)
{
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, diag_init(NULL));
}

/* @verifies REQ-SAF-002 */
static void test_diag_open_circuit_faulty_after_five_samples(void)
{
    feed_range(0u, DIAG_RANGE_SAMPLES - 1u);
    TEST_ASSERT_FALSE(is_faulty());
    feed_range(0u, 1u);
    TEST_ASSERT_TRUE(is_faulty());
}

/* @verifies REQ-SAF-002 */
static void test_diag_short_circuit_faulty_after_five_samples(void)
{
    feed_range(DIAG_SHORT_MIN, DIAG_RANGE_SAMPLES);
    TEST_ASSERT_TRUE(is_faulty());
}

/* @verifies REQ-SAF-002 */
static void test_diag_valid_sample_resets_the_count(void)
{
    feed_range(0u, DIAG_RANGE_SAMPLES - 1u);
    feed_range(300u, 1u);
    feed_range(0u, DIAG_RANGE_SAMPLES - 1u);
    TEST_ASSERT_FALSE(is_faulty());
}

/* @verifies REQ-SAF-002 */
static void test_diag_range_rejects_out_of_range_input(void)
{
    TEST_ASSERT_EQUAL_INT(GG_ERR_RANGE, diag_check_range(&s_diag, HAL_ADC_RAW_MAX + 1u));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, diag_is_faulty(NULL, NULL));
}

/* @verifies REQ-SAF-003 */
static void test_diag_stuck_value_faulty_after_ten_minutes(void)
{
    uint32_t t;

    for (t = 0u; t < DIAG_STUCK_MS; t += 100u) {
        TEST_ASSERT_EQUAL_INT(GG_OK, check_stuck(512u, t, false));
    }
    TEST_ASSERT_FALSE(is_faulty());
    TEST_ASSERT_EQUAL_INT(GG_OK, check_stuck(512u, DIAG_STUCK_MS, false));
    TEST_ASSERT_TRUE(is_faulty());
}

/* @verifies REQ-SAF-003 */
static void test_diag_moving_value_is_not_stuck(void)
{
    uint32_t t;
    uint16_t raw = 300u;

    /* Two LSB of change every 100 ms: the window never settles, so no fault. */
    for (t = 0u; t <= (DIAG_STUCK_MS + 1000u); t += 100u) {
        raw = (uint16_t)((raw == 300u) ? 302u : 300u);
        TEST_ASSERT_EQUAL_INT(GG_OK, check_stuck(raw, t, false));
    }
    TEST_ASSERT_FALSE(is_faulty());
}

/* @verifies REQ-SAF-003 */
static void test_diag_stuck_check_is_inhibited_during_warmup(void)
{
    uint32_t t;

    for (t = 0u; t <= (DIAG_STUCK_MS + 1000u); t += 100u) {
        TEST_ASSERT_EQUAL_INT(GG_OK, check_stuck(512u, t, true));
    }
    TEST_ASSERT_FALSE(is_faulty());
}

/* @verifies REQ-SAF-004 */
static void test_diag_bandgap_nominal_is_ok(void)
{
    bool ok = false;

    TEST_ASSERT_EQUAL_INT(GG_OK, diag_bandgap_ok(DIAG_BANDGAP_NOMINAL, &ok));
    TEST_ASSERT_TRUE(ok);
}

/* @verifies REQ-SAF-004 */
static void test_diag_bandgap_within_ten_percent_is_ok(void)
{
    bool ok = false;

    TEST_ASSERT_EQUAL_INT(GG_OK, diag_bandgap_ok(205u, &ok));
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_INT(GG_OK, diag_bandgap_ok(245u, &ok));
    TEST_ASSERT_TRUE(ok);
}

/* @verifies REQ-SAF-004 */
static void test_diag_bandgap_outside_ten_percent_is_fault(void)
{
    bool ok = true;

    TEST_ASSERT_EQUAL_INT(GG_OK, diag_bandgap_ok(200u, &ok));
    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_EQUAL_INT(GG_OK, diag_bandgap_ok(250u, &ok));
    TEST_ASSERT_FALSE(ok);
}

/* @verifies REQ-SAF-004 */
static void test_diag_bandgap_rejects_null_and_out_of_range(void)
{
    bool ok = false;

    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, diag_bandgap_ok(DIAG_BANDGAP_NOMINAL, NULL));
    TEST_ASSERT_EQUAL_INT(GG_ERR_RANGE, diag_bandgap_ok(HAL_ADC_RAW_MAX + 1u, &ok));
}

// cppcheck-suppress misra-c2012-8.6
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_diag_init_rejects_null);
    RUN_TEST(test_diag_open_circuit_faulty_after_five_samples);
    RUN_TEST(test_diag_short_circuit_faulty_after_five_samples);
    RUN_TEST(test_diag_valid_sample_resets_the_count);
    RUN_TEST(test_diag_range_rejects_out_of_range_input);
    RUN_TEST(test_diag_stuck_value_faulty_after_ten_minutes);
    RUN_TEST(test_diag_moving_value_is_not_stuck);
    RUN_TEST(test_diag_stuck_check_is_inhibited_during_warmup);
    RUN_TEST(test_diag_bandgap_nominal_is_ok);
    RUN_TEST(test_diag_bandgap_within_ten_percent_is_ok);
    RUN_TEST(test_diag_bandgap_outside_ten_percent_is_fault);
    RUN_TEST(test_diag_bandgap_rejects_null_and_out_of_range);
    return UNITY_END();
}
