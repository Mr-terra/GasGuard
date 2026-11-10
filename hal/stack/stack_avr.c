/**
 * @file stack_avr.c
 * @brief Stack watermark on the ATmega328P: paint the free RAM, then count the intact bytes.
 *
 * How it works
 * ------------
 * The linker places __heap_start just above the global data. The program uses
 * no heap, so the RAM from there up to the stack is free. At boot the bytes
 * below the stack pointer, minus a margin for the frames still to come, are
 * painted with a known value. Later, the stack grows down into the painted
 * area and overwrites some of it; counting the intact painted bytes from the
 * bottom gives the room the stack has never used (REQ-SAF-009). The margin
 * keeps the painted area clear of the frames of the current call.
 *
 * The heap start and the stack pointer are addresses, so their pointer and
 * integer conversions are the deviation DEV-09 (docs/04-coding-standard.md §5).
 *
 * @req REQ-SAF-009
 */
#include <stddef.h>
#include <stdint.h>

#include <avr/io.h>

#include "hal/stack/stack.h"

/** @brief Value written into the free RAM. */
#define STACK_PAINT_BYTE 0xC5u

/** @brief Bytes kept unpainted below the stack pointer, for the frames still to come. */
#define STACK_MARGIN_BYTES 32u

/** @brief First byte of the heap, set by the linker just above the global data. */
// DEV-09: linker symbol, defined outside this translation unit
// cppcheck-suppress misra-c2012-8.7
extern uint8_t __heap_start;

gg_status_t hal_stack_paint(void)
{
    gg_status_t status = GG_OK;
    uint16_t sp = (uint16_t)SP;
    uint16_t heap = 0u;

    // DEV-09: the address of the heap start, as an integer, to compare with the stack pointer
    // cppcheck-suppress misra-c2012-11.4
    heap = (uint16_t)(uintptr_t)&__heap_start;
    if (sp <= (uint16_t)(heap + STACK_MARGIN_BYTES)) {
        status = GG_ERR_RANGE;
    } else {
        uint16_t addr;

        for (addr = heap; addr < (uint16_t)(sp - STACK_MARGIN_BYTES); addr++) {
            // DEV-09: pointer write from an integer address
            // cppcheck-suppress misra-c2012-11.4
            *((uint8_t *)(uintptr_t)addr) = STACK_PAINT_BYTE;
        }
    }
    return status;
}

gg_status_t hal_stack_free_bytes(uint16_t *out_free)
{
    gg_status_t status = GG_OK;

    if (out_free == NULL) {
        status = GG_ERR_NULL;
    } else {
        uint16_t addr;
        uint16_t count = 0u;

        // DEV-09: the address of the heap start, as an integer, to walk the painted bytes
        // cppcheck-suppress misra-c2012-11.4
        addr = (uint16_t)(uintptr_t)&__heap_start;
        // DEV-09: read the painted bytes through the address computed above
        // cppcheck-suppress misra-c2012-11.4
        while ((*((uint8_t *)(uintptr_t)addr) == STACK_PAINT_BYTE) && (count < UINT16_MAX)) {
            addr++;
            count++;
        }
        *out_free = count;
    }
    return status;
}
