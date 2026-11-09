/**
 * @file stack_fake.c
 * @brief Host fake of the stack watermark: the test scripts the free stack.
 *
 * @req REQ-SAF-009
 */
#include <stddef.h>
#include <stdint.h>

#include "hal/stack/stack.h"
#include "hal/stack/stack_fake.h"

/** @brief Free stack reported by the fake, in bytes. */
#define STACK_FAKE_DEFAULT_FREE 1024u

static uint16_t fake_free = STACK_FAKE_DEFAULT_FREE;

void hal_stack_fake_reset(void)
{
    fake_free = STACK_FAKE_DEFAULT_FREE;
}

void hal_stack_fake_set_free(uint16_t free_bytes)
{
    fake_free = free_bytes;
}

gg_status_t hal_stack_paint(void)
{
    return GG_OK;
}

gg_status_t hal_stack_free_bytes(uint16_t *out_free)
{
    gg_status_t status = GG_OK;

    if (out_free == NULL) {
        status = GG_ERR_NULL;
    } else {
        *out_free = fake_free;
    }
    return status;
}
