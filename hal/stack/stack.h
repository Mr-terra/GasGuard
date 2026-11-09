/**
 * @file stack.h
 * @brief Hardware interface: stack watermark, to detect a stack that grows into the data.
 *
 * At boot the free RAM between the heap start and the stack pointer is painted
 * with a known byte. Later the painted bytes that are still intact show how far
 * the stack has never gone; the number of them is the free stack (REQ-SAF-009).
 *
 * @req REQ-SAF-009
 */
#ifndef HAL_STACK_H
// DEV-02: include guard, not an unused macro
// cppcheck-suppress misra-c2012-2.5
#define HAL_STACK_H

#include <stdint.h>

#include "common/gg_types/gg_types.h"

/**
 * @brief Paint the unused RAM below the stack pointer. Call once, at boot, before the loop.
 * @return GG_OK, or GG_ERR_RANGE if the stack pointer is too low to paint.
 * @req REQ-SAF-009
 */
// DEV-04: HAL interface or test control API, one definition on the PC
// cppcheck-suppress misra-c2012-8.7
gg_status_t hal_stack_paint(void);

/**
 * @brief Number of bytes the stack has never used, counted from the heap start.
 * @param out_free  Receives the free bytes. Must not be NULL.
 * @return GG_OK, or GG_ERR_NULL if out_free is NULL.
 * @req REQ-SAF-009
 */
// DEV-04: HAL interface or test control API, one definition on the PC
// cppcheck-suppress misra-c2012-8.7
gg_status_t hal_stack_free_bytes(uint16_t *out_free);

#endif /* HAL_STACK_H */
