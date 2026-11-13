# Test report

Hardware test results, one section per bench session. Each entry gives the
date, the firmware (branch and commit), and the result of each step. Steps not
yet run are listed as open, so nothing is reported as passed by default.

## Open steps for the next bench session (Day 5 and after)

No result is recorded for these steps yet. Each one needs the board, a logic
analyser or the serial monitor.

| Step | What to check | Requirement |
|---|---|---|
| Relay at reset | Pin D5 low before the first GPIO init (FMEA F21) | REQ-SAF-001 |
| Watchdog timing | Reset after 500 ms without kick; no reset in normal running | REQ-SAF-005 |
| Stack watermark | Value of `hal_stack_free_bytes` after boot and after a minute | REQ-SAF-009 |
| Buzzer patterns | 1 Hz (ALARM_1), 4 Hz (ALARM_2), one beep every 5 s (FAULT) | REQ-HMI-003 |
| Button | Short press silences ALARM_1; long press resets ALARM_2; long press in NORMAL starts the test | REQ-HMI-004..006 |
| Touch | 3 s touch enters MAINTENANCE, the display shows MAINT | REQ-HMI-008, REQ-HMI-009 |
| LCD | Line 2 shows the concentration, FAULT blinks amber | REQ-HMI-001, REQ-HMI-002 |
| `log` command | Dump over the UART at 38400 baud, complete, with the reset and config entries | REQ-LOG-004 |
| Bandgap | Raw value of the bandgap against AVcc (expected near 0xED at 4.75 V) | REQ-SAF-004 |
| Second MQ sensor | Wiring and reading on A1 | REQ-FUN-001 |
| MQ5 baseline | Clean-air voltage on A0 once it is stable, after 24 to 48 h of continuous power (datasheet preheat). Then fix `zero_raw` from it. Not yet measured: the voltage was still drifting after about 4 h | REQ-FUN-003, REQ-SAF-003 |
| Watchdog reset cause | After a watchdog reset, WDRF is seen (the bootloader can clear the reset flags first) | REQ-SAF-006 |
| Stuck sensor | Freeze one input for 10 minutes: its channel is excluded and the alarm clears | REQ-SAF-003 |
| Bandgap | Measured at start-up and every 60 s; the value is within 202..248 | REQ-SAF-004 |
| Display time | Longest display task on the board (I2C): stays well under 100 ms per cycle | REQ-SAF-008 |
| UART input | Paste a 16-character command: no byte lost | REQ-LOG-004 |
| Relay pull-down | Relay state at reset, with and without the proposed 10 kΩ pull-down | REQ-SAF-001, FMEA F21 |
| Buzzer rate | Alarm 2 measured on the buzzer pin: 4 beeps per second | REQ-HMI-003 |
| Simulation build | `make avr-sim`: the potentiometer on A2 drives the MQ-5 channel | REQ-FUN-006 |
| Calibration in the configuration | REQ-FUN-003 is **only partly met**: the zero offsets are compiled in (`FIRMWARE_ZERO_RAW_*`), not stored in the configuration, and the span is not checked with a reference gas. The trace shows code and tests for REQ-FUN-003, not this gap | REQ-FUN-003 |

## Day 5 — full firmware on the board (2026-11-13)

Firmware: `main` at `60ac2b2` (button read as active high), flashed with
`tools/flash-monitor.ps1`. Supply: 9 V jack, 1 A, and USB. Both MQ sensors wired.

| Check | Result | Requirement |
|---|---|---|
| Supply under load | 5V-GND 5.12 to 5.15 V and AREF-GND 5.15 V, both heaters on | HW-003 |
| `log` command | Pass: the dump works. The line must end with `\n` (a `\r` alone is ignored); the first line after opening the port reads `ERR` (stray byte at port open) | REQ-LOG-004 |
| Event log across resets | Pass: entries kept over 8 boots, the ring wraps after 32 | REQ-LOG-001 |
| Reset cause | Logged as `4` (unknown) at every boot: the bootloader clears the flags (F23) | REQ-SAF-006 |
| Configuration | `CONFIG 2` (defaults): the EEPROM was never written | REQ-CFG-002 |
| Display missing at boot | Pass: with SCL unplugged, `DISPLAY 0` is logged and the detector runs | REQ-HMI-001 |
| Channel fault | Pass: MQ-2 wire off, `CHANNEL 1` logged, the detector goes on with the MQ-5 and reaches ALARM_1 after the warm-up (uncalibrated MQ-5 reads about 11.5 %LEL) | REQ-SAF-002 |
| Bandgap check | **Fail (false fault)** with `60ac2b2`: `FAULT 1` at 0 s on 6 boots and at 61 s on one, with a correct supply and no channel fault. **Pass** with `day-5-fix-bench` (`0648f81`, ten conversions discarded): 6 boots without `FAULT` or `BANDGAP`, and one run past 120 s (so through the 60 s check) | REQ-SAF-004, F43 |
| LCD in FAULT | Amber blink seen. **Fail** with `60ac2b2`: line 1 read `FAULTP` (end of `WARMUP` kept). **Pass** with `e5435f1` (rows filled with spaces): `ALARM 1` shown with no extra letter | REQ-HMI-001 |
| FAULT buzzer | Pass: one beep every 5 s; the short press does not silence it (by design, REQ-HMI-004 covers ALARM_1 only) | REQ-HMI-003 |
| Warm-up end | Pass: `MODE 1` (NORMAL) then `MODE 2` (ALARM_1) at 120 s, with the uncalibrated MQ-5 at about 11.5 %LEL | REQ-ALM-006 |
| Button short press in ALARM_1 | Pass: the buzzer stops, the display stays on `ALARM 1` | REQ-HMI-004 |
| Button long press in ALARM_1 | Pass: 3 s press, no change of state or buzzer (by design: the long press acts only in ALARM_2 and NORMAL) | REQ-HMI-005 |
| Calibrated start (`298db0b`) | Pass: after the warm-up the detector reaches NORMAL with the MQ-5 offset at 113 | REQ-FUN-003 |
| Touch 3 s in NORMAL | Pass: MAINTENANCE entered, yellow backlight (looks orange on this module), automatic return to NORMAL after 10 min | REQ-HMI-008, REQ-HMI-010 |
| Button long press in NORMAL | Pass: the functional test runs, the buzzer beeps. Relay step not seen: no relay wired | REQ-HMI-006 |
| ALARM_2 with lighter gas (unlit) on the MQ-5 | Pass: up to 51.6 %LEL, ALARM_2 entered, buzzer 4 beeps per second. Backlight colour not reported | REQ-ALM-002, REQ-HMI-003 |
| ALARM_2 short press | Pass: the buzzer stops, then sounds again after 60 s | REQ-HMI-004 |
| ALARM_2 latch and reset | Pass: the state stays ALARM_2 after the gas is gone; a 3 s press between 8 and 10 %LEL is refused (above AL1 − hysteresis); the same press below 8 %LEL resets the latch | REQ-ALM-004, REQ-HMI-005 |
| Unexplained channel event | With `60ac2b2`, `CHANNEL 0` at 832 s then `RECOVERED 0` at 1111 s (MQ-5). A cable was replaced during that session, maybe at that time; not confirmed. The detector reacted as designed (channel excluded, then back in service). **Open**: watch for it again with the wiring untouched | REQ-SAF-002 |

