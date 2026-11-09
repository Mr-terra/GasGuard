/**
 * @file button.h
 * @brief Debounce the push button (D2, active low): a level counts only once it is stable.
 *
 * The caller reads the raw level from the GPIO HAL and passes it in with the
 * current time. This keeps the logic free of hardware and testable on the PC.
 *
 * @req REQ-HMI-007
 */
#ifndef BUTTON_H
// DEV-02: include guard, not an unused macro
// cppcheck-suppress misra-c2012-2.5
#define BUTTON_H

#include <stdbool.h>
#include <stdint.h>

#include "common/gg_types/gg_types.h"

/** @brief A new level must stay unchanged for this long (REQ-HMI-007). */
#define BUTTON_DEBOUNCE_MS 30u

/** @brief Debounce state of one button. Owned by the caller. */
typedef struct {
    bool stable;      /**< Debounced state: true when pressed. */
    bool candidate;   /**< Last raw level seen. */
    uint32_t since_ms; /**< Time at which the candidate level first appeared. */
} button_t;

/**
 * @brief Reset a button to "released", with no pending change.
 * @param button  Debounce state to reset. Must not be NULL.
 * @return GG_OK, or GG_ERR_NULL if button is NULL.
 * @req REQ-HMI-007
 */
gg_status_t button_init(button_t *button);

/**
 * @brief Feed one raw sample and get the debounced state.
 *
 * The debounced state changes only when the raw level has been the same for
 * BUTTON_DEBOUNCE_MS milliseconds. Time is unsigned and may wrap around.
 *
 * @param button       Debounce state. Must not be NULL.
 * @param raw_pressed  Raw level: true when the button reads pressed.
 * @param now_ms       Current time in milliseconds.
 * @param out_pressed  Receives the debounced state. Must not be NULL.
 * @return GG_OK, or GG_ERR_NULL if a pointer is NULL.
 * @req REQ-HMI-007
 */
gg_status_t button_step(button_t *button, bool raw_pressed, uint32_t now_ms, bool *out_pressed);

#endif /* BUTTON_H */
