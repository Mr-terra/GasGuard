/**
 * @file hal_reset_cause.h
 * @brief Hardware interface: why the microcontroller last reset.
 *
 * Read once at boot, then logged and shown on the display (REQ-SAF-006).
 *
 * @req REQ-SAF-006
 */
#ifndef HAL_RESET_CAUSE_H
#define HAL_RESET_CAUSE_H

/** @brief Reset source, as reported by the MCU status register. */
typedef enum {
    HAL_RESET_POWER_ON = 0, /**< Power-on reset. */
    HAL_RESET_EXTERNAL,     /**< Reset pin (manual reset or programmer). */
    HAL_RESET_BROWN_OUT,    /**< Supply fell below the BOD level. */
    HAL_RESET_WATCHDOG,     /**< Watchdog timeout: the main loop stalled. */
    HAL_RESET_UNKNOWN       /**< Several flags set or none: treat as suspicious. */
} hal_reset_cause_t;

/**
 * @brief Return the cause of the last reset and clear the status flags.
 *
 * Must be called once, early in boot, before the watchdog is enabled.
 *
 * @return The reset cause.
 * @req REQ-SAF-006
 */
hal_reset_cause_t hal_reset_cause_get(void);

#endif /* HAL_RESET_CAUSE_H */
