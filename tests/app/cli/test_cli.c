/* Unit tests for app/cli: the `log` command dumps the event log over UART, a line per poll (REQ-LOG-004). */
#include "unity.h"
#include "common/gg_types/gg_types.h"
#include "app/cli/cli.h"
#include "hal/eeprom/eeprom_fake.h"
#include "services/eventlog/eventlog.h"

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

/* DEV-03: Unity hooks, called by the framework; MISRA 8.6 and unusedFunction do not apply. */
// cppcheck-suppress misra-c2012-8.6
void setUp(void);
// cppcheck-suppress misra-c2012-8.6
void tearDown(void);

#define CAPTURE_MAX 512u
#define POLL_LIMIT 200u

/** @brief Text written by the CLI during one test. */
static char capture[CAPTURE_MAX];
static uint16_t capture_len;
/** @brief Number of next writes that fail, as when the transmit buffer is full. */
static uint8_t refuse_next;
static cli_t cli;

static gg_status_t capture_write(const uint8_t *data, uint8_t len)
{
    gg_status_t status = GG_OK;

    if (refuse_next > 0u) {
        refuse_next--;
        status = GG_ERR_TIMEOUT;
    } else {
        uint8_t i;

        for (i = 0u; (i < len) && (capture_len < (CAPTURE_MAX - 1u)); i++) {
            capture[capture_len] = (char)data[i];
            capture_len++;
        }
        capture[capture_len] = '\0';
    }
    return status;
}

void setUp(void)
{
    hal_eeprom_fake_reset();
    capture_len = 0u;
    capture[0] = '\0';
    refuse_next = 0u;
    TEST_ASSERT_EQUAL_INT(GG_OK, cli_init(&cli));
}

void tearDown(void) {}

/** @brief Feed a whole command line, ending with a newline. */
static gg_status_t type_line(const char *text)
{
    gg_status_t status = GG_OK;
    uint8_t i;

    for (i = 0u; (status == GG_OK) && (text[i] != '\0'); i++) {
        status = cli_feed(&cli, (uint8_t)text[i], capture_write);
    }
    if (status == GG_OK) {
        status = cli_feed(&cli, (uint8_t)'\n', capture_write);
    }
    return status;
}

/** @brief Poll until the dump is over, as the main loop does every 10 ms. */
static void drain(void)
{
    uint16_t n;

    for (n = 0u; (n < POLL_LIMIT) && cli.dumping; n++) {
        (void)cli_poll(&cli, capture_write);
    }
    TEST_ASSERT_FALSE(cli.dumping);
}

/** @brief Append one event with the event log, as the firmware does. */
static void log_event(uint32_t uptime_s, eventlog_code_t code, uint16_t arg)
{
    eventlog_entry_t entry;

    entry.uptime_s = uptime_s;
    entry.code = code;
    entry.arg = arg;
    TEST_ASSERT_EQUAL_INT(GG_OK, eventlog_append(&entry));
}

/* @verifies REQ-LOG-004 */
static void test_cli_log_on_an_empty_log_prints_only_the_end_line(void)
{
    TEST_ASSERT_EQUAL_INT(GG_OK, type_line("log"));
    drain();
    TEST_ASSERT_EQUAL_STRING("END\r\n", capture);
}

/* @verifies REQ-LOG-004 */
static void test_cli_log_prints_each_written_slot_with_its_code_name(void)
{
    log_event(10u, EVENTLOG_MODE_CHANGE, 1u);
    log_event(20u, EVENTLOG_FAULT, 2u);
    log_event(30u, EVENTLOG_OVERRUN, 3u);
    TEST_ASSERT_EQUAL_INT(GG_OK, type_line("log"));
    drain();
    TEST_ASSERT_EQUAL_STRING("0 10 MODE 1\r\n1 20 FAULT 2\r\n2 30 OVERRUN 3\r\nEND\r\n", capture);
}

/* @verifies REQ-LOG-004 */
static void test_cli_log_skips_slots_that_hold_no_valid_entry(void)
{
    log_event(5u, EVENTLOG_RESET, 1u);
    TEST_ASSERT_EQUAL_INT(GG_OK, type_line("log"));
    drain();
    TEST_ASSERT_EQUAL_STRING("0 5 RESET 1\r\nEND\r\n", capture);
}

