/**
 * @file config.c
 * @brief Configuration copies in EEPROM, with CRC-16 and repair.
 *
 * How it works
 * ------------
 * A copy is ten bytes: al1, al2, hysteresis and warm-up as little-endian 16-bit
 * fields, then the CRC-16 of those eight bytes. Copy A is at address 0, copy B
 * right after it. Load reads both, keeps only copies whose CRC and values are
 * correct, and uses A when it is valid. A copy that is wrong is rewritten from
 * the good one, so a single corrupted copy is repaired on the next boot. If
 * neither copy is valid, the compiled-in defaults are used and reported. Store
 * validates first, so an invalid configuration is never written.
 *
 * @req REQ-CFG-001 REQ-CFG-002 REQ-CFG-003 REQ-CFG-004
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "common/gg_types/gg_types.h"
#include "hal/eeprom/eeprom.h"
#include "services/config/config.h"
#include "services/crc16/crc16.h"

/** @brief Byte offsets of the fields inside a copy. */
#define CONFIG_OFS_AL1        0u
#define CONFIG_OFS_AL2        2u
#define CONFIG_OFS_HYSTERESIS 4u
#define CONFIG_OFS_WARMUP     6u
#define CONFIG_OFS_CRC        8u
/** @brief Number of bytes covered by the CRC. */
#define CONFIG_CRC_COVERED    CONFIG_OFS_CRC

/** @brief Byte helpers for the little-endian fields. */
#define CONFIG_BYTE_MASK  0xFFu
#define CONFIG_BYTE_SHIFT 8u

/**
 * @brief Store a 16-bit value as two little-endian bytes.
 * @param bytes  Copy buffer.
 * @param ofs    Offset of the low byte.
 * @param value  Value to store.
 */
static void put_u16(uint8_t *bytes, uint8_t ofs, uint16_t value)
{
    bytes[ofs] = (uint8_t)(value & CONFIG_BYTE_MASK);
    bytes[ofs + 1u] = (uint8_t)(value >> CONFIG_BYTE_SHIFT);
}

/**
 * @brief Read a 16-bit little-endian value.
 * @param bytes  Copy buffer.
 * @param ofs    Offset of the low byte.
 * @return The value.
 */
static uint16_t get_u16(const uint8_t *bytes, uint8_t ofs)
{
    return (uint16_t)((uint16_t)bytes[ofs] | (uint16_t)((uint16_t)bytes[ofs + 1u] << CONFIG_BYTE_SHIFT));
}

/**
 * @brief Read a 16-bit field as a signed value, without implementation-defined conversion.
 * @param bytes  Copy buffer.
 * @param ofs    Offset of the low byte.
 * @return The value, in -32768 .. 32767.
 */
static int16_t get_i16(const uint8_t *bytes, uint8_t ofs)
{
    int32_t value = (int32_t)get_u16(bytes, ofs);

    if (value > INT16_MAX) {
        value -= (int32_t)UINT16_MAX + 1;
    }
    return (int16_t)value;
}

/**
 * @brief Encode a configuration into one copy, with its CRC.
 * @param cfg    Configuration to encode.
 * @param bytes  Receives the CONFIG_COPY_SIZE bytes.
 * @return GG_OK, or the CRC error (cannot happen with a valid buffer).
 */
static gg_status_t encode_copy(const config_t *cfg, uint8_t *bytes)
{
    uint16_t crc = 0u;
    gg_status_t status = GG_OK;

    put_u16(bytes, CONFIG_OFS_AL1, (uint16_t)cfg->al1);
    put_u16(bytes, CONFIG_OFS_AL2, (uint16_t)cfg->al2);
    put_u16(bytes, CONFIG_OFS_HYSTERESIS, (uint16_t)cfg->hysteresis);
    put_u16(bytes, CONFIG_OFS_WARMUP, cfg->warmup_s);
    status = crc16_compute(bytes, CONFIG_CRC_COVERED, &crc);
    put_u16(bytes, CONFIG_OFS_CRC, crc);
    return status;
}

/**
 * @brief Decode one copy and check its CRC and its values.
 * @param bytes  The CONFIG_COPY_SIZE bytes read from EEPROM.
 * @param cfg    Receives the decoded configuration, only when the copy is valid.
 * @return true when the CRC matches and the values pass config_validate().
 */
static bool decode_copy(const uint8_t *bytes, config_t *cfg)
{
    uint16_t crc = 0u;
    bool valid = false;
    config_t candidate;

    if (crc16_compute(bytes, CONFIG_CRC_COVERED, &crc) == GG_OK) {
        if (crc == get_u16(bytes, CONFIG_OFS_CRC)) {
            candidate.al1 = get_i16(bytes, CONFIG_OFS_AL1);
            candidate.al2 = get_i16(bytes, CONFIG_OFS_AL2);
            candidate.hysteresis = get_i16(bytes, CONFIG_OFS_HYSTERESIS);
            candidate.warmup_s = get_u16(bytes, CONFIG_OFS_WARMUP);
            if (config_validate(&candidate) == GG_OK) {
                *cfg = candidate;
                valid = true;
            }
        }
    }
    return valid;
}

