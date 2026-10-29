/**
 * @file gg_types.h
 * @brief Status codes and unit conventions shared by every GasGuard module.
 *
 * Concentrations are stored as signed 16-bit integers in tenths of a percent
 * of the Lower Explosive Limit (0.1 %LEL). No floating point is used anywhere
 * in the application layers (ADR-6).
 *
 * @req REQ-FUN-003 REQ-NFR-002
 */
#ifndef GG_TYPES_H
#define GG_TYPES_H

#include <stdint.h>

/**
 * @brief Result of a public function. Zero means success; any other value
 *        names the failure (coding standard R17).
 */
typedef enum {
    GG_OK = 0,
    GG_ERR_NULL,
    GG_ERR_RANGE,
    GG_ERR_CRC,
    GG_ERR_TIMEOUT,
    GG_ERR_HW
} gg_status_t;

/** @brief Gas concentration in units of 0.1 %LEL. */
typedef int16_t gg_conc_t;

/** @brief Number of concentration units in 1 %LEL. */
#define GG_CONC_SCALE 10

/** @brief Upper bound of an alarm threshold: 60 %LEL (REQ-ALM-005). */
#define GG_CONC_MAX 600

#endif /* GG_TYPES_H */
