/**
 * @file relay.h
 * @brief Drive the alarm relay coil on D5 (normally-energised output).
 *
 * The coil is energised in the healthy state and de-energised for an alarm,
 * a fault, or a power loss (REQ-SAF-001). relay_init() puts the coil in the
 * de-energised state first, so a reset never leaves the output in the
 * healthy state by accident.
 *
 * @req REQ-SAF-001
 */
#ifndef RELAY_H
// DEV-02: include guard, not an unused macro
// cppcheck-suppress misra-c2012-2.5
#define RELAY_H

#include <stdbool.h>

#include "common/gg_types/gg_types.h"

/**
 * @brief Set the relay coil to the de-energised state.
 * @return GG_OK on success, or the error returned by the GPIO HAL.
 * @req REQ-SAF-001
 */
// DEV-05: driver API, called by app/ from Day 4; cppcheck sees no caller yet
// cppcheck-suppress misra-c2012-8.7
gg_status_t relay_init(void);

/**
 * @brief Energise or de-energise the relay coil.
 * @param energised  true for the healthy state, false for an alarm or fault.
 * @return GG_OK on success, or the error returned by the GPIO HAL.
 * @req REQ-SAF-001
 */
// DEV-05: driver API, called by app/ from Day 4; cppcheck sees no caller yet
// cppcheck-suppress misra-c2012-8.7
gg_status_t relay_set(bool energised);

#endif /* RELAY_H */