/**
 * @brief Read one copy from EEPROM and decode it.
 * @param addr   Copy address.
 * @param bytes  Receives the raw copy bytes.
 * @param cfg    Receives the configuration when the copy is valid.
 * @return true when the copy was read and is valid.
 */
static bool read_valid_copy(uint16_t addr, uint8_t *bytes, config_t *cfg)
{
    bool valid = false;

    if (hal_eeprom_read(addr, bytes, CONFIG_COPY_SIZE) == GG_OK) {
        valid = decode_copy(bytes, cfg);
    }
    return valid;
}

/**
 * @brief Fill a configuration with the compiled-in defaults.
 * @req REQ-CFG-004
 */
gg_status_t config_defaults(config_t *cfg)
{
    gg_status_t status = GG_OK;

    if (cfg == NULL) {
        status = GG_ERR_NULL;
    } else {
        cfg->al1 = CONFIG_AL1_DEFAULT;
        cfg->al2 = CONFIG_AL2_DEFAULT;
        cfg->hysteresis = CONFIG_HYSTERESIS_DEFAULT;
        cfg->warmup_s = CONFIG_WARMUP_DEFAULT_S;
    }
    return status;
}

/**
 * @brief Check a configuration against the stored-value rules.
 * @req REQ-CFG-003 REQ-ALM-005
 */
gg_status_t config_validate(const config_t *cfg)
{
    gg_status_t status = GG_OK;

    if (cfg == NULL) {
        status = GG_ERR_NULL;
    } else if ((cfg->al1 <= 0) || (cfg->al1 >= cfg->al2) || (cfg->al2 > CONFIG_AL2_MAX) ||
               (cfg->hysteresis < 0) || (cfg->hysteresis >= cfg->al1) ||
               (cfg->warmup_s < CONFIG_WARMUP_MIN_S) || (cfg->warmup_s > CONFIG_WARMUP_MAX_S)) {
        status = GG_ERR_RANGE;
    } else {
        status = GG_OK;
    }
    return status;
}

/**
 * @brief Load the configuration from the first valid EEPROM copy.
 * @req REQ-CFG-001 REQ-CFG-002 REQ-CFG-003
 */
/**
 * @brief Rewrite the damaged copy from the valid one (REQ-CFG-002).
 *
 * A failed repair is not fatal: the valid copy is in use, and the next boot
 * tries the repair again. The boot must not stop for it (fail-safe: the
 * detector keeps working with the thresholds it has).
 *
 * @param addr  Address of the damaged copy.
 * @param copy  Bytes of the valid copy.
 */
static void repair_copy(uint16_t addr, const uint8_t *copy)
{
    if (hal_eeprom_write(addr, copy, CONFIG_COPY_SIZE) != GG_OK) {
        /* A failed repair is not fatal: the valid copy is already in use. */
    }
}

gg_status_t config_load(config_t *cfg, config_source_t *out_source)
{
    gg_status_t status = GG_OK;

    if ((cfg == NULL) || (out_source == NULL)) {
        status = GG_ERR_NULL;
    } else {
        uint8_t copy_a[CONFIG_COPY_SIZE];
        uint8_t copy_b[CONFIG_COPY_SIZE];
        config_t cfg_a;
        config_t cfg_b;
        bool valid_a;
        bool valid_b;

        valid_a = read_valid_copy(CONFIG_ADDR_A, copy_a, &cfg_a);
        valid_b = read_valid_copy(CONFIG_ADDR_B, copy_b, &cfg_b);
        if (valid_a) {
            *cfg = cfg_a;
            *out_source = CONFIG_SRC_COPY_A;
            if (!valid_b) {
                repair_copy(CONFIG_ADDR_B, copy_a);
            }
        } else if (valid_b) {
            *cfg = cfg_b;
            *out_source = CONFIG_SRC_COPY_B;
            repair_copy(CONFIG_ADDR_A, copy_b);
        } else {
            status = config_defaults(cfg);
            *out_source = CONFIG_SRC_DEFAULTS;
        }
    }
    return status;
}

/**
 * @brief Validate a configuration, then write copy A and then copy B.
 * @req REQ-CFG-001 REQ-CFG-003
 */
gg_status_t config_store(const config_t *cfg)
{
    gg_status_t status = config_validate(cfg);
    uint8_t copy[CONFIG_COPY_SIZE];

    if (status == GG_OK) {
        status = encode_copy(cfg, copy);
    }
    if (status == GG_OK) {
        status = hal_eeprom_write(CONFIG_ADDR_A, copy, CONFIG_COPY_SIZE);
    }
    if (status == GG_OK) {
        status = hal_eeprom_write(CONFIG_ADDR_B, copy, CONFIG_COPY_SIZE);
    }
    return status;
}
