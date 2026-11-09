/* Unit tests for the configuration service: two EEPROM copies, CRC-16, validation, safe defaults. */
#include "unity.h"
#include "common/gg_types/gg_types.h"
#include "hal/eeprom/eeprom.h"
#include "hal/eeprom/eeprom_fake.h"
#include "services/config/config.h"
#include "services/crc16/crc16.h"

#include <stdint.h>

/* DEV-03: Unity hooks, called by the framework; MISRA 8.6 and unusedFunction do not apply. */
// cppcheck-suppress misra-c2012-8.6
void setUp(void);
// cppcheck-suppress misra-c2012-8.6
void tearDown(void);

void setUp(void)
{
    hal_eeprom_fake_reset();
}

void tearDown(void) {}

/**
 * @brief Write a copy with a correct CRC but without validating its values.
 *
 * Layout as in config.c: four little-endian 16-bit fields, then the CRC of those eight bytes.
 */
static gg_status_t write_raw_copy(uint16_t addr, const config_t *cfg)
{
    uint8_t bytes[CONFIG_COPY_SIZE];
    uint16_t crc = 0u;
    gg_status_t status = GG_OK;

    bytes[0] = (uint8_t)((uint16_t)cfg->al1 & 0xFFu);
    bytes[1] = (uint8_t)((uint16_t)cfg->al1 >> 8u);
    bytes[2] = (uint8_t)((uint16_t)cfg->al2 & 0xFFu);
    bytes[3] = (uint8_t)((uint16_t)cfg->al2 >> 8u);
    bytes[4] = (uint8_t)((uint16_t)cfg->hysteresis & 0xFFu);
    bytes[5] = (uint8_t)((uint16_t)cfg->hysteresis >> 8u);
    bytes[6] = (uint8_t)(cfg->warmup_s & 0xFFu);
    bytes[7] = (uint8_t)(cfg->warmup_s >> 8u);
    status = crc16_compute(bytes, 8u, &crc);
    bytes[8] = (uint8_t)(crc & 0xFFu);
    bytes[9] = (uint8_t)(crc >> 8u);
    if (status == GG_OK) {
        status = hal_eeprom_write(addr, bytes, CONFIG_COPY_SIZE);
    }
    return status;
}

/** @brief A valid configuration that differs from the defaults. */
static config_t sample_config(void)
{
    config_t cfg;

    cfg.al1 = 150;
    cfg.al2 = 300;
    cfg.hysteresis = 30;
    cfg.warmup_s = 60u;
    return cfg;
}

/* @verifies REQ-CFG-003 */
static void test_config_validate_accepts_the_defaults(void)
{
    config_t cfg;

    TEST_ASSERT_EQUAL_INT(GG_OK, config_defaults(&cfg));
    TEST_ASSERT_EQUAL_INT(GG_OK, config_validate(&cfg));
}

/* @verifies REQ-CFG-004 */
static void test_config_defaults_match_the_requirements(void)
{
    config_t cfg;

    TEST_ASSERT_EQUAL_INT(GG_OK, config_defaults(&cfg));
    TEST_ASSERT_EQUAL_INT16(100, cfg.al1);
    TEST_ASSERT_EQUAL_INT16(250, cfg.al2);
    TEST_ASSERT_EQUAL_INT16(20, cfg.hysteresis);
    TEST_ASSERT_EQUAL_UINT16(120u, cfg.warmup_s);
}

/* @verifies REQ-ALM-005 */
static void test_config_validate_rejects_alarm_thresholds_out_of_order(void)
{
    config_t cfg = sample_config();

    cfg.al2 = cfg.al1;
    TEST_ASSERT_EQUAL_INT(GG_ERR_RANGE, config_validate(&cfg));
    cfg.al2 = 299;
    TEST_ASSERT_EQUAL_INT(GG_OK, config_validate(&cfg));
}

/* @verifies REQ-ALM-005 */
static void test_config_validate_rejects_al1_not_positive(void)
{
    config_t cfg = sample_config();

    cfg.al1 = 0;
    TEST_ASSERT_EQUAL_INT(GG_ERR_RANGE, config_validate(&cfg));
    cfg.al1 = 1;
    cfg.hysteresis = 0;
    TEST_ASSERT_EQUAL_INT(GG_OK, config_validate(&cfg));
}

