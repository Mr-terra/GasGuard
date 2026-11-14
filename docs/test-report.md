# Test report

Hardware test results, one section per bench session. Each entry gives the
date, the firmware (branch and commit), and the result of each step. Steps not
yet run are listed as open, so nothing is reported as passed by default.

## Open steps for the next bench session (Day 5 and after)

No result is recorded for these steps yet. The steps confirmed on the board
(button, touch, LCD, `log`, second sensor, stuck channel, bandgap, ALARM_2
buzzer rate) are in the sessions below and have left this list. Each one needs the board, the
multimeter or the serial monitor (tools in
[`07-hardware.md`](07-hardware.md)).

| Step | What to check | Requirement |
|---|---|---|
| Relay at reset | Pin D5 low before the first GPIO init (FMEA F21) | REQ-SAF-001 |
| Watchdog timing | Reset after 500 ms without kick; no reset in normal running | REQ-SAF-005 |
| Stack watermark | Value of `hal_stack_free_bytes` after boot and after a minute | REQ-SAF-009 |
| Buzzer patterns | 1 Hz in ALARM_1 (4 Hz in ALARM_2 and one beep every 5 s in FAULT were heard on Day 5) | REQ-HMI-003 |
| Bandgap | Raw value of the bandgap against AVcc (expected near 0xED at 4.75 V) | REQ-SAF-004 |
| MQ5 baseline | Clean-air voltage on A0 once it is stable, after 24 to 48 h of continuous power (datasheet preheat). Then fix `zero_raw` from it. Not yet measured: the voltage was still drifting after about 4 h | REQ-FUN-003, REQ-SAF-003 |
| Watchdog reset cause | After a watchdog reset, WDRF is seen (the bootloader can clear the reset flags first) | REQ-SAF-006 |
| Display time | Longest display task on the board (I2C): stays well under 100 ms per cycle | REQ-SAF-008 |
| Overrun detection | With the end time fix (`day-5-fix-overrun`): no `OVERRUN` in normal running over one hour; a cycle made longer on purpose (bench build only) logs `OVERRUN` and three in a row give `FAULT 3` | REQ-SAF-008 |
| UART input | Paste a 16-character command: no byte lost | REQ-LOG-004 |
| Relay pull-down | Only once a relay module is fitted (none on the bench): relay state at reset, with and without the proposed 10 kΩ pull-down | REQ-SAF-001, FMEA F21 |
| All channels stuck | One night with both sensors in clean air: any FAULT `6` (stuck) ends by itself when a channel moves; no restart needed | REQ-SAF-003, REQ-SAF-007 |
| Periodic gas test | The procedure of the README, in MAINTENANCE (unlit lighter on each sensor): each reading rises above 10 %LEL, no relay drop and no buzzer; after MAINTENANCE ends, each `PEAK` entry is above 100 (10 %LEL) and no `OVERRUN` follows | REQ-SAF-003, REQ-HMI-009, REQ-LOG-001 |
| Temperature input | Grove Temperature Sensor v1.2 on A3: `status` gives a raw value near 512 at about 25 degC (lower when colder) and `temp valid` 1; with the cable unplugged, check what A3 reads (no pull-down on A3: a floating input may read as valid, to settle before the coefficient is used) | REQ-FUN-004 |
| Status command | `status` answers one line with the state, the concentration and the raw values; the raw values match the voltages measured on A0, A1 and A3 (about 4.9 mV per count) | REQ-LOG-005 |
| Temperature coefficient | Clean air, one to two days with the room warming and cooling: `status` every few minutes, then the slope of the MQ raw values against the temperature raw value | REQ-FUN-004 |
| Reset loop | 4 manual resets less than 3 min apart: the 4th start is in FAULT (`FAULT 5` in the log); 3 min of running, then a reset: normal start | REQ-SAF-011 |
| Calibration in the configuration | REQ-FUN-003 is **only partly met**: the zero offsets are compiled in (`FIRMWARE_ZERO_RAW_*`), not stored in the configuration, and the span is not checked with a reference gas. The trace shows code and tests for REQ-FUN-003, not this gap. Decision of the author (Day 5): v1.0 ships with the current compiled-in values; the values are changed later | REQ-FUN-003 |

## Day 5 — second night run (2026-11-14 to 2026-11-15)

Firmware: `main` at `863eb47` (stuck-only FAULT that ends by itself, PEAK record,
cycle end time), normal build. Both MQ sensors in clean air. Log dumped in the
morning (entries 17 to 56).

