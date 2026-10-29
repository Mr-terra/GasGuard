/**
 * @file hal_reset_cause_fake.h
 * @brief Host-only control API of the reset-cause fake (PC tests only, never on AVR).
 */
#ifndef HAL_RESET_CAUSE_FAKE_H
// DEV-02: include guard, not an unused macro
// cppcheck-suppress misra-c2012-2.5
#define HAL_RESET_CAUSE_FAKE_H

#include "hal/hal_reset_cause.h"

/**
 * @brief Report a power-on reset, as after a fresh start.
 * @req REQ-SAF-006
 */
// DEV-04: HAL interface or test control API, one definition on the PC
// cppcheck-suppress misra-c2012-8.7
void hal_reset_cause_fake_reset(void);

/**
 * @brief Script the cause that the next hal_reset_cause_get() returns.
 * @param cause  Reset cause to report.
 * @req REQ-SAF-006
 */
// DEV-04: HAL interface or test control API, one definition on the PC
// cppcheck-suppress misra-c2012-8.7
void hal_reset_cause_fake_set(hal_reset_cause_t cause);

#endif /* HAL_RESET_CAUSE_FAKE_H */