/* @verifies REQ-ALM-005 */
static void test_config_validate_limits_al2_to_600(void)
{
    config_t cfg = sample_config();

    cfg.al2 = 600;
    TEST_ASSERT_EQUAL_INT(GG_OK, config_validate(&cfg));
    cfg.al2 = 601;
    TEST_ASSERT_EQUAL_INT(GG_ERR_RANGE, config_validate(&cfg));
}

/* @verifies REQ-ALM-005 */
static void test_config_validate_hysteresis_must_be_below_al1(void)
{
    config_t cfg = sample_config();

    cfg.hysteresis = -1;
    TEST_ASSERT_EQUAL_INT(GG_ERR_RANGE, config_validate(&cfg));
    cfg.hysteresis = 0;
    TEST_ASSERT_EQUAL_INT(GG_OK, config_validate(&cfg));
    cfg.hysteresis = cfg.al1;
    TEST_ASSERT_EQUAL_INT(GG_ERR_RANGE, config_validate(&cfg));
}

/* @verifies REQ-CFG-004 */
static void test_config_validate_warmup_is_between_30_and_600_s(void)
{
    config_t cfg = sample_config();

    cfg.warmup_s = 29u;
    TEST_ASSERT_EQUAL_INT(GG_ERR_RANGE, config_validate(&cfg));
    cfg.warmup_s = 30u;
    TEST_ASSERT_EQUAL_INT(GG_OK, config_validate(&cfg));
    cfg.warmup_s = 600u;
    TEST_ASSERT_EQUAL_INT(GG_OK, config_validate(&cfg));
    cfg.warmup_s = 601u;
    TEST_ASSERT_EQUAL_INT(GG_ERR_RANGE, config_validate(&cfg));
}

/* @verifies REQ-CFG-001 */
static void test_config_store_then_load_returns_copy_a(void)
{
    config_t stored = sample_config();
    config_t loaded;
    config_source_t source = CONFIG_SRC_DEFAULTS;

    TEST_ASSERT_EQUAL_INT(GG_OK, config_store(&stored));
    TEST_ASSERT_EQUAL_INT(GG_OK, config_load(&loaded, &source));
    TEST_ASSERT_EQUAL_INT(CONFIG_SRC_COPY_A, source);
    TEST_ASSERT_EQUAL_INT16(150, loaded.al1);
    TEST_ASSERT_EQUAL_INT16(300, loaded.al2);
    TEST_ASSERT_EQUAL_INT16(30, loaded.hysteresis);
    TEST_ASSERT_EQUAL_UINT16(60u, loaded.warmup_s);
}

/* @verifies REQ-CFG-002 */
static void test_config_load_falls_back_to_defaults_when_both_copies_are_invalid(void)
{
    config_t loaded;
    config_source_t source = CONFIG_SRC_COPY_A;

    TEST_ASSERT_EQUAL_INT(GG_OK, config_load(&loaded, &source));
    TEST_ASSERT_EQUAL_INT(CONFIG_SRC_DEFAULTS, source);
    TEST_ASSERT_EQUAL_INT16(100, loaded.al1);
    TEST_ASSERT_EQUAL_UINT16(120u, loaded.warmup_s);
}

/* @verifies REQ-CFG-002 */
static void test_config_load_uses_copy_b_and_repairs_copy_a(void)
{
    config_t stored = sample_config();
    config_t loaded;
    config_source_t source = CONFIG_SRC_DEFAULTS;
    uint8_t byte = 0u;
    uint8_t copy_b[CONFIG_COPY_SIZE];
    uint8_t copy_a[CONFIG_COPY_SIZE];

    TEST_ASSERT_EQUAL_INT(GG_OK, config_store(&stored));
    /* Corrupt one byte of copy A (a power cut in the middle of its write). */
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_eeprom_read(CONFIG_ADDR_A, &byte, 1u));
    byte = (uint8_t)(byte ^ 0x40u);
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_eeprom_write(CONFIG_ADDR_A, &byte, 1u));

    TEST_ASSERT_EQUAL_INT(GG_OK, config_load(&loaded, &source));
    TEST_ASSERT_EQUAL_INT(CONFIG_SRC_COPY_B, source);
    TEST_ASSERT_EQUAL_INT16(150, loaded.al1);

    TEST_ASSERT_EQUAL_INT(GG_OK, hal_eeprom_read(CONFIG_ADDR_A, copy_a, CONFIG_COPY_SIZE));
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_eeprom_read(CONFIG_ADDR_B, copy_b, CONFIG_COPY_SIZE));
    TEST_ASSERT_EQUAL_MEMORY(copy_b, copy_a, CONFIG_COPY_SIZE);
}

