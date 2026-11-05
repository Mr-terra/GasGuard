/* Unit tests for the button debounce logic (pure computation, no hardware). */
#include "unity.h"
#include "common/gg_types/gg_types.h"
#include "drivers/button/button.h"

#include <stdbool.h>
#include <stdint.h>

/* DEV-03: Unity hooks, called by the framework; MISRA 8.6 and unusedFunction do not apply. */
// cppcheck-suppress misra-c2012-8.6
void setUp(void);
// cppcheck-suppress misra-c2012-8.6
void tearDown(void);

static button_t test_button;

void setUp(void)
{
    (void)button_init(&test_button);
}

void tearDown(void) {}

/* @verifies REQ-HMI-007 */
static void test_button_press_shorter_than_30_ms_is_ignored(void)
{
    bool pressed = true;

    TEST_ASSERT_EQUAL_INT(GG_OK, button_step(&test_button, true, 0u, &pressed));
    TEST_ASSERT_EQUAL_INT(GG_OK, button_step(&test_button, true, 29u, &pressed));
    TEST_ASSERT_EQUAL_INT(GG_OK, button_step(&test_button, false, 30u, &pressed));
    TEST_ASSERT_FALSE(pressed);
}

/* @verifies REQ-HMI-007 */
static void test_button_press_is_accepted_after_exactly_30_ms(void)
{
    bool pressed = false;

    TEST_ASSERT_EQUAL_INT(GG_OK, button_step(&test_button, true, 100u, &pressed));
    TEST_ASSERT_FALSE(pressed);
    TEST_ASSERT_EQUAL_INT(GG_OK, button_step(&test_button, true, 129u, &pressed));
    TEST_ASSERT_FALSE(pressed);
    TEST_ASSERT_EQUAL_INT(GG_OK, button_step(&test_button, true, 130u, &pressed));
    TEST_ASSERT_TRUE(pressed);
}

/* @verifies REQ-HMI-007 */
static void test_button_bounce_restarts_the_30_ms_window(void)
{
    bool pressed = false;

    (void)button_step(&test_button, true, 0u, &pressed);
    (void)button_step(&test_button, false, 20u, &pressed);
    (void)button_step(&test_button, true, 25u, &pressed);
    TEST_ASSERT_EQUAL_INT(GG_OK, button_step(&test_button, true, 54u, &pressed));
    TEST_ASSERT_FALSE(pressed);
    TEST_ASSERT_EQUAL_INT(GG_OK, button_step(&test_button, true, 55u, &pressed));
    TEST_ASSERT_TRUE(pressed);
}

/* @verifies REQ-HMI-007 */
static void test_button_release_is_debounced_too(void)
{
    bool pressed = false;

    (void)button_step(&test_button, true, 0u, &pressed);
    (void)button_step(&test_button, true, 30u, &pressed);
    TEST_ASSERT_TRUE(pressed);
    (void)button_step(&test_button, false, 100u, &pressed);
    TEST_ASSERT_TRUE(pressed);
    TEST_ASSERT_EQUAL_INT(GG_OK, button_step(&test_button, false, 130u, &pressed));
    TEST_ASSERT_FALSE(pressed);
}

/* @verifies REQ-HMI-007 */
static void test_button_window_is_correct_across_millisecond_wraparound(void)
{
    bool pressed = false;
    uint32_t start = UINT32_MAX - 10u;

    (void)button_step(&test_button, true, start, &pressed);
    TEST_ASSERT_EQUAL_INT(GG_OK, button_step(&test_button, true, start + 30u, &pressed));
    TEST_ASSERT_TRUE(pressed);
}

/* @verifies REQ-HMI-007 */
static void test_button_step_rejects_null_arguments(void)
{
    bool pressed = false;

    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, button_step(NULL, true, 0u, &pressed));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, button_step(&test_button, true, 0u, NULL));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, button_init(NULL));
}

// DEV-03: Unity runner entry point, called by the test harness; MISRA 8.6 does not apply.
// cppcheck-suppress misra-c2012-8.6
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_button_press_shorter_than_30_ms_is_ignored);
    RUN_TEST(test_button_press_is_accepted_after_exactly_30_ms);
    RUN_TEST(test_button_bounce_restarts_the_30_ms_window);
    RUN_TEST(test_button_release_is_debounced_too);
    RUN_TEST(test_button_window_is_correct_across_millisecond_wraparound);
    RUN_TEST(test_button_step_rejects_null_arguments);
    return UNITY_END();
}