/* @verifies REQ-LOG-004 */
static void test_cli_prints_large_uptimes_in_full(void)
{
    log_event(4294967295u, EVENTLOG_CONFIG, 0u);
    TEST_ASSERT_EQUAL_INT(GG_OK, type_line("log"));
    drain();
    TEST_ASSERT_EQUAL_STRING("0 4294967295 CONFIG 0\r\nEND\r\n", capture);
}

/* @verifies REQ-LOG-004 */
static void test_cli_dump_resumes_when_the_transmit_buffer_is_full(void)
{
    log_event(10u, EVENTLOG_MODE_CHANGE, 1u);
    log_event(20u, EVENTLOG_FAULT, 2u);
    TEST_ASSERT_EQUAL_INT(GG_OK, type_line("log"));
    refuse_next = 5u;
    drain();
    TEST_ASSERT_EQUAL_STRING("0 10 MODE 1\r\n1 20 FAULT 2\r\nEND\r\n", capture);
}

/* @verifies REQ-LOG-004 */
static void test_cli_unknown_or_too_long_command_is_refused(void)
{
    TEST_ASSERT_EQUAL_INT(GG_OK, type_line("status"));
    TEST_ASSERT_EQUAL_STRING("ERR\r\n", capture);
    capture_len = 0u;
    TEST_ASSERT_EQUAL_INT(GG_OK, type_line("logxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"));
    TEST_ASSERT_EQUAL_STRING("ERR\r\n", capture);
}

/* @verifies REQ-LOG-004 */
static void test_cli_empty_line_prints_nothing(void)
{
    TEST_ASSERT_EQUAL_INT(GG_OK, type_line(""));
    TEST_ASSERT_EQUAL_UINT16(0u, capture_len);
    TEST_ASSERT_FALSE(cli.dumping);
}

/* @verifies REQ-LOG-004 */
static void test_cli_write_error_on_an_answer_is_returned(void)
{
    refuse_next = 1u;
    TEST_ASSERT_EQUAL_INT(GG_ERR_TIMEOUT, type_line("status"));
}

/* @verifies REQ-NFR-002 */
static void test_cli_rejects_null_arguments(void)
{
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, cli_init(NULL));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, cli_feed(NULL, 'l', capture_write));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, cli_feed(&cli, 'l', NULL));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, cli_poll(NULL, capture_write));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, cli_poll(&cli, NULL));
}

/* @verifies REQ-LOG-004 */
static void test_cli_names_the_channel_and_display_codes(void)
{
    log_event(1u, EVENTLOG_CHANNEL_FAULT, 0u);
    log_event(2u, EVENTLOG_DISPLAY_FAULT, 0u);
    TEST_ASSERT_EQUAL_INT(GG_OK, type_line("log"));
    drain();
    TEST_ASSERT_EQUAL_STRING("0 1 CHANNEL 0\r\n1 2 DISPLAY 0\r\nEND\r\n", capture);
}

// DEV-03: Unity runner entry point, called by the test harness; MISRA 8.6 does not apply.
// cppcheck-suppress misra-c2012-8.6
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_cli_log_on_an_empty_log_prints_only_the_end_line);
    RUN_TEST(test_cli_log_prints_each_written_slot_with_its_code_name);
    RUN_TEST(test_cli_log_skips_slots_that_hold_no_valid_entry);
    RUN_TEST(test_cli_prints_large_uptimes_in_full);
    RUN_TEST(test_cli_names_the_channel_and_display_codes);
    RUN_TEST(test_cli_dump_resumes_when_the_transmit_buffer_is_full);
    RUN_TEST(test_cli_unknown_or_too_long_command_is_refused);
    RUN_TEST(test_cli_empty_line_prints_nothing);
    RUN_TEST(test_cli_write_error_on_an_answer_is_returned);
    RUN_TEST(test_cli_rejects_null_arguments);
    return UNITY_END();
}
