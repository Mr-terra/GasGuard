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

/* @verifies REQ-CFG-002 */
static void test_config_failed_repair_keeps_the_valid_copy_in_use(void)
{
    config_t stored;
    config_t cfg;
    config_source_t source = CONFIG_SRC_DEFAULTS;
    uint8_t junk[CONFIG_COPY_SIZE];
    uint8_t i;

    TEST_ASSERT_EQUAL_INT(GG_OK, config_defaults(&stored));
    stored.al1 = 120;
    TEST_ASSERT_EQUAL_INT(GG_OK, config_store(&stored));
    for (i = 0u; i < CONFIG_COPY_SIZE; i++) {
        junk[i] = 0xFFu;
    }
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_eeprom_write(CONFIG_ADDR_A, junk, CONFIG_COPY_SIZE));

    hal_eeprom_fake_fail_writes(true);
    TEST_ASSERT_EQUAL_INT(GG_OK, config_load(&cfg, &source));
    hal_eeprom_fake_fail_writes(false);
    TEST_ASSERT_EQUAL_INT(CONFIG_SRC_COPY_B, source);
    TEST_ASSERT_EQUAL_INT(120, cfg.al1);
}

// DEV-03: Unity runner entry point, called by the test harness; MISRA 8.6 does not apply.
/** @brief Flip one bit of one stored byte, as a worn or half-written EEPROM cell would. */
static void flip_stored_bit(uint16_t addr, uint8_t mask)
{
    uint8_t byte = 0u;

    TEST_ASSERT_EQUAL_INT(GG_OK, hal_eeprom_read(addr, &byte, 1u));
    byte = (uint8_t)(byte ^ mask);
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_eeprom_write(addr, &byte, 1u));
}

/* @verifies REQ-ALM-005 */
static void test_config_validate_al2_one_step_above_al1_is_accepted(void)
{
    config_t cfg = sample_config();

    cfg.al2 = (int16_t)(cfg.al1 + 1);
    TEST_ASSERT_EQUAL_INT(GG_OK, config_validate(&cfg));
    cfg.al2 = (int16_t)(cfg.al1 - 1);
    TEST_ASSERT_EQUAL_INT(GG_ERR_RANGE, config_validate(&cfg));
}

/* @verifies REQ-ALM-005 */
static void test_config_validate_negative_thresholds_are_rejected(void)
{
    config_t cfg = sample_config();

    cfg.al1 = -1;
    cfg.hysteresis = 0;
    TEST_ASSERT_EQUAL_INT(GG_ERR_RANGE, config_validate(&cfg));
    cfg = sample_config();
    cfg.al2 = -1;
    TEST_ASSERT_EQUAL_INT(GG_ERR_RANGE, config_validate(&cfg));
    cfg = sample_config();
    cfg.al1 = INT16_MIN;
    cfg.al2 = INT16_MAX;
    TEST_ASSERT_EQUAL_INT(GG_ERR_RANGE, config_validate(&cfg));
}

/* @verifies REQ-ALM-005 */
static void test_config_validate_accepts_the_widest_valid_settings(void)
{
    config_t cfg = sample_config();

    /* Every limit at once, on its valid side: none may be refused (a refused
     * valid setting would put the unit on its defaults for no reason). */
    cfg.al1 = 1;
    cfg.al2 = CONFIG_AL2_MAX;
    cfg.hysteresis = 0;
    cfg.warmup_s = CONFIG_WARMUP_MIN_S;
    TEST_ASSERT_EQUAL_INT(GG_OK, config_validate(&cfg));
    cfg.al1 = CONFIG_AL2_MAX - 1;
    cfg.hysteresis = (int16_t)(cfg.al1 - 1);
    cfg.warmup_s = CONFIG_WARMUP_MAX_S;
    TEST_ASSERT_EQUAL_INT(GG_OK, config_validate(&cfg));
}

/* @verifies REQ-ALM-005 */
static void test_config_validate_hysteresis_one_below_al1_is_accepted(void)
{
    config_t cfg = sample_config();

    cfg.hysteresis = (int16_t)(cfg.al1 - 1);
    TEST_ASSERT_EQUAL_INT(GG_OK, config_validate(&cfg));
}

