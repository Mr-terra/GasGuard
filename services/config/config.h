/**
 * @file config.h
 * @brief Configuration: two EEPROM copies (A and B), each with a CRC-16, and safe defaults.
 *
 * Load takes the first valid copy (A, then B), repairs the other one, and falls
 * back to the compiled-in defaults when neither copy is valid. Store validates
 * the values first, then writes copy A and copy B. A values are in 0.1 %LEL
 * (REQ-ALM-005), the warm-up in seconds (REQ-CFG-004).
 *
 * @req REQ-CFG-001 REQ-CFG-002 REQ-CFG-003 REQ-CFG-004
 */
#ifndef CONFIG_H
// DEV-02: include guard, not an unused macro
// cppcheck-suppress misra-c2012-2.5
#define CONFIG_H

#include <stdint.h>

#include "common/gg_types/gg_types.h"

/** @brief Size of one stored copy: four 16-bit fields and a 16-bit CRC. */
#define CONFIG_COPY_SIZE 10u
/** @brief EEPROM address of copy A and of copy B. */
#define CONFIG_ADDR_A 0u
#define CONFIG_ADDR_B CONFIG_COPY_SIZE

/** @brief Compiled-in defaults (REQ-CFG-002, REQ-CFG-004). */
#define CONFIG_AL1_DEFAULT       100
#define CONFIG_AL2_DEFAULT       250
#define CONFIG_HYSTERESIS_DEFAULT 20
#define CONFIG_WARMUP_DEFAULT_S  120u

/** @brief Limits of the stored values (REQ-ALM-005, REQ-CFG-004). */
#define CONFIG_AL2_MAX       600
#define CONFIG_WARMUP_MIN_S  30u
#define CONFIG_WARMUP_MAX_S  600u

/** @brief Thresholds, hysteresis and warm-up as stored in each copy. */
typedef struct {
    int16_t al1;         /**< ALARM_1 threshold, 0.1 %LEL. */
    int16_t al2;         /**< ALARM_2 threshold, 0.1 %LEL. */
    int16_t hysteresis;  /**< Hysteresis, 0.1 %LEL. */
    uint16_t warmup_s;   /**< Warm-up duration, seconds. */
} config_t;

/** @brief Where the configuration returned by config_load() came from. */
typedef enum {
    CONFIG_SRC_COPY_A,   /**< Copy A was valid. */
    CONFIG_SRC_COPY_B,   /**< Copy A was invalid, copy B was valid. */
    CONFIG_SRC_DEFAULTS  /**< Neither copy was valid: compiled-in defaults (log this). */
} config_source_t;

/**
 * @brief Fill a configuration with the compiled-in defaults.
 * @param cfg  Receives the defaults. Must not be NULL.
 * @return GG_OK, or GG_ERR_NULL.
 * @req REQ-CFG-004
 */
// DEV-05: service API, called by app/ from Day 4; cppcheck sees one caller
// cppcheck-suppress misra-c2012-8.7
gg_status_t config_defaults(config_t *cfg);

/**
 * @brief Check a configuration against the stored-value rules.
 *
 * Rules: 0 < al1 < al2 <= 600, 0 <= hysteresis < al1, 30 <= warmup_s <= 600.
 * @param cfg  Configuration to check. Must not be NULL.
 * @return GG_OK, GG_ERR_NULL, or GG_ERR_RANGE if a rule is broken.
 * @req REQ-CFG-003 REQ-ALM-005
 */
// DEV-05: service API, called by app/ from Day 4; cppcheck sees one caller
// cppcheck-suppress misra-c2012-8.7
gg_status_t config_validate(const config_t *cfg);

/**
 * @brief Load the configuration from the first valid EEPROM copy.
 *
 * If one copy is invalid and the other is valid, the invalid copy is rewritten
 * from the valid one. If the repair write fails, the configuration is still
 * returned, and the write error is returned.
 *
 * @param cfg         Receives the configuration. Must not be NULL.
 * @param out_source  Receives where it came from. Must not be NULL.
 * @return GG_OK, GG_ERR_NULL, or the EEPROM error of a repair write.
 * @req REQ-CFG-001 REQ-CFG-002 REQ-CFG-003
 */
// DEV-05: service API, called by app/ from Day 4; cppcheck sees one caller
// cppcheck-suppress misra-c2012-8.7
gg_status_t config_load(config_t *cfg, config_source_t *out_source);

/**
 * @brief Validate a configuration, then write copy A and then copy B.
 * @param cfg  Configuration to store. Must not be NULL.
 * @return GG_OK, GG_ERR_NULL, GG_ERR_RANGE (nothing written), or the EEPROM error.
 * @req REQ-CFG-001 REQ-CFG-003
 */
// DEV-05: service API, called by app/ from Day 4; cppcheck sees one caller
// cppcheck-suppress misra-c2012-8.7
gg_status_t config_store(const config_t *cfg);

#endif /* CONFIG_H */
