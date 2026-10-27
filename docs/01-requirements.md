# GasGuard — Software Requirements Specification

| Field | Value |
|---|---|
| Product | GasGuard — combustible gas detector firmware (portfolio / demonstrator) |
| Target | Arduino Uno R3 (ATmega328P, 16 MHz, 32 KB flash, 2 KB SRAM, 1 KB EEPROM) |
| Language | C99, bare-metal (avr-gcc + avr-libc, no Arduino framework) |
| Status | v0.1 — draft |

> **Disclaimer.** GasGuard is a learning and demonstration project. MQ-series
> sensors are uncalibrated, low-cost parts. This device is **not** a certified
> safety product and must never be used to protect people or property.

## 1. Purpose

GasGuard monitors combustible gas concentration with two semiconductor sensors,
raises a two-level alarm, drives an alarm relay output, and detects its own
faults. The design goal is **fail-safe behaviour**: when the device cannot trust
its own measurement, it must say so and move to a safe state.

## 2. Conventions

- Each requirement has a unique ID: `REQ-<AREA>-<NNN>`.
  Areas: `FUN` (function), `ALM` (alarm), `SAF` (safety / diagnostics),
  `HMI` (user interface), `CFG` (configuration / storage), `LOG` (event log),
  `NFR` (non-functional).
- "shall" = mandatory, "should" = recommended.
- Every requirement must be traced to code (`@req` tag) and to at least one test
  (see `docs/05-verification-plan.md`).
- Concentrations are expressed in **%LEL** (percent of the Lower Explosive
  Limit), the reference unit of industrial gas detectors. Conversion from raw
  sensor value is an approximation (see REQ-FUN-003).

## 3. Operating modes

`INIT → SELF_TEST → WARMUP → NORMAL ⇄ ALARM_1 → ALARM_2 (latched)`
Any mode → `FAULT`. `NORMAL` ⇄ `MAINTENANCE` (operator request, time-limited).

## 4. Functional requirements

| ID | Requirement |
|---|---|
| REQ-FUN-001 | The firmware shall sample each gas sensor channel (MQ5 on A0, MQ2 on A1) every 100 ms. |
| REQ-FUN-002 | The firmware shall filter each channel with a moving average over the last 8 samples. |
| REQ-FUN-003 | The firmware shall convert the filtered value to %LEL using a per-channel linear calibration (zero offset + span) stored in configuration. |
| REQ-FUN-004 | The firmware should apply a linear temperature compensation using the temperature sensor on A3, when that sensor is valid. |
| REQ-FUN-005 | The reported concentration shall be the **maximum** of the valid gas channels (conservative choice). |
| REQ-FUN-006 | In simulation build mode, the potentiometer on A2 shall replace the MQ5 channel, so the full logic can be exercised without gas. |

## 5. Alarm requirements

| ID | Requirement |
|---|---|
| REQ-ALM-001 | The firmware shall enter ALARM_1 when the concentration is ≥ AL1 threshold (default 10 %LEL) for 3 consecutive filtered samples. |
| REQ-ALM-002 | The firmware shall enter ALARM_2 when the concentration is ≥ AL2 threshold (default 25 %LEL) for 3 consecutive filtered samples. |
| REQ-ALM-003 | ALARM_1 shall clear automatically when the concentration falls below AL1 − hysteresis (default 2 %LEL) for 10 consecutive samples. |
| REQ-ALM-004 | ALARM_2 shall be **latched**: it shall only clear on an operator reset, and only if the concentration is below AL1 − hysteresis. |
| REQ-ALM-005 | Threshold configuration shall be rejected unless 0 < AL1 < AL2 ≤ 60 %LEL. |
| REQ-ALM-006 | Alarm detection shall be inhibited during WARMUP; the inhibition shall be visible on the display. |

## 6. Safety and diagnostic requirements