/* @verifies REQ-CFG-001 */
static void test_config_store_writes_two_identical_copies(void)
{
    config_t stored = sample_config();
    uint8_t copy_a[CONFIG_COPY_SIZE];
    uint8_t copy_b[CONFIG_COPY_SIZE];

    TEST_ASSERT_EQUAL_INT(GG_OK, config_store(&stored));
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_eeprom_read(CONFIG_ADDR_A, copy_a, CONFIG_COPY_SIZE));
    TEST_ASSERT_EQUAL_INT(GG_OK, hal_eeprom_read(CONFIG_ADDR_B, copy_b, CONFIG_COPY_SIZE));
    TEST_ASSERT_EQUAL_MEMORY(copy_a, copy_b, CONFIG_COPY_SIZE);
}

/* @verifies REQ-CFG-001 */
static void test_config_flipped_bit_in_a_crc_byte_is_caught(void)
{
    config_t stored = sample_config();
    config_t loaded;
    config_source_t source = CONFIG_SRC_COPY_A;
    uint16_t crc_high_addr = CONFIG_ADDR_A;   /* Becomes the last byte of copy A. */

    /* The damage is in the checksum itself, not in the values: copy A must
     * still be refused, and copy B used. */
    crc_high_addr = (uint16_t)(crc_high_addr + CONFIG_COPY_SIZE);
    crc_high_addr--;
    TEST_ASSERT_EQUAL_INT(GG_OK, config_store(&stored));
    flip_stored_bit(crc_high_addr, 0x01u);
    TEST_ASSERT_EQUAL_INT(GG_OK, config_load(&loaded, &source));
    TEST_ASSERT_EQUAL_INT(CONFIG_SRC_COPY_B, source);
    TEST_ASSERT_EQUAL_INT16(stored.al1, loaded.al1);
}

/* @verifies REQ-CFG-001 */
static void test_config_flipped_bit_in_each_value_byte_is_caught(void)
{
    config_t stored = sample_config();
    config_t loaded;
    config_source_t source = CONFIG_SRC_COPY_A;
    uint16_t offset;

    /* One flipped bit in any of the eight value bytes of copy A: never used. */
    for (offset = 0u; offset < (CONFIG_COPY_SIZE - 2u); offset++) {
        hal_eeprom_fake_reset();
        TEST_ASSERT_EQUAL_INT(GG_OK, config_store(&stored));
        flip_stored_bit((uint16_t)(CONFIG_ADDR_A + offset), 0x80u);
        TEST_ASSERT_EQUAL_INT(GG_OK, config_load(&loaded, &source));
        TEST_ASSERT_EQUAL_INT(CONFIG_SRC_COPY_B, source);
        TEST_ASSERT_EQUAL_INT16(stored.al2, loaded.al2);
    }
}

/* @verifies REQ-CFG-002 */
static void test_config_copy_a_wins_when_both_copies_are_valid_but_differ(void)
{
    config_t older = sample_config();
    config_t newer = sample_config();
    config_t loaded;
    config_source_t source = CONFIG_SRC_DEFAULTS;

    newer.al1 = 120;
    TEST_ASSERT_EQUAL_INT(GG_OK, write_raw_copy(CONFIG_ADDR_A, &newer));
    TEST_ASSERT_EQUAL_INT(GG_OK, write_raw_copy(CONFIG_ADDR_B, &older));
    TEST_ASSERT_EQUAL_INT(GG_OK, config_load(&loaded, &source));
    TEST_ASSERT_EQUAL_INT(CONFIG_SRC_COPY_A, source);
    TEST_ASSERT_EQUAL_INT16(120, loaded.al1);
}

