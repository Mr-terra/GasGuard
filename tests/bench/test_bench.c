/* Unit tests for the bench tool (bench/bench.c, `make avr-bench` only, FMEA F49).
 *
 * The bench code measures requirements on the board but implements none, so these
 * tests carry no @verifies tag: they check that the measuring tool itself is right
 * (commands, number printing, report period, injected overruns).
 * The `#h` hang is an endless loop by design: it is checked on the board only.
 */
#include "unity.h"
#include "bench/bench.h"
#include "common/gg_types/gg_types.h"
#include "hal/stack/stack_fake.h"
#include "hal/timer/timer.h"
#include "hal/timer/timer_fake.h"
#include "hal/uart/uart.h"
#include "hal/uart/uart_fake.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* DEV-03: Unity hooks, called by the framework; MISRA 8.6 and unusedFunction do not apply. */
// cppcheck-suppress misra-c2012-8.6
void setUp(void);
// cppcheck-suppress misra-c2012-8.6
void tearDown(void);

/** @brief Bench period of the statistics line, as in bench.c. */
#define BENCH_TEST_REPORT_MS 10000u
/** @brief Length of a stretched cycle, as in bench.c (overrun limit 100 ms + 50 ms). */
#define BENCH_TEST_STALL_MS 150u
/** @brief Boot time used by most tests. */
#define BENCH_TEST_BOOT_MS 1000u
/** @brief Room for the transmitted text and its terminator. */
#define BENCH_TEST_TEXT_MAX (HAL_UART_FAKE_TX_CAPACITY + 1u)

/** @brief Copy the transmitted bytes into a static text, zero-terminated. */
static const char *sent_text(void)
{
    static char s_sent[BENCH_TEST_TEXT_MAX];   /* Text the bench sent since the last reset of the UART fake. */
    uint8_t count = hal_uart_fake_tx_count();
    uint8_t i;

    for (i = 0u; i < count; i++) {
        uint8_t byte = 0u;

        TEST_ASSERT_EQUAL_INT(GG_OK, hal_uart_fake_tx_byte(i, &byte));
        s_sent[i] = (char)byte;
    }
    s_sent[count] = '\0';
    return s_sent;
}

/** @brief Feed a text to bench_take_byte(); return how many bytes the bench took. */
static uint8_t feed(const char *text)
{
    uint8_t taken_count = 0u;
    size_t i;

    for (i = 0u; text[i] != '\0'; i++) {
        bool taken = false;

        TEST_ASSERT_EQUAL_INT(GG_OK, bench_take_byte((uint8_t)text[i], &taken));
        if (taken) {
            taken_count++;
        }
    }
    return taken_count;
}

void setUp(void)
{
    hal_timer_fake_reset();
    hal_uart_fake_reset();
    hal_stack_fake_reset();
    hal_timer_fake_set_ms(BENCH_TEST_BOOT_MS);
    TEST_ASSERT_EQUAL_INT(GG_OK, bench_boot());
    hal_uart_fake_reset();   /* Drop the boot banner: each test checks its own output. */
}

void tearDown(void) {}

/* --- Boot banner --- */

static void test_bench_boot_prints_the_boot_time(void)
{
    hal_uart_fake_reset();
    hal_timer_fake_set_ms(1234u);
    TEST_ASSERT_EQUAL_INT(GG_OK, bench_boot());
    TEST_ASSERT_EQUAL_STRING("BENCH BOOT 1234\r\n", sent_text());
}

static void test_bench_boot_prints_zero_and_the_largest_time(void)
{
    hal_uart_fake_reset();
    hal_timer_fake_set_ms(0u);
    TEST_ASSERT_EQUAL_INT(GG_OK, bench_boot());
    TEST_ASSERT_EQUAL_STRING("BENCH BOOT 0\r\n", sent_text());
    hal_uart_fake_reset();
    hal_timer_fake_set_ms(UINT32_MAX);
    TEST_ASSERT_EQUAL_INT(GG_OK, bench_boot());
    TEST_ASSERT_EQUAL_STRING("BENCH BOOT 4294967295\r\n", sent_text());
}

/* --- Commands --- */

static void test_bench_take_byte_rejects_a_null_pointer(void)
{
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, bench_take_byte((uint8_t)'#', NULL));
}

static void test_bench_command_letters_without_prefix_go_to_the_command_line(void)
{
    TEST_ASSERT_EQUAL_UINT8(0u, feed("status\n"));
    TEST_ASSERT_EQUAL_INT(GG_OK, bench_after_cycle(BENCH_TEST_BOOT_MS, BENCH_TEST_BOOT_MS));
    TEST_ASSERT_EQUAL_UINT8(0u, hal_uart_fake_tx_count());
}

