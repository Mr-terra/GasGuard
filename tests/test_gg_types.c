/* Unit tests for common/gg_types.h: status codes and concentration units. */
#include "unity.h"
#include "common/gg_types.h"

#include <stdint.h>

/* Called by Unity around every test; prototypes keep -Wmissing-prototypes quiet. */
void setUp(void);
void tearDown(void);

void setUp(void) {}
void tearDown(void) {}

/* @verifies REQ-NFR-002 */
static void test_gg_types_status_ok_is_zero(void)
{
    TEST_ASSERT_EQUAL_INT(0, GG_OK);
}

/* @verifies REQ-NFR-002 */
static void test_gg_types_status_codes_are_distinct(void)
{
    TEST_ASSERT_NOT_EQUAL(GG_OK, GG_ERR_NULL);
    TEST_ASSERT_NOT_EQUAL(GG_ERR_NULL, GG_ERR_RANGE);
    TEST_ASSERT_NOT_EQUAL(GG_ERR_RANGE, GG_ERR_CRC);
    TEST_ASSERT_NOT_EQUAL(GG_ERR_CRC, GG_ERR_TIMEOUT);
    TEST_ASSERT_NOT_EQUAL(GG_ERR_TIMEOUT, GG_ERR_HW);
}

/* @verifies REQ-FUN-003 */
static void test_gg_types_concentration_is_int16_tenths_of_lel(void)
{
    TEST_ASSERT_EQUAL_UINT(2u, sizeof(gg_conc_t));
    TEST_ASSERT_EQUAL_INT(10, GG_CONC_SCALE);      /* 10 units = 1 %LEL */
    TEST_ASSERT_EQUAL_INT(600, GG_CONC_MAX);       /* 60 %LEL */
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_gg_types_status_ok_is_zero);
    RUN_TEST(test_gg_types_status_codes_are_distinct);
    RUN_TEST(test_gg_types_concentration_is_int16_tenths_of_lel);
    return UNITY_END();
}