| Check | Result | Requirement |
|---|---|---|
| Stuck-only FAULT | **Pass**: three `FAULT 6` episodes (at 2601 s, 3712 s and 6624 s), each with both channels stuck; each ended by itself 2 to 4 s later (`RECOVERED`, `MODE 1`), with no restart | REQ-SAF-003, REQ-SAF-007, FMEA F46 |
| Single stuck channel | **Pass**: the MQ-2 alone was stuck four times (`CHANNEL 1` then `RECOVERED 1`); the detector stayed in NORMAL on the MQ-5 | REQ-SAF-003 |
| Overrun detection | **Pass for normal running**: no `OVERRUN` entry over the run, the first one with the real cycle end time. The forced overrun (bench build) is still open | REQ-SAF-008 |
| Log volume | About 25 entries in about 1 h 50 min of events, then no event until the next start. The 111-entry ring keeps the night | REQ-LOG-001, REQ-LOG-003 |
| Bandgap at a start | **To explain**: one start logged `BANDGAP 266` then `FAULT 1` at 0 s (entries 49 to 53). 266 means AVcc about 1.1 × 1023 / 266 = 4.2 V, below the tolerance. The next start was normal. Probable cause: the board was powered by USB only while the temperature sensor was wired (USB gave 4.53 V without load on Day 5) | REQ-SAF-004 |

The entries 0 to 16 are the simulation-build test of the same day.

## Day 5 — simulation build (2026-11-14)

Firmware: `gasguard_sim.hex` built by `make avr-sim` from `main` at `7f41634`,
flashed with `tools/flash.ps1 -Bench`. Grove Rotary Angle Sensor on A2 in place
of the MQ-5 channel; A1 read as usual. Validated by the author.

| Check | Result | Requirement |
|---|---|---|
| Potentiometer drives the MQ-5 channel | **Pass**: NORMAL, ALARM_1, ALARM_2 and the return below the hysteresis follow the potentiometer | REQ-FUN-006, REQ-ALM-001..004 |
| Potentiometer at 0 V (open circuit) | **Pass**: the channel is excluded; A1 still answers to gas and breath | REQ-SAF-002, REQ-FUN-005 |
| Recovery of the channel | **Pass, as specified**: the channel comes back 2 to 3 minutes after 0 V. Turned to full scale first, the potentiometer read as a short circuit (raw ≥ 1018), which restarts the 600 valid samples (60 s) of REQ-SAF-002; the author keeps 60 s | REQ-SAF-002 |
| Gas test record (`PEAK`) | Not run | REQ-LOG-001 |

The sensor that answered to breath and gas during this test is the MQ-2: the
author confirmed that MQ-2 - AO is wired to Arduino A1, the only gas input this
build reads besides the potentiometer. The wiring matches docs/02 §2.

## Day 5 — dead heater (2026-11-14)

Voltage measurement only, with the multimeter; the firmware does not matter.
Question: is a sensor whose heater is dead seen as a fault, or does it look like
calm air (FMEA F03)?

| Check | Result | Requirement |
|---|---|---|
| MQ5 - VCC unplugged, MQ5 - AO read on Arduino A0 ↔ Arduino GND | **Pass**: 0 V after 10 minutes, below the 0.025 V open-circuit limit (`DIAG_OPEN_MAX` = 5 raw). A dead heater reads as an open circuit, so the channel is excluded within 5 samples and, with both channels out, the FAULT latches | REQ-SAF-002, REQ-SAF-003 |

Not covered: a sensing element that ages while its heater still works, and the
MQ-2 (same Grove module type, not measured). The monthly gas test covers both.

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
| ALARM_2 with lighter gas (unlit) on the MQ-5 | Pass: up to 51.6 %LEL (first try) and 88 %LEL (second try), ALARM_2 entered, red backlight, buzzer 4 beeps per second | REQ-ALM-002, REQ-HMI-002, REQ-HMI-003 |
| ALARM_2 short press | Pass: the buzzer stops, then sounds again after 60 s | REQ-HMI-004 |
| ALARM_2 latch and reset | Pass: the state stays ALARM_2 after the gas is gone; a 3 s press between 8 and 10 %LEL is refused (above AL1 − hysteresis); the same press below 8 %LEL resets the latch | REQ-ALM-004, REQ-HMI-005 |
| Night run (firmware: `day-5-fix-reset-loop` working tree, flashed at about 22:10; 13 h) | **Fail (false fault)**: both channels declared stuck and back again many times, each `CHANNEL` exactly 600 s after the previous `RECOVERED` of the same channel; values stable within 1 LSB for up to 2 h 10. Both channels out at once gives FAULT, seen again on the morning of Day 5+1 (display 0 %LEL, then FAULT latched). The author keeps the 10 min window; a FAULT caused only by stuck channels now ends by itself (REQ-SAF-007, F46). To check over a night | REQ-SAF-003, REQ-SAF-007 |
| Display dark after a flash | The display stayed off for the whole first run (about 13 h), then worked after the next start. Probable cause, not confirmed: a reset in the middle of an I2C transfer leaves the LCD holding SDA low | REQ-HMI-001 |
| Reset in the morning | The run ended after 47 024 s (about 13 h, near 11:15) with `RESET 4`. Probable cause: the PC was switched on and the USB enumeration reset the Uno; the watchdog is not proven | REQ-SAF-005 |
| Unexplained channel event | With `60ac2b2`, `CHANNEL 0` at 832 s then `RECOVERED 0` at 1111 s (MQ-5). Explained by the night run: the stuck check (10 min window) declared the MQ-5 stuck about 10 min after the warm-up. Not a wiring fault | REQ-SAF-002 |

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