/* @verifies REQ-CFG-002 */
static void test_config_load_uses_copy_a_and_repairs_copy_b(void)
{
    config_t stored = sample_config();
    config_t loaded;
    config_source_t source = CONFIG_SRC_DEFAULTS;
    uint8_t byte = 0u;

    TEST_ASSERT_EQUAL_INT(GG_OK, config_store(&stored));
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_eeprom_read(CONFIG_ADDR_B + 2u, &byte, 1u));
    byte = (uint8_t)(byte ^ 0x01u);
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_eeprom_write(CONFIG_ADDR_B + 2u, &byte, 1u));

    TEST_ASSERT_EQUAL_INT(GG_OK, config_load(&loaded, &source));
    TEST_ASSERT_EQUAL_INT(CONFIG_SRC_COPY_A, source);
    TEST_ASSERT_EQUAL_INT16(150, loaded.al1);
}

/* @verifies REQ-CFG-003 */
static void test_config_store_refuses_an_invalid_configuration(void)
{
    config_t bad = sample_config();
    config_t loaded;
    config_source_t source = CONFIG_SRC_COPY_A;

    bad.al2 = bad.al1;
    TEST_ASSERT_EQUAL_INT(GG_ERR_RANGE, config_store(&bad));
    /* Nothing was written: the next load finds no valid copy. */
    TEST_ASSERT_EQUAL_INT(GG_OK, config_load(&loaded, &source));
    TEST_ASSERT_EQUAL_INT(CONFIG_SRC_DEFAULTS, source);
}

/* @verifies REQ-CFG-003 */
static void test_config_load_rejects_a_valid_crc_with_out_of_range_values(void)
{
    config_t stored = sample_config();
    config_t loaded;
    config_source_t source = CONFIG_SRC_COPY_A;

    /* Write a copy whose CRC is right but whose values are not: only validation catches it. */
    stored.al1 = 400;
    stored.al2 = 300;
    TEST_ASSERT_EQUAL_INT(GG_OK, write_raw_copy(CONFIG_ADDR_A, &stored));
    TEST_ASSERT_EQUAL_INT(GG_OK, write_raw_copy(CONFIG_ADDR_B, &stored));

    TEST_ASSERT_EQUAL_INT(GG_OK, config_load(&loaded, &source));
    TEST_ASSERT_EQUAL_INT(CONFIG_SRC_DEFAULTS, source);
}

/* @verifies REQ-CFG-001 */
static void test_config_null_arguments_are_rejected(void)
{
    config_source_t source = CONFIG_SRC_DEFAULTS;
    config_t cfg = sample_config();

    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, config_defaults(NULL));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, config_validate(NULL));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, config_store(NULL));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, config_load(NULL, &source));
    TEST_ASSERT_EQUAL_INT(GG_ERR_NULL, config_load(&cfg, NULL));
}

// DEV-03: Unity runner entry point, called by the test harness; MISRA 8.6 does not apply.
// cppcheck-suppress misra-c2012-8.6
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_config_validate_accepts_the_defaults);
    RUN_TEST(test_config_defaults_match_the_requirements);
    RUN_TEST(test_config_validate_rejects_alarm_thresholds_out_of_order);
    RUN_TEST(test_config_validate_rejects_al1_not_positive);
    RUN_TEST(test_config_validate_limits_al2_to_600);
    RUN_TEST(test_config_validate_hysteresis_must_be_below_al1);
    RUN_TEST(test_config_validate_warmup_is_between_30_and_600_s);
    RUN_TEST(test_config_store_then_load_returns_copy_a);
    RUN_TEST(test_config_load_falls_back_to_defaults_when_both_copies_are_invalid);
    RUN_TEST(test_config_load_uses_copy_b_and_repairs_copy_a);
    RUN_TEST(test_config_load_uses_copy_a_and_repairs_copy_b);
    RUN_TEST(test_config_store_refuses_an_invalid_configuration);
    RUN_TEST(test_config_load_rejects_a_valid_crc_with_out_of_range_values);
    RUN_TEST(test_config_null_arguments_are_rejected);
    return UNITY_END();
}