| ID | Requirement |
|---|---|
| REQ-SAF-001 | The alarm relay shall be **energised in the healthy, no-alarm state** and de-energised in ALARM_2, FAULT, and on power loss (fail-safe / normally-energised output). |
| REQ-SAF-002 | A gas channel shall be declared faulty if its raw ADC value is ≤ 5 (open circuit / missing sensor) or ≥ 1018 (short circuit) for 5 consecutive samples. |
| REQ-SAF-003 | A gas channel shall be declared faulty if its raw value does not change by more than 1 LSB over 10 minutes outside WARMUP (stuck sensor). |
| REQ-SAF-004 | At start-up and every 60 s, the firmware shall measure the internal 1.1 V bandgap through the ADC; a result outside ±10 % shall raise an ADC fault. |
| REQ-SAF-005 | The hardware watchdog shall be enabled with a timeout of 500 ms and serviced only from the main loop once all periodic tasks have run. |
| REQ-SAF-006 | At boot, the firmware shall read the reset cause; a watchdog or brown-out reset shall be logged and displayed. |
| REQ-SAF-007 | If all gas channels are faulty, or the ADC is faulty, the firmware shall enter FAULT. FAULT shall de-energise the relay and sound the fault buzzer pattern. |
| REQ-SAF-008 | The scheduler shall detect a task overrun (cycle > 100 ms) and log it; 3 consecutive overruns shall raise FAULT. |
| REQ-SAF-009 | Unused stack shall be painted at boot; the firmware shall check the stack watermark every second and raise FAULT if fewer than 64 bytes remain. |
| REQ-SAF-010 | The firmware shall not use dynamic memory allocation or recursion. |

## 7. User interface requirements

| ID | Requirement |
|---|---|
| REQ-HMI-001 | The LCD shall show the mode on line 1 and the concentration (%LEL) on line 2, refreshed at least every 500 ms. |
| REQ-HMI-002 | The LCD backlight colour shall be green (NORMAL), yellow (ALARM_1, WARMUP, MAINTENANCE), red (ALARM_2), and blinking amber (FAULT). |
| REQ-HMI-003 | The buzzer shall use distinct patterns: ALARM_1 = slow beep (1 Hz), ALARM_2 = fast beep (4 Hz), FAULT = 1 beep every 5 s. |
| REQ-HMI-004 | A short press (< 1 s) on the button shall acknowledge (silence) the buzzer for ALARM_1; ALARM_2 shall not be silenceable for more than 60 s. |
| REQ-HMI-005 | A long press (≥ 3 s) on the button shall request ALARM_2 reset (see REQ-ALM-004). |
| REQ-HMI-006 | A press on the button in NORMAL for ≥ 3 s shall run a **functional test**: buzzer, all backlight colours, relay toggle, then return to NORMAL. |
| REQ-HMI-007 | All button inputs shall be debounced (stable for 30 ms). |

## 8. Maintenance mode

| ID | Requirement |
|---|---|
| REQ-HMI-008 | A touch on the touch sensor for ≥ 3 s in NORMAL shall enter MAINTENANCE. |
| REQ-HMI-009 | In MAINTENANCE, the relay shall hold its last healthy state, alarms shall be shown but not output, and "MAINT" shall be displayed permanently. |
| REQ-HMI-010 | MAINTENANCE shall end automatically after 10 minutes (no forgotten inhibition). |

## 9. Configuration and storage

| ID | Requirement |
|---|---|
| REQ-CFG-001 | Configuration (thresholds, hysteresis, calibration, warm-up time) shall be stored in EEPROM in two copies (A and B), each protected by a CRC-16/CCITT-FALSE. |
| REQ-CFG-002 | At boot, the firmware shall use the first valid copy, repair the other, and fall back to compiled-in safe defaults if both are invalid (and log it). |
| REQ-CFG-003 | Configuration shall be validated (ranges, REQ-ALM-005) before use and before storage. |
| REQ-CFG-004 | The default warm-up duration shall be 120 s (configurable 30–600 s). |

## 10. Event log

| ID | Requirement |
|---|---|
| REQ-LOG-001 | The firmware shall record mode transitions, faults, resets, and configuration events in a ring buffer of 32 entries in EEPROM. |
| REQ-LOG-002 | Each entry shall contain an uptime timestamp (s), an event code, and a 16-bit argument, and shall be protected by an 8-bit checksum. |
| REQ-LOG-003 | The firmware shall write the log only on events (never periodically), to limit EEPROM wear. |
| REQ-LOG-004 | The log shall be dumpable over UART (38400 baud, 8N1) on the command `log`. |

## 11. Non-functional requirements

| ID | Requirement |
|---|---|
| REQ-NFR-001 | Application logic shall not depend on AVR headers; it shall build and run on a PC for unit testing. |
| REQ-NFR-002 | The code shall comply with the project coding standard (MISRA C:2012 subset, `docs/04-coding-standard.md`); deviations shall be documented. |
| REQ-NFR-003 | The build shall be warning-free with `-Wall -Wextra -Werror` and the extra flags listed in the coding standard. |
| REQ-NFR-004 | Unit test line coverage of the `app/` and `services/` layers shall be ≥ 90 %. |
| REQ-NFR-005 | Flash usage shall stay ≤ 80 % and static RAM ≤ 60 % of the ATmega328P capacity. |