static void test_bench_statistics_command_prints_at_the_next_cycle_end(void)
{
    hal_stack_fake_set_free(300u);
    TEST_ASSERT_EQUAL_INT(GG_OK, bench_display_time(12u));
    TEST_ASSERT_EQUAL_INT(GG_OK, bench_after_cycle(2000u, 2037u));
    TEST_ASSERT_EQUAL_UINT8(0u, hal_uart_fake_tx_count());
    TEST_ASSERT_EQUAL_UINT8(2u, feed("#s"));
    TEST_ASSERT_EQUAL_INT(GG_OK, bench_after_cycle(3000u, 3001u));
    TEST_ASSERT_EQUAL_STRING("BENCH 3 37 12 300\r\n", sent_text());
}

static void test_bench_maxima_keep_the_longest_value(void)
{
    TEST_ASSERT_EQUAL_INT(GG_OK, bench_display_time(20u));
    TEST_ASSERT_EQUAL_INT(GG_OK, bench_display_time(5u));
    TEST_ASSERT_EQUAL_INT(GG_OK, bench_after_cycle(2000u, 2050u));
    TEST_ASSERT_EQUAL_INT(GG_OK, bench_after_cycle(2100u, 2101u));
    hal_stack_fake_set_free(64u);
    (void)feed("#s");
    TEST_ASSERT_EQUAL_INT(GG_OK, bench_after_cycle(2200u, 2200u));
    TEST_ASSERT_EQUAL_STRING("BENCH 2 50 20 64\r\n", sent_text());
}

static void test_bench_prefix_alone_on_a_line_passes_the_line_end_and_runs_nothing(void)
{
    bool taken = false;

    TEST_ASSERT_EQUAL_INT(GG_OK, bench_take_byte((uint8_t)'#', &taken));
    TEST_ASSERT_TRUE(taken);
    TEST_ASSERT_EQUAL_INT(GG_OK, bench_take_byte((uint8_t)'\n', &taken));
    TEST_ASSERT_FALSE(taken);
    /* The next letter is a normal byte again. */
    TEST_ASSERT_EQUAL_UINT8(0u, feed("s"));
    TEST_ASSERT_EQUAL_INT(GG_OK, bench_after_cycle(BENCH_TEST_BOOT_MS, BENCH_TEST_BOOT_MS));
    TEST_ASSERT_EQUAL_UINT8(0u, hal_uart_fake_tx_count());
}

static void test_bench_prefix_cancelled_by_carriage_return(void)
{
    bool taken = true;

    (void)feed("#");
    TEST_ASSERT_EQUAL_INT(GG_OK, bench_take_byte((uint8_t)'\r', &taken));
    TEST_ASSERT_FALSE(taken);
}

static void test_bench_double_prefix_still_runs_the_command(void)
{
    TEST_ASSERT_EQUAL_UINT8(3u, feed("##s"));
    TEST_ASSERT_EQUAL_INT(GG_OK, bench_after_cycle(BENCH_TEST_BOOT_MS, BENCH_TEST_BOOT_MS));
    TEST_ASSERT_TRUE(hal_uart_fake_tx_count() > 0u);
}

static void test_bench_unknown_letter_is_taken_and_does_nothing(void)
{
    uint32_t before = 0u;
    uint32_t after = 0u;

    TEST_ASSERT_EQUAL_UINT8(2u, feed("#x"));
    hal_timer_fake_set_step(1u);
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_timer_get_ms(&before));
    TEST_ASSERT_EQUAL_INT(GG_OK, bench_stall());
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_timer_get_ms(&after));
    TEST_ASSERT_EQUAL_UINT32(1u, after - before);   /* No stall: only the read of `before` moved it. */
    TEST_ASSERT_EQUAL_INT(GG_OK, bench_after_cycle(BENCH_TEST_BOOT_MS, BENCH_TEST_BOOT_MS));
    TEST_ASSERT_EQUAL_UINT8(0u, hal_uart_fake_tx_count());
}

/* --- Report period --- */

static void test_bench_report_comes_at_ten_seconds_not_one_ms_before(void)
{
    TEST_ASSERT_EQUAL_INT(GG_OK, bench_after_cycle(0u, BENCH_TEST_BOOT_MS + BENCH_TEST_REPORT_MS - 1u));
    TEST_ASSERT_EQUAL_UINT8(0u, hal_uart_fake_tx_count());
    TEST_ASSERT_EQUAL_INT(GG_OK, bench_after_cycle(0u, BENCH_TEST_BOOT_MS + BENCH_TEST_REPORT_MS));
    TEST_ASSERT_TRUE(hal_uart_fake_tx_count() > 0u);
    /* The next one is 10 s after this one. */
    hal_uart_fake_reset();
    TEST_ASSERT_EQUAL_INT(GG_OK, bench_after_cycle(0u, BENCH_TEST_BOOT_MS + (2u * BENCH_TEST_REPORT_MS) - 1u));
    TEST_ASSERT_EQUAL_UINT8(0u, hal_uart_fake_tx_count());
}

static void test_bench_report_period_survives_the_clock_wrap(void)
{
    const uint32_t boot_ms = UINT32_MAX - 5000u;

    hal_timer_fake_set_ms(boot_ms);
    TEST_ASSERT_EQUAL_INT(GG_OK, bench_boot());
    hal_uart_fake_reset();
    /* 10 s after the boot, the clock has wrapped to 4999. */
    TEST_ASSERT_EQUAL_INT(GG_OK, bench_after_cycle(4998u, boot_ms + BENCH_TEST_REPORT_MS));
    TEST_ASSERT_TRUE(hal_uart_fake_tx_count() > 0u);
}

