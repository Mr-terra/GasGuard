/**
 * @file stack_fake.h
 * @brief Host-only control API of the stack watermark fake (PC tests only, never on AVR).
 */
#ifndef HAL_STACK_FAKE_H
// DEV-02: include guard, not an unused macro
// cppcheck-suppress misra-c2012-2.5
#define HAL_STACK_FAKE_H

#include <stdint.h>

/**
 * @brief Restore the fake: the stack is reported as 1024 bytes free.
 * @req REQ-SAF-009
 */
// DEV-04: HAL interface or test control API, one definition on the PC
// cppcheck-suppress misra-c2012-8.7
void hal_stack_fake_reset(void);

/**
 * @brief Script the free stack that the next hal_stack_free_bytes() reports.
 * @param free_bytes  Free bytes to report.
 * @req REQ-SAF-009
 */
// DEV-04: HAL interface or test control API, one definition on the PC
// cppcheck-suppress misra-c2012-8.7
void hal_stack_fake_set_free(uint16_t free_bytes);

#endif /* HAL_STACK_FAKE_H */