/* @verifies REQ-CFG-002 */
static void test_config_both_copies_damaged_give_the_defaults_not_a_half_copy(void)
{
    config_t stored = sample_config();
    config_t loaded;
    config_source_t source = CONFIG_SRC_COPY_A;

    TEST_ASSERT_EQUAL_INT(GG_OK, config_store(&stored));
    flip_stored_bit(CONFIG_ADDR_A, 0x01u);
    flip_stored_bit(CONFIG_ADDR_B, 0x01u);
    TEST_ASSERT_EQUAL_INT(GG_OK, config_load(&loaded, &source));
    TEST_ASSERT_EQUAL_INT(CONFIG_SRC_DEFAULTS, source);
    TEST_ASSERT_EQUAL_INT16(CONFIG_AL1_DEFAULT, loaded.al1);
    TEST_ASSERT_EQUAL_INT16(CONFIG_AL2_DEFAULT, loaded.al2);
    TEST_ASSERT_EQUAL_INT16(CONFIG_HYSTERESIS_DEFAULT, loaded.hysteresis);
    TEST_ASSERT_EQUAL_UINT16(CONFIG_WARMUP_DEFAULT_S, loaded.warmup_s);
}

/* @verifies REQ-CFG-003 */
static void test_config_extreme_valid_values_survive_a_store_and_load(void)
{
    config_t stored = sample_config();
    config_t loaded;
    config_source_t source = CONFIG_SRC_DEFAULTS;

    stored.al1 = CONFIG_AL2_MAX - 1;
    stored.al2 = CONFIG_AL2_MAX;
    stored.hysteresis = (int16_t)(stored.al1 - 1);
    stored.warmup_s = CONFIG_WARMUP_MAX_S;
    TEST_ASSERT_EQUAL_INT(GG_OK, config_store(&stored));
    TEST_ASSERT_EQUAL_INT(GG_OK, config_load(&loaded, &source));
    TEST_ASSERT_EQUAL_INT(CONFIG_SRC_COPY_A, source);
    TEST_ASSERT_EQUAL_INT16(stored.al1, loaded.al1);
    TEST_ASSERT_EQUAL_INT16(stored.al2, loaded.al2);
    TEST_ASSERT_EQUAL_INT16(stored.hysteresis, loaded.hysteresis);
    TEST_ASSERT_EQUAL_UINT16(stored.warmup_s, loaded.warmup_s);
}

/* @verifies REQ-CFG-003 */
static void test_config_copy_with_a_valid_crc_and_warmup_out_of_range_is_not_used(void)
{
    config_t bad = sample_config();
    config_t good = sample_config();
    config_t loaded;
    config_source_t source = CONFIG_SRC_DEFAULTS;

    bad.warmup_s = CONFIG_WARMUP_MAX_S + 1u;
    TEST_ASSERT_EQUAL_INT(GG_OK, write_raw_copy(CONFIG_ADDR_A, &bad));
    TEST_ASSERT_EQUAL_INT(GG_OK, write_raw_copy(CONFIG_ADDR_B, &good));
    TEST_ASSERT_EQUAL_INT(GG_OK, config_load(&loaded, &source));
    TEST_ASSERT_EQUAL_INT(CONFIG_SRC_COPY_B, source);
    TEST_ASSERT_EQUAL_UINT16(good.warmup_s, loaded.warmup_s);
}

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
    RUN_TEST(test_config_failed_repair_keeps_the_valid_copy_in_use);
    RUN_TEST(test_config_null_arguments_are_rejected);
    RUN_TEST(test_config_validate_al2_one_step_above_al1_is_accepted);
    RUN_TEST(test_config_validate_negative_thresholds_are_rejected);
    RUN_TEST(test_config_validate_accepts_the_widest_valid_settings);
    RUN_TEST(test_config_validate_hysteresis_one_below_al1_is_accepted);
    RUN_TEST(test_config_store_writes_two_identical_copies);
    RUN_TEST(test_config_flipped_bit_in_a_crc_byte_is_caught);
    RUN_TEST(test_config_flipped_bit_in_each_value_byte_is_caught);
    RUN_TEST(test_config_copy_a_wins_when_both_copies_are_valid_but_differ);
    RUN_TEST(test_config_both_copies_damaged_give_the_defaults_not_a_half_copy);
    RUN_TEST(test_config_extreme_valid_values_survive_a_store_and_load);
    RUN_TEST(test_config_copy_with_a_valid_crc_and_warmup_out_of_range_is_not_used);
    return UNITY_END();
}