static void test_bench_full_transmit_buffer_loses_the_line_without_error(void)
{
    static uint8_t fill[HAL_UART_FAKE_TX_CAPACITY]; /* zero-initialised by the C runtime (MISRA 9.3) */

    TEST_ASSERT_EQUAL_INT(GG_OK, hal_uart_write(fill, (uint8_t)HAL_UART_FAKE_TX_CAPACITY));
    (void)feed("#s");
    TEST_ASSERT_EQUAL_INT(GG_OK, bench_after_cycle(BENCH_TEST_BOOT_MS, BENCH_TEST_BOOT_MS));
    TEST_ASSERT_EQUAL_UINT8(HAL_UART_FAKE_TX_CAPACITY, hal_uart_fake_tx_count());
}

static void test_bench_longest_line_fits_the_transmit_buffer(void)
{
    hal_stack_fake_set_free(UINT16_MAX);
    TEST_ASSERT_EQUAL_INT(GG_OK, bench_display_time(UINT32_MAX));
    TEST_ASSERT_EQUAL_INT(GG_OK, bench_after_cycle(1u, 0u));   /* Wrapped: UINT32_MAX ms long. */
    hal_uart_fake_reset();   /* That cycle end was also a periodic report: drop it. */
    (void)feed("#s");
    TEST_ASSERT_EQUAL_INT(GG_OK, bench_after_cycle(UINT32_MAX, UINT32_MAX));
    TEST_ASSERT_EQUAL_STRING("BENCH 4294967 4294967295 4294967295 65535\r\n", sent_text());
}

/* --- Injected overruns --- */

/** @brief Run bench_stall() with a clock that moves 1 ms per read; return the time it took. */
static uint32_t stall_ms(void)
{
    uint32_t before = 0u;
    uint32_t after = 0u;

    hal_timer_fake_set_step(0u);
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_timer_get_ms(&before));
    hal_timer_fake_set_step(1u);
    TEST_ASSERT_EQUAL_INT(GG_OK, bench_stall());
    hal_timer_fake_set_step(0u);
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_timer_get_ms(&after));
    return after - before;
}

static void test_bench_stall_does_nothing_without_a_command(void)
{
    TEST_ASSERT_EQUAL_UINT32(0u, stall_ms());   /* The clock is not even read. */
}

static void test_bench_one_overrun_command_stretches_exactly_one_cycle(void)
{
    uint32_t first = 0u;

    (void)feed("#o");
    first = stall_ms();
    TEST_ASSERT_TRUE(first >= BENCH_TEST_STALL_MS);          /* Over the 100 ms overrun limit. */
    TEST_ASSERT_TRUE(first <= (BENCH_TEST_STALL_MS + 2u));   /* Far below the 500 ms watchdog. */
    TEST_ASSERT_EQUAL_UINT32(0u, stall_ms());
}

static void test_bench_fault_command_stretches_three_cycles_in_a_row(void)
{
    (void)feed("#f");
    TEST_ASSERT_TRUE(stall_ms() >= BENCH_TEST_STALL_MS);
    TEST_ASSERT_TRUE(stall_ms() >= BENCH_TEST_STALL_MS);
    TEST_ASSERT_TRUE(stall_ms() >= BENCH_TEST_STALL_MS);
    TEST_ASSERT_EQUAL_UINT32(0u, stall_ms());
}

// cppcheck-suppress misra-c2012-8.6
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_bench_boot_prints_the_boot_time);
    RUN_TEST(test_bench_boot_prints_zero_and_the_largest_time);
    RUN_TEST(test_bench_take_byte_rejects_a_null_pointer);
    RUN_TEST(test_bench_command_letters_without_prefix_go_to_the_command_line);
    RUN_TEST(test_bench_statistics_command_prints_at_the_next_cycle_end);
    RUN_TEST(test_bench_maxima_keep_the_longest_value);
    RUN_TEST(test_bench_prefix_alone_on_a_line_passes_the_line_end_and_runs_nothing);
    RUN_TEST(test_bench_prefix_cancelled_by_carriage_return);
    RUN_TEST(test_bench_double_prefix_still_runs_the_command);
    RUN_TEST(test_bench_unknown_letter_is_taken_and_does_nothing);
    RUN_TEST(test_bench_report_comes_at_ten_seconds_not_one_ms_before);
    RUN_TEST(test_bench_report_period_survives_the_clock_wrap);
    RUN_TEST(test_bench_full_transmit_buffer_loses_the_line_without_error);
    RUN_TEST(test_bench_longest_line_fits_the_transmit_buffer);
    RUN_TEST(test_bench_stall_does_nothing_without_a_command);
    RUN_TEST(test_bench_one_overrun_command_stretches_exactly_one_cycle);
    RUN_TEST(test_bench_fault_command_stretches_three_cycles_in_a_row);
    return UNITY_END();
}
