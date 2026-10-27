# GasGuard — Safety Analysis (FMEA)

Scale: Severity S (1–5), Occurrence O (1–5), Detection D (1–5, 5 = undetectable).
RPN = S × O × D. Target: every residual RPN ≤ 20 or justified.

## 1. Hazard

**H1:** Gas accumulates and the device does not warn (missed alarm). → worst case.
**H2:** The device warns without gas (false alarm). → loss of trust, people
start ignoring or disconnecting the detector, which leads back to H1.

## 2. FMEA

| # | Item | Failure mode | Effect | S | O | D | RPN | Mitigation | Req | Residual S/O/D → RPN |
|---|---|---|---|---|---|---|---|---|---|---|
| F01 | Gas sensor / cable | Disconnected / open circuit | Input floats: random or ~0 reading → missed alarm (H1) | 5 | 3 | 5 | 75 | 100 kΩ pull-down forces ≈ 0 when open; range check ≤ 5 LSB → channel fault | HW-001, REQ-SAF-002 | 5/3/1 → 15 |
| F02 | Gas sensor | Short circuit | Reads max → false alarm (H2) | 3 | 2 | 4 | 24 | Range check ≥ 1018 LSB → channel fault | REQ-SAF-002 | 3/2/1 → 6 |
| F03 | Gas sensor | Stuck value (heater dead, aged) | Missed alarm (H1) | 5 | 2 | 5 | 50 | Stuck-at detection over 10 min | REQ-SAF-003 | 5/2/2 → 20 |
| F04 | Gas sensor | Not warmed up | Wrong readings at boot | 4 | 5 | 3 | 60 | Warm-up period, alarms inhibited and shown | REQ-ALM-006, REQ-CFG-004 | 4/5/1 → 20 |
| F05 | Gas sensor | Drift / cross-sensitivity | Under/over reading | 4 | 3 | 4 | 48 | Two channels, max of valid; temperature compensation | REQ-FUN-004/005 | 4/3/3 → 36 *(accepted: needs periodic gas calibration, out of scope)* |
| F06 | ADC | Reference drift / ADC broken | All readings wrong | 5 | 1 | 5 | 25 | Bandgap measurement at boot + every 60 s | REQ-SAF-004 | 5/1/2 → 10 |
| F07 | Firmware | Infinite loop / hang | No monitoring at all | 5 | 2 | 5 | 50 | Watchdog 500 ms, kicked only after all tasks | REQ-SAF-005 | 5/2/1 → 10 |
| F08 | Firmware | Task overrun (timing) | Late detection | 4 | 2 | 4 | 32 | Overrun detection, FAULT after 3 | REQ-SAF-008 | 4/2/1 → 8 |
| F09 | Firmware | Stack overflow corrupts data | Unpredictable | 5 | 2 | 5 | 50 | Stack painting + watermark check; no recursion, no malloc | REQ-SAF-009/010 | 5/1/2 → 10 |
| F10 | EEPROM | Corrupted config (power cut during write) | Wrong thresholds → missed alarm | 5 | 2 | 5 | 50 | A/B copies + CRC-16, safe defaults fallback, range validation | REQ-CFG-001..003 | 5/2/1 → 10 |
| F11 | Config | Absurd thresholds (AL1 ≥ AL2, 0…) | Alarm never triggers | 5 | 2 | 4 | 40 | Validation on load and store | REQ-ALM-005, REQ-CFG-003 | 5/1/1 → 5 |
| F12 | Relay / wiring | Coil or wire broken | Output never signals | 4 | 2 | 4 | 32 | Normally-energised relay: break = alarm side; functional test | REQ-SAF-001, REQ-HMI-006 | 4/2/2 → 16 |
| F13 | Power | Brown-out / power loss | Device off | 5 | 3 | 3 | 45 | Fail-safe relay; BOD enabled 4.3 V; reset cause logged | REQ-SAF-001/006 | 5/3/1 → 15 |
| F14 | Operator | Alarm silenced and forgotten | Missed second event | 4 | 3 | 3 | 36 | ALARM_2 not silenceable > 60 s; latched | REQ-HMI-004, REQ-ALM-004 | 4/2/2 → 16 |
| F15 | Operator | Maintenance mode left on | Output inhibited | 5 | 2 | 3 | 30 | 10 min auto-exit, permanent display | REQ-HMI-009/010 | 5/1/2 → 10 |
| F16 | Noise | Single spike above threshold | False alarm (H2) | 3 | 4 | 3 | 36 | Moving average + 3-sample confirmation | REQ-FUN-002, REQ-ALM-001 | 3/2/2 → 12 |
| F17 | Noise | Value oscillating around threshold | Alarm chattering | 2 | 3 | 2 | 12 | Hysteresis + 10-sample clear | REQ-ALM-003 | 2/2/1 → 4 |
| F19 | Wiring | Grove 2nd signal wire connected to module DO | DO drives the neighbour ADC channel → wrong reading on other sensor | 4 | 2 | 4 | 32 | White wire left unconnected and insulated; documented wiring table | HW-002 | 4/1/2 → 8 |
| F20 | Power | Too many MQ heaters on USB 5 V | Supply sags → brown-out resets, wrong readings | 4 | 2 | 3 | 24 | Max 2 sensors; BOD + reset cause logging; bandgap check | HW-003, REQ-SAF-004/006 | 4/1/2 → 8 |
| F18 | Button | Bounce | Double / spurious command | 2 | 4 | 3 | 24 | 30 ms debounce, long press for safety actions | REQ-HMI-007 | 2/1/2 → 4 |

## 3. Residual risks (explicitly accepted)

- **F05** — semiconductor sensors drift; real products require periodic
  calibration with reference gas. Out of scope for a demonstrator.
- Single microcontroller, no hardware redundancy (a certified device would add
  an independent monitoring channel; see SIL discussion in README).
- No certification claim (IEC 60079-29-1, JIS, EN 50194) — standards were used
  as **design inspiration** only.
