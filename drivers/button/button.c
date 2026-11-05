/**
 * @file button.c
 * @brief Push-button debounce.
 *
 * How it works
 * ------------
 * A mechanical switch bounces for a few milliseconds when it is pressed or
 * released, so the raw level flickers. The driver remembers the last raw level
 * (candidate) and the time it appeared. The debounced state (stable) follows
 * the candidate only after the candidate has held for 30 ms. A bounce restarts
 * that 30 ms window, so a short flicker never reaches the application. The
 * time difference uses unsigned subtraction, which stays correct when the
 * millisecond counter wraps around.
 *
 * @req REQ-HMI-007
 */
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "common/gg_types/gg_types.h"
#include "drivers/button/button.h"

/**
 * @brief Reset a button to "released", with no pending change.
 *
 * The full contract is in the header.
 * @req REQ-HMI-007
 */
gg_status_t button_init(button_t *button)
{
    gg_status_t status = GG_OK;

    if (button == NULL) {
        status = GG_ERR_NULL;
    } else {
        button->stable = false;
        button->candidate = false;
        button->since_ms = 0u;
    }
    return status;
}

/**
 * @brief Feed one raw sample and get the debounced state.
 *
 * The full contract is in the header.
 * @req REQ-HMI-007
 */
gg_status_t button_step(button_t *button, bool raw_pressed, uint32_t now_ms, bool *out_pressed)
{
    gg_status_t status = GG_OK;

    if ((button == NULL) || (out_pressed == NULL)) {
        status = GG_ERR_NULL;
    } else {
        if (raw_pressed != button->candidate) {
            button->candidate = raw_pressed;
            button->since_ms = now_ms;
        }
        if ((button->candidate != button->stable) &&
            ((now_ms - button->since_ms) >= BUTTON_DEBOUNCE_MS)) {
            button->stable = button->candidate;
        }
        *out_pressed = button->stable;
    }
    return status;
}
