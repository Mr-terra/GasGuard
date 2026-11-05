/**
 * @file buzzer.h
 * @brief Switch the audible alarm output on D6.
 *
 * This driver only turns the buzzer on and off. The alarm patterns
 * (ALARM_1 at 1 Hz, ALARM_2 at 4 Hz, FAULT once every 5 s) are timed by the
 * application on Day 4 (REQ-HMI-003).
 *
 * @req REQ-HMI-003
 */
#ifndef BUZZER_H
// DEV-02: include guard, not an unused macro
// cppcheck-suppress misra-c2012-2.5
#define BUZZER_H

#include <stdbool.h>

#include "common/gg_types/gg_types.h"

/**
 * @brief Silence the buzzer.
 * @return GG_OK on success, or the error returned by the GPIO HAL.
 * @req REQ-HMI-003
 */
// DEV-05: driver API, called by app/ from Day 4; cppcheck sees no caller yet
// cppcheck-suppress misra-c2012-8.7
gg_status_t buzzer_init(void);

/**
 * @brief Sound or silence the buzzer.
 * @param on  true to sound the buzzer, false to silence it.
 * @return GG_OK on success, or the error returned by the GPIO HAL.
 * @req REQ-HMI-003
 */
// DEV-05: driver API, called by app/ from Day 4; cppcheck sees no caller yet
// cppcheck-suppress misra-c2012-8.7
gg_status_t buzzer_set(bool on);

#endif /* BUZZER_H */
