/**
 * @file channel_select.h
 * @brief Choose the concentration reported for the whole detector.
 *
 * The reported value is the maximum of the channels that are still valid
 * (REQ-FUN-005). Taking the maximum is the conservative choice: a single
 * sensor that reads high raises the alarm, and a faulty channel never does
 * (ADR-5). This file includes no AVR header (R20).
 *
 * @req REQ-FUN-005
 */
#ifndef CHANNEL_SELECT_H
// DEV-02: include guard, not an unused macro
// cppcheck-suppress misra-c2012-2.5
#define CHANNEL_SELECT_H

#include <stdbool.h>
#include <stdint.h>

#include "common/gg_types.h"

/**
 * @brief Maximum concentration over the valid channels.
 *
 * When no channel is valid, the result is 0 and out_any_valid is false; the
 * caller must then raise FAULT (REQ-SAF-007).
 *
 * @param concs          Concentration of each channel, 0.1 %LEL. Must not be NULL.
 * @param valid          Validity of each channel. Must not be NULL.
 * @param count          Number of channels, at least 1.
 * @param out_max        Receives the maximum over the valid channels. Must not be NULL.
 * @param out_any_valid  Receives true if at least one channel is valid. Must not be NULL.
 * @return GG_OK, GG_ERR_NULL, or GG_ERR_RANGE if count is zero.
 * @req REQ-FUN-005
 */
// DEV-05: service API, called by app/ and tests; cppcheck sees one caller at this stage
// cppcheck-suppress misra-c2012-8.7
gg_status_t channel_select_max(const gg_conc_t *concs, const bool *valid, uint8_t count,
                               gg_conc_t *out_max, bool *out_any_valid);

#endif /* CHANNEL_SELECT_H */
