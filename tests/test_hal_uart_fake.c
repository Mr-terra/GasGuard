/* Unit tests for the host UART fake, which stands in for hal/hal_uart.h on the PC. */
#include "unity.h"
#include "common/gg_types.h"
#include "hal/hal_uart.h"
#include "hal/host/hal_uart_fake.h"

#include <stdint.h>

/* DEV-03: Unity hooks, called by the framework; MISRA 8.6 and unusedFunction do not apply. */
// cppcheck-suppress misra-c2012-8.6
void setUp(void);
// cppcheck-suppress misra-c2012-8.6
void tearDown(void);

void setUp(void)
{
    hal_uart_fake_reset();
}

void tearDown(void) {}

/* @verifies REQ-LOG-004 */
static void test_hal_uart_init_succeeds(void)
{
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_uart_init());
}

/* @verifies REQ-LOG-004 */
static void test_hal_uart_write_is_captured_by_fake(void)
{
    const uint8_t msg[2] = {'O', 'K'};
    uint8_t first = 0u;

    TEST_ASSERT_EQUAL_INT(GG_OK, hal_uart_write(msg, 2u));
    TEST_ASSERT_EQUAL_UINT8(2u, hal_uart_fake_tx_count());
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_uart_fake_tx_byte(0u, &first));
    TEST_ASSERT_EQUAL_UINT8('O', first);
}

/* @verifies REQ-LOG-004 */
static void test_hal_uart_write_is_all_or_nothing_when_buffer_full(void)
{
    static uint8_t big[20]; /* zero-initialised by the C runtime (MISRA 9.3) */

    TEST_ASSERT_EQUAL_INT(GG_ERR_TIMEOUT, hal_uart_write(big, 20u));
    TEST_ASSERT_EQUAL_UINT8(0u, hal_uart_fake_tx_count());
}

/* @verifies REQ-LOG-004 */
static void test_hal_uart_read_returns_received_byte_then_timeout(void)
{
    uint8_t byte = 0u;

    TEST_ASSERT_EQUAL_INT(GG_OK, hal_uart_fake_push_rx('A'));
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_uart_read_byte(&byte));
    TEST_ASSERT_EQUAL_UINT8('A', byte);
    TEST_ASSERT_EQUAL_INT(GG_ERR_TIMEOUT, hal_uart_read_byte(&byte));
}

/* @verifies REQ-LOG-004 */
static void test_hal_uart_rejects_null_pointers(void)
{
    uint8_t byte = 0u;

    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, hal_uart_write(NULL, 1u));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, hal_uart_read_byte(NULL));
    TEST_ASSERT_EQUAL_INT(GG_ERR_RANGE, hal_uart_fake_tx_byte(5u, &byte));
}

// cppcheck-suppress misra-c2012-8.6
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_hal_uart_init_succeeds);
    RUN_TEST(test_hal_uart_write_is_captured_by_fake);
    RUN_TEST(test_hal_uart_write_is_all_or_nothing_when_buffer_full);
    RUN_TEST(test_hal_uart_read_returns_received_byte_then_timeout);
    RUN_TEST(test_hal_uart_rejects_null_pointers);
    return UNITY_END();
}