## Day 3 — first bench session (2026-11-05)

Firmware: bring-up program on branch `day-3-bringup` (commit `d577f30` and later),
with the I²C STOP fix from `day-3-fix-i2c-stop` (`2d0d056`).

### Power

| Check | Setup | Result |
|---|---|---|
| 5V-GND on the Uno, USB port | Only USB | 4.53 V (other USB port: same) — below the 4.75 V USB minimum |
| 5V-GND on the breadboard rail | Wall adapter, no sensor | 4.75 V |
| 5V-GND with the MQ5 heater | Wall adapter, MQ5 on | **4.50 V** — the supply sags by 0.25 V with one heater |
| Supply for the Uno | 9 V barrel jack, 1 A | Fitted (Day 5): 5.12 to 5.15 V on the 5V pin with both heaters on |

### Hardware pull-downs and inputs (HW-001)

| Check | Result |
|---|---|
| A0 to GND with the 100 kΩ pull-down, no sensor | 0 V |
| A1 to GND with the 100 kΩ pull-down, no sensor | 0 V |
| A0 with the MQ5 AO wire removed | 0 V |
| A0 with the MQ5 AO wire connected | Responds. Still drifting during the preheat: 1.41 V at 13:57 the first day, 0.63 V at 18:00, 0.71 V at 23:09 (night), 0.56 V at 11:08 and 12:10 next day, 0.52 to 0.55 V from 15:26 to 16:38. Temperature recorded from 11:08 the next day: 17.3 to 20.4 °C. Earlier readings were at about 20 °C (estimate, not recorded) |
| MQ5 baseline (provisional) | **Provisional, not validated**: 0.56 V (about 115 on the ADC), at 20 °C, after about 17 h of preheat. After about 27 h the board itself read about 122 (12.2 %LEL with no offset) for 0.56 V on the meter, about 10 LSB more than 0.56 V / 5.15 V × 1023 = 111. Cause not found (meter accuracy, or AVcc lower than the 5V pin under load); the offset is taken from the board reading, which is what the firmware uses. Offset set to 113 on the MQ-5 (`FIRMWARE_ZERO_RAW_MQ5`, Day 5, approved by the author), the low end of the drift; MQ-2 offset stays 0 (0.44 V, still in preheat). Confirm both after 48 h of continuous power (FMEA F45) | REQ-FUN-003 |

### I²C bus (REQ-HMI-001, REQ-HMI-002)

| Check | Result |
|---|---|
| Scan before the STOP fix | No device answered (STOP not awaited, next START lost) |
| Scan after the STOP fix | 0x3E, 0x62 and 0x70 answer |
| LCD init and text | Pass: "GasGuard" on line 1, "bring-up OK" on line 2 |
| Backlight colour | Pass: green (NORMAL) |

### Firmware start-up

| Check | Result |
|---|---|
| Boot banner on UART at 38400 baud | Pass |
| Heartbeat once per second | Pass |
| Bandgap raw value (REQ-SAF-004) | **Open**: printed on each heartbeat; to record with the supply voltage measured at the same time |

### Still open (not run on the board)

| Check | Why it matters | Reference |
|---|---|---|
| Relay pin D5 state at reset, before any init | Floating input at reset could energise the coil | F21 |
| Watchdog reset timing with the longest blocking step | Must stay below 500 ms | F22 |
| Power-on reset cause (PORF and BORF) | Reported as unknown when both flags are set | F23 |
| Buzzer on D6 | Driver is host-tested only | REQ-HMI-003 |
| Button on D2 with the 30 ms debounce | Driver is host-tested only | REQ-HMI-007 |
| Second MQ on A1 and its pull-down | Not wired yet | HW-001, HW-003 |
