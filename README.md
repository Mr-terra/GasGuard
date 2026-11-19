# GasGuard

[![CI](https://github.com/Mr-terra/GasGuard/actions/workflows/ci.yml/badge.svg)](https://github.com/Mr-terra/GasGuard/actions/workflows/ci.yml)

**Fail-safe combustible gas detector firmware — bare-metal C99 for the ATmega328P (Arduino Uno R3).**

GasGuard reads two semiconductor gas sensors, raises a two-level alarm with a
latched high alarm, drives a normally-energised relay, and checks its own health
continuously: open and short circuits, stuck sensors, ADC reference, watchdog,
task timing, stack usage and EEPROM integrity.

> ⚠️ **Demonstration project. Not a certified safety device.** MQ-series sensors
> are uncalibrated, low-cost parts. Do not rely on this firmware to protect
> people or property.

---

## Contents

- [Why this project](#why-this-project)
- [Features](#features)
- [Architecture](#architecture)
- [Detector behaviour](#detector-behaviour)
- [Safety design](#safety-design)
- [Hardware](#hardware)
- [Getting started](#getting-started)
- [Verification and quality gates](#verification-and-quality-gates)
- [Development workflow](#development-workflow)
- [Roadmap](#roadmap)
- [Known limitations](#known-limitations)
- [How this project was built](#how-this-project-was-built)
- [Documentation](#documentation)
- [License](#license)

---

## Why this project

This project shows how I approach safety-related embedded software. The
process is the point:

1. **Requirements first.** 45 numbered requirements (`REQ-…`) define the
   behaviour before any code is written.
2. **Failure analysis.** A documented FMEA lists 20 failure modes, each with a
   mitigation and a residual risk.
3. **Constrained coding.** A MISRA C:2012 subset with 20 project rules, enforced
   by compilers and static analysers.
4. **Tests before code.** Host-based unit tests run in CI on every push;
   scripted scenario tests (SC-01 to SC-10, SC-13) run the whole chain on the PC.
5. **Traceability.** Every requirement is linked to the code that implements it
   and the tests that verify it, generated automatically.

## Features

| Area | Feature |
|---|---|
| Foundations | Fixed-width status codes and 0.1 %LEL concentration type |
| Foundations | Build, unit test, static analysis and memory budget gates |
| Foundations | Hardware interfaces (ADC, timer, GPIO, EEPROM, UART, I2C, watchdog, reset cause), each with a PC fake and unit tests |
| Sensing | Median of the last 3 raw samples, then moving average over the last 8 medians, per channel (`services/filter`): a single wild sample is removed |
| Sensing | Linear calibration to %LEL in Q8 fixed point, zero offset per channel (lowest clean-air values over 2.5 days on the bench: MQ-5 111, MQ-2 35); temperature on A3 read and checked every sample (valid between 0 and 50 degC, 1 MΩ pull-down so an unplugged sensor is invalid), compensation coefficient 0, as measured on the bench (REQ-FUN-004, see limitations) |
| Diagnostics | Open/short circuit (5 samples), stuck sensor (10 min, outside warm-up), bandgap (±10 %) |
| Diagnostics | Bandgap raw read against AVcc, checked at start-up and every 60 s; ten conversions discarded first so the reference settles; the first bad raw value of each boot is logged as `BANDGAP` (REQ-SAF-004, FMEA F43) |
| Diagnostics | Task overrun (three in a row raise FAULT, REQ-SAF-008) and stack watermark (FAULT below 64 free bytes, REQ-SAF-009) |
| Diagnostics | Stuck-only FAULT: when every sensor is out only because it is stuck (calm air), FAULT code 6 ends by itself as soon as a sensor moves; any real fault latches (REQ-SAF-007, FMEA F46) |
| Diagnostics | Reset loop: three starts in a row without 60 s of running past the warm-up, from the fourth on each start enters FAULT and logs it with its reset and configuration source (REQ-SAF-011, FMEA F33). Manual resets less than 3 min apart (with the default 120 s warm-up) count too |
| Alarms | Two-level alarm with hysteresis, latched ALARM_2, warm-up inhibition (extended once by 60 s when a cold sensor still reads between AL1 and AL2), max of valid channels |
| Alarms | Relay normally energised: energised in WARMUP, NORMAL and MAINTENANCE (holds the healthy state during a gas test, REQ-HMI-009), de-energised in ALARM_1, ALARM_2, FAULT and from start-up (`hal_gpio_init`; while the chip is held in reset the pin floats and a relay module needs the HW-004 pull-down, FMEA F21) |
| Storage | A/B configuration with CRC-16 and safe defaults, repair of the other copy (REQ-CFG-001..004) |
| Storage | Event log in a 111-entry EEPROM ring buffer (the largest that fits), CRC-16 per entry, written on events only (REQ-LOG-001..003) |
| Interface | 16×2 LCD driver: two lines of text and backlight colour (`drivers/lcd_rgb`), verified on the board |
| Interface | Buzzer and button drivers (`drivers/buzzer`, `drivers/button` with 30 ms debounce) |
| Interface | Buzzer patterns (1 Hz, 4 Hz, 1 beep / 5 s), short-press silence, long-press reset and functional test, debounce (REQ-HMI-003..007) |
| Interface | LCD: state text (MAINT, TEST), backlight colour per state, amber blink in FAULT, concentration on line 2, rows filled with spaces (REQ-HMI-001, REQ-HMI-002) |
| Interface | Maintenance mode from NORMAL by a 3 s touch (refused while the gas is at or above 8.0 %LEL, the ALARM_1 clear level), relay held, alarms not output, automatic exit after 10 minutes (REQ-HMI-008..010, FMEA F36) |
| Interface | Gas test record: at the end of MAINTENANCE, the highest reading of each sensor is logged as `PEAK <channel> <peak in 0.1 %LEL>` (REQ-LOG-001) |
| Interface | UART command line: `log` dumps the event log, one line per poll for the 64-byte transmit buffer (REQ-LOG-004); `status` prints the state, the concentration and the raw inputs in one line (REQ-LOG-005); `cfg` planned |
| Simulation | Potentiometer replaces the MQ-5 channel for bench tests (`GG_SIM_POT` build) |

Requirement-level coverage is tracked in
[`docs/traceability.md`](docs/traceability.md).

## Architecture

The firmware is organised in layers. A layer may only depend on the layers
below it, and the two top layers never include AVR headers. That rule is what
lets the application logic run unchanged on a PC (ADR-3).

```
┌──────────────────────────────────────────────────────────────┐
│ main.c        boot sequence, cooperative scheduler, watchdog │
├──────────────────────────────────────────────────────────────┤
│ app/          detector state machine, alarm logic, HMI,      │  no <avr/...>
│               maintenance, self-test                         │  pure C
├──────────────────────────────────────────────────────────────┤
│ services/     filter, calibration, diagnostics, config,      │  no <avr/...>
│               event log, CLI parser                          │  pure C
├──────────────────────────────────────────────────────────────┤
│ hal/          interfaces (.h only): adc, gpio, i2c, eeprom,  │
│               timer, uart, wdt, reset cause                  │
├──────────────────────────────┬───────────────────────────────┤
│ hal/*/*_avr.c  ATmega328P    │ hal/*/*_fake.c  PC fakes      │
│ register-level drivers       │ scriptable ADC, EEPROM in RAM,│
│                              │ simulated clock               │
├──────────────────────────────┴───────────────────────────────┤
│ drivers/      LCD RGB (I2C), buzzer, relay, button (debounce)│
└──────────────────────────────────────────────────────────────┘
```

Design decisions are recorded as ADRs in
[`docs/02-architecture.md`](docs/02-architecture.md#6-key-design-decisions-adr-summary):

| ADR | Decision | Why |
|---|---|---|
| ADR-1 | Bare-metal C, no Arduino framework | Register-level control; the Arduino core is C++ |
| ADR-2 | Cooperative scheduler, no RTOS | Deterministic and analysable; fits 2 KB of RAM |
| ADR-3 | Hardware abstraction with host fakes | Tests run on a PC; CI needs no board |
| ADR-4 | Normally-energised relay | Power loss or a crash ends on the alarm side |
| ADR-5 | Maximum of the valid channels | Never under-reports |
| ADR-6 | Fixed-point arithmetic, `int16_t` in 0.1 %LEL | No floating point on AVR: smaller and deterministic |

## Detector behaviour

### Operating modes

```
power-on → INIT → SELF_TEST → WARMUP → NORMAL ⇄ ALARM_1 → ALARM_2 (latched)
                                          ⇅
                                     MAINTENANCE  (automatic exit after 10 min)

Any state → FAULT   (all channels faulty, ADC fault, 3 overruns, stack low)
```

- **WARMUP** lasts 120 s by default (configurable 30–600 s). Alarms are inhibited
  and the display says so. If the gas is still between AL1 and AL2 at its end (a
  sensor left unpowered for days reads high for a while), the warm-up is extended
  once by 60 s and the display shows `WARMUP +1 MIN`. It ends as soon as the gas
  falls below AL1; at AL2 or above there is no extension and the alarm follows
  (REQ-ALM-007).
- **FAULT** de-energises the relay and sounds the fault pattern. It stays until a
  restart, except a FAULT caused only by stuck channels (calm air): that one ends
  by itself as soon as a channel moves again (REQ-SAF-007).
- **MAINTENANCE** holds the relay in its last healthy state, shows alarms without
  outputting them, and always ends by itself.

### Alarm thresholds

| Parameter | Default | Rule |
|---|---|---|
| AL1 (ALARM_1 threshold) | 10 %LEL | Entered after 3 consecutive filtered samples at or above AL1 |
| AL2 (ALARM_2 threshold) | 25 %LEL | Entered after 3 consecutive filtered samples at or above AL2 |
| Hysteresis | 2 %LEL | ALARM_1 clears below AL1 − 2 %LEL for 10 consecutive samples |
| ALARM_2 latch | — | Clears only on an operator reset, and only when the gas is below AL1 − 2 %LEL |
| Validity | 0 < AL1 < AL2 ≤ 60 %LEL and 0 ≤ hysteresis < AL1 | Any configuration outside these ranges is rejected |

### Buzzer and display

| Condition | Backlight | Buzzer |
|---|---|---|
| NORMAL | Green | Silent |
| WARMUP, ALARM_1, MAINTENANCE | Yellow | ALARM_1: slow beep (1 Hz) |
| ALARM_2 | Red | Fast beep (4 Hz) |
| FAULT | Blinking amber | One beep every 5 s |

A short press (under 1 s) silences ALARM_1. ALARM_2 can be silenced for 60 s in
total per alarm, from the first press: pressing again does not extend it. A long press (3 s or more) requests the ALARM_2 reset. In NORMAL, a
long press runs the functional test: buzzer, every backlight colour, relay
toggle.

### Status command

Send `status` (with a newline) on the serial port, 38400 baud. The answer is
one line:

```
STATUS <state> <conc> <mq5 raw> <mq2 raw> <temp raw> <temp valid>
STATUS 1 64 135 30 512 1
```

- `state`: 0 WARMUP, 1 NORMAL, 2 ALARM_1, 3 ALARM_2, 4 FAULT, 5 MAINTENANCE.
- `conc`: the concentration the detector uses, in 0.1 %LEL (64 is 6.4 %LEL).
- `mq5 raw`, `mq2 raw`, `temp raw`: last ADC values, 0 to 1023 (about 4.9 mV each).
- `temp valid`: 1 when the temperature reading is between 0 and 50 degC. With
  the sensor unplugged, the 1 MΩ pull-down holds A3 at 0, so it reads 0 (HW-005).

Sent during a `log` dump, the answer can be lost (transmit buffer full): send
`status` again after `END`.

### Periodic gas test

A value that does not move can be calm air or a dead sensor: the firmware cannot
tell them apart (FMEA F03, F46). A periodic gas test proves that each sensor still
responds. Recommended once a month, and after any maintenance. It is done in
MAINTENANCE, so the test gas does not trip the relay or sound the buzzer:

1. Check that the detector is in NORMAL, then touch the touch sensor for 3 s:
   the display shows `MAINT` (yellow). The relay holds, alarms are not output
   (REQ-HMI-009), and MAINTENANCE ends by itself after 10 minutes. If `MAINT`
   does not appear and line 2 reads 8.0 %LEL or more, the touch was refused
   (REQ-HMI-008): ventilate, wait for the reading to drop, then touch again.
2. Release a short puff of gas from an **unlit** lighter (about 1 s, no flame)
   a few centimetres from the MQ-5, in a ventilated room.
3. Expected: the concentration on line 2 rises above 10 %LEL (the ALARM_1
   threshold).
4. Wait until the reading is back below 8 %LEL, then repeat with the MQ-2.
5. Before the 10 minutes are over, let the reading fall back below 8 %LEL:
   if gas is still there when MAINTENANCE ends, the alarm sounds as usual.
6. When MAINTENANCE ends, the firmware logs the highest reading of each sensor
   during it. Dump the log (`log`): after the `MODE` entry of the exit come
   `PEAK 0 <value>` (MQ-5) and `PEAK 1 <value>` (MQ-2), in 0.1 %LEL, so `PEAK 0 516` is 51.6 %LEL. A
   sensor whose peak stays below 100 (10 %LEL) did not respond and must be
   replaced.

## Safety design

The design goal is **fail-safe behaviour**: when the device cannot trust its own
measurement, it says so and moves to a safe state.

| Hazard | Principle | Mechanism |
|---|---|---|
| Missed alarm when the sensor is unplugged | Open input must not look like clean air | 100 kΩ pull-down on the Arduino side of each gas input; open or short detected in software (REQ-SAF-002) |
| Missed alarm from a dead or stuck sensor | A frozen reading must be detected | Stuck-value check over 10 minutes outside WARMUP (REQ-SAF-003) |
| Wrong readings from a cold sensor | Do not trust what has not warmed up | Warm-up period with alarms inhibited and shown (REQ-ALM-006), extended once by 60 s for a sensor still above AL1 and below AL2 (REQ-ALM-007) |
| Undetected firmware hang | Hardware watchdog | 500 ms watchdog, kicked only after every task has run (REQ-SAF-005) |
| Stack overflow corrupts memory | No dynamic memory, no recursion | Stack painted at boot, watermark checked every second (REQ-SAF-009/010) |
| Corrupted configuration | Redundant copies and checksums | Two EEPROM copies with CRC-16, safe defaults if both fail (REQ-CFG-001/002) |
| Power loss or crash | Fail-safe output | Normally-energised relay: the alarm side is the default (REQ-SAF-001) |
| Alarm silenced and forgotten | Latch the severe alarm | ALARM_2 is latched and can be silenced for 60 s in total per alarm, however often the button is pressed (REQ-ALM-004, REQ-HMI-004) |

The full analysis, with severity, occurrence, detection, RPN before and after
mitigation, is in [`docs/03-safety-analysis.md`](docs/03-safety-analysis.md).

## Hardware

### Bill of materials

| Part | Role |
|---|---|
| Elegoo Uno R3 (ATmega328P, 16 MHz) | Microcontroller board |
| Seeed Grove Base Shield v2.1 | Connectors for the Grove modules |
| MQ-5 and MQ-2 gas sensor modules | Gas channels (generic modules, not Grove) |
| Grove LCD RGB Backlight v4.0 | 16×2 display (I2C) |
| Grove Button v1.2, Grove Touch Sensor v1.1 | Operator input |
| Grove Buzzer v1.2 | Audible alarm |
| Grove Rotary Angle Sensor v1.2 | Gas simulator (simulation build only) |
| Grove Temperature Sensor v1.2 | Temperature input on A3 (compensation coefficient 0) |
| 100 kΩ resistors (×2), 1 MΩ resistor | Pull-downs on the gas inputs and on the temperature input |
| 9 V 1 A wall adapter | Supply on the barrel jack |

Models, quantities, connections, supply and tools are listed in
[`docs/07-hardware.md`](docs/07-hardware.md).

### Pin mapping (Grove Base Shield v2.1)

| Grove port | MCU pin | Function |
|---|---|---|
| A0 | PC0 / ADC0 | MQ-5 analog output (natural gas / LPG) |
| A1 | PC1 / ADC1 | MQ-2 analog output (combustible gas, smoke) |
| A2 | PC2 / ADC2 | Potentiometer, used as gas simulator |
| A3 | PC3 / ADC3 | Temperature sensor |
| D2 | PD2 | Button |
| D3 | PD3 | Touch sensor |
| D5 | PD5 | Relay (alarm contact) |
| D6 | PD6 (OC0A) | Buzzer |
| I2C | PC4 / PC5 | LCD at 0x3E, RGB backlight at 0x62 (v5.0 boards use 0x30) |
| UART | PD0 / PD1 | 38400 baud, 8N1, over USB |

### Wiring rules

The MQ modules are wired by colour through a Grove-to-Dupont cable:

| Cable wire | Signal | MQ module pin |
|---|---|---|
| Yellow | SIG1 (port A0 or A1) | **AO** |
| Red | 5 V | **VCC** |
| Black | GND | **GND** |
| White | SIG2 (second Grove signal) | **Not connected, insulated** |

- Add a **100 kΩ pull-down** from each gas input to GND, on the Arduino side of
  the cable. Without it, an unplugged cable reads random values instead of zero.
- The module's DO output is not used. Thresholds, filtering and hysteresis are
  done in software.
- Power: each MQ heater draws about 150 mA. Do not connect more than two MQ
  modules to the board's 5 V supply.

Full rationale and hardware requirements HW-001 to HW-003 and HW-005 are in
[`docs/02-architecture.md`](docs/02-architecture.md#21-gas-sensor-wiring-generic-mq-modules-vcc--gnd--do--ao).

## Getting started

### Prerequisites

- Windows 10 or 11 with **WSL2** and an Ubuntu distribution. The build runs
  inside WSL, so that it matches CI exactly.
- For flashing: the board, a USB cable, and a USB-serial driver if Windows does
  not detect the board on a COM port.

### Install the toolchain (Ubuntu in WSL)

```bash
sudo apt-get update
sudo apt-get install -y build-essential gcc-avr avr-libc binutils-avr \
     cppcheck clang-tidy gcovr python3-pip
pip install --user lizard
```

### Build and check

```bash
cd /mnt/c/path/to/GasGuard   # the repository, seen from WSL
make check                   # runs every gate listed below
```

Individual targets:

| Target | What it does |
|---|---|
| `make test` | Builds and runs every `test_*.c` under `tests/` on the PC, with AddressSanitizer and UBSan |
| `make lint` | cppcheck with the MISRA addon, clang-tidy, lizard, header self-containment, R20 grep |
| `make coverage` | Line coverage report in `build/coverage/index.html` |
| `make avr` | Cross-compiles `build/avr/gasguard.hex` for the ATmega328P |
| `make avr-sim` | Simulation image `build/avr-sim/gasguard_sim.hex`: the MQ-5 channel reads the potentiometer on A2 (bench only, FMEA F41) |
| `make avr-bench` | Bench image `build/avr-bench/gasguard_bench.hex`: timing and stack statistics on the UART, faults injected on command (bench only, FMEA F49) |
| `make size` | Checks flash and RAM against the budget of REQ-NFR-005 |
| `make trace` | Regenerates `docs/traceability.md`; `--allow-gaps` until Day 5 |
| `make clean` | Removes `build/` |

### Flash the board

Flashing is done by hand from Windows, never from the build system:

```powershell
tools\flash.ps1 -Port COM3
```

The firmware prints nothing by itself on the serial port: send `log` (event
log) or `status` (state and raw inputs) at 38400 baud, with a newline.

Two helper scripts, both run from Windows in the project folder:

```powershell
tools\monitor.ps1 -Port COM3              # read the serial log (Ctrl+C to stop)
tools\flash-monitor.ps1 -Port COM3        # flash, then read the log
tools\status-log.ps1 -Port COM3           # send `status` every 5 min, append it to status-log.csv
```

`status-log.ps1` records the state, the concentration and the raw inputs over
hours, for the measurement of the temperature coefficient (REQ-FUN-004). Run it
in clean air for one to two days while the room warms and cools. The first
measurement (2026-11-15 to 17) is in the test report.

If PowerShell refuses to run a script (execution policy), start it with
`powershell -ExecutionPolicy Bypass -File tools\flash-monitor.ps1 -Port COM3`.
The flash needs `avrdude` in the `Path` (see `tools/flash.ps1`).

The simulation image (`make avr-sim`, `gasguard_sim.hex`) has no real gas sensor, so
`flash.ps1` refuses it unless `-Bench` is given (FMEA F41).

The bench image (`make avr-bench`, `gasguard_bench.hex`) measures what the host
tests cannot: it prints `BENCH <uptime s> <longest cycle ms> <longest display ms>
<free stack bytes>` every 10 s, and takes commands of a `#` and a letter: `#s`
statistics now, `#o` one 150 ms cycle (OVERRUN), `#f` three in a row (FAULT 3),
`#h` stops the loop right after a watchdog kick (watchdog reset). `flash.ps1`
refuses it unless `-Bench` is given (FMEA F49). `make avr` never compiles the
bench code. `tools\bench.ps1 -Port COM3 [-Command '#h'] [-Seconds 15]` prints each
line with the PC time in ms; the watchdog timeout is the gap between
`BENCH HANG` and `BENCH BOOT`, minus the boot time that `BENCH BOOT` prints.

### Power-up order

The USB port of the PC cannot supply the two gas sensor heaters: on USB alone
the 5 V rail falls to about 4.2 V. The board's USB chip, on the other hand, is
not recognised by Windows when the 9 V jack is plugged in first. Start the
board in this order:

1. Plug the **USB cable** first. Windows finds the serial port (COMx). The
   board starts on USB alone, sees its supply too low (`BANDGAP` about 268 in
   the log) and enters **FAULT**. That is the expected, safe reaction
   (REQ-SAF-004): with 4.2 V the gas readings would be wrong.
2. Plug the **9 V jack**. The 5 V rail rises to about 5.1 V; the FAULT stays
   latched.
3. Press the **RESET** button of the Arduino **once**. The board starts again
   on the jack: warm-up (2 min), then NORMAL. The serial port stays recognised.

Do not press RESET again within 3 minutes, and do not unplug and replug the
supply several times in a row: from the fourth start in a row that runs less
than 3 minutes, the board starts in FAULT (`FAULT 5`, reset-loop protection,
REQ-SAF-011). To leave it, keep the board powered for 3 minutes without
touching it, then press RESET once.

## Verification and quality gates

Verification follows four levels, defined in
[`docs/05-verification-plan.md`](docs/05-verification-plan.md).

| Level | Where | What | Tool |
|---|---|---|---|
| L1 Static | CI | Warnings, MISRA subset, complexity, AVR header isolation | gcc, cppcheck, clang-tidy, lizard |
| L2 Unit | CI (PC) | Each `services/` and `app/` module with host fakes | Unity, gcov |
| L3 Scenario | CI (PC) | Full detector driven by scripted ADC sequences, time-stepped | Unity, `hal/*/*_fake.c` |
| L4 Hardware | Bench | Real board, potentiometer, then butane from an unlit lighter | Manual, multimeter, event log |

Scenario tests SC-01 to SC-10 and SC-13 are implemented (Day 2), with two deviations: SC-01 starts in WARMUP (INIT and SELF_TEST come on Day 4), and SC-15 is unit-tested only. SC-11, SC-12, SC-14 and SC-16 are implemented on Day 4 (`tests/scenarios/test_scenarios_day4.c`). They cover boot, alarms,
latching, sensor faults, configuration recovery, maintenance timeout, bandgap
failure and task overruns.

### Gates

| Gate | Requirement | Limit |
|---|---|---|
| Compiler | REQ-NFR-003 | Zero warnings with `-Wall -Wextra -Werror` and the extra flags of the standard |
| Static analysis | Coding standard §6 | Zero findings outside the register of deviations |
| Unit test coverage | REQ-NFR-004 | At least 90 % of lines in `app/` and `services/`; `make coverage` fails below |
| Flash | REQ-NFR-005 | At most 80 % of 32 KB, measured on the firmware image (`make size`: about 14.9 KB, 58 % of the budget) |
| RAM | REQ-NFR-005 | At most 60 % of 2 KB, static data only (`make size`: 701 B of 1228 B); the stack is watched at run time (REQ-SAF-009) |
| Traceability | `trace.py` | Every requirement has code and a test (enforced on Day 5) |

Accepted deviations from the coding standard are listed in
[`docs/04-coding-standard.md`](docs/04-coding-standard.md#5-deviations-register).

## Development workflow

- **Branches**: one per plan day (`day-1`, `day-2`, …), created from `main`.
  Merged back with a fast-forward after rebase, once the day is validated.
- **Commits**: conventional commits, one logical change each, in English.
- **TDD**: the test comes first. A `test(...)` commit may therefore be red on a
  day branch; the branch must be green before it is merged.
- **Requirements**: a change starts from a `REQ-…` identifier. Tests are tagged
  `@verifies`, code is tagged `@req`.
- **Continuous integration**: `make check` runs on every push to `main`. Other
  branches are checked on demand, with the manual trigger or a `ci-*` tag.

## Roadmap

**Version 1.0 — five-day plan** ([`PLAN.md`](PLAN.md))

1. **Foundations**: toolchain, Makefile, Unity, types and HAL interfaces, CI.
2. **Core logic on the PC**: filter, calibration, diagnostics, state machine,
   alarm logic, first scenario tests.
3. **Hardware layer**: AVR drivers for every peripheral, LCD, buzzer, relay,
   button, first flash.
4. **Integration**: scheduler, watchdog policy, configuration A/B, event log,
   CLI, HMI patterns, maintenance mode, remaining scenario tests, bench test
   with the potentiometer.
5. **Evidence**: 100 % traceability, MISRA review of the whole tree, hardware
   test report with logic-analyser captures, final documentation, tag `v1.0`.

**Version 2 (planned)**

- Modbus RTU over UART (industrial standard).
- MQ-7 carbon monoxide channel with heater cycling.
- PC simulator with live plots.
- Low-power mode.
- Hardware-in-the-loop test with a second board.

## Known limitations

- **Not a certified product.** The standards IEC 60079-29-1, EN 50194 and JIS
  were used as design inspiration only. No certification is claimed.
- **Sensor drift** (FMEA F05, residual RPN 36) is accepted. A real product needs
  periodic calibration with reference gas, which is out of scope here.
- **Single microcontroller.** There is no hardware redundancy and no independent
  monitoring channel.
- **Semiconductor sensors** respond to many gases and vapours, not only to the
  target gas. Readings are approximations (see REQ-FUN-003).
- **Maintenance inhibits the alarm outputs** for up to 10 minutes, as REQ-HMI-009
  states. The relay stays energised and the buzzer stays silent during that time
  (FMEA F36, accepted by the author).
- **Start-up needs the USB, then the 9 V jack, then RESET** (see "Power-up
  order"): a start on USB alone ends in FAULT, and with the jack first the
  board's USB chip is not recognised by Windows.
- **No relay module is fitted.** The firmware drives the relay output (D5) as
  specified; on the bench only its pin level can be checked.
- **Not yet checked on the board:** the stack watermark value, the D5 level at
  reset and during the functional test, and the watchdog timing.
  The open steps are listed in
  [`docs/test-report.md`](docs/test-report.md).
- **The `log` command needs a newline** (`\n`): a carriage return alone is ignored.
  The first line after opening the port can read `ERR`, because a stray byte arrives
  when the port opens; send a newline first.
- **Calm air gives short nuisance FAULTs.** In clean, still air a sensor can stay
  within 1 LSB for 10 minutes and be seen as stuck. With both sensors stuck the
  detector enters FAULT code 6 and drops the relay; it ends by itself as soon as
  a sensor moves (FMEA F46, REQ-SAF-007).
- **An ageing sensing element is not detected by the firmware.** A dead heater
  reads as an open circuit (checked on the bench, FMEA F03), but an element that
  loses its sensitivity with a working heater looks like clean air. The monthly
  gas test in MAINTENANCE, recorded as `PEAK` entries, is the only check.
- **A faulty gas channel is excluded from the detection.** It is written to the event log
  (`CHANNEL`). It comes back into service (`RECOVERED`) after 60 s of valid readings
  (open or short circuit, or failed ADC reads), or as soon as a frozen value moves again (REQ-SAF-002,
  REQ-SAF-003, confirmed by the author on Day 5; the values may be tuned later).
- **No display, no stop**: the boot goes on without the LCD and logs a `DISPLAY` event.
  A failed boot arms the watchdog, so the board resets and tries again.
- **Temperature compensation does not change the reading**: the temperature
  input (A3) is read and checked, but its coefficient is zero. Two days of
  measurement in clean air (16 to 19 degC) showed changes with the time of day
  (about +10 raw on the MQ-5 every evening) and a slow drift, larger than any
  temperature effect; a coefficient fitted on them would add an error (FMEA
  F48). A new measurement needs a wider temperature swing.
- **Calibration is compiled in, not stored in the configuration** as REQ-FUN-003
  asks. The zero offsets are bench values (MQ-5 111, MQ-2 35, lowest clean air over 2.5 days) and the
  span is one LSB per 0.1 %LEL, not checked with a reference gas (FMEA F05, F45).
- **Relay pull-down at reset** (FMEA F21) is proposed in docs/02, not yet fitted.
- **The `cfg` command** of the UART interface is not implemented; `log` and
  `status` are (REQ-LOG-004, REQ-LOG-005).

## How this project was built

Code was written with Claude Code, inside a quality system defined in the
[`.claude/`](.claude) folder:

- `CLAUDE.md` holds the project rules, the workflow for each change, and the
  day-by-day plan the agent must follow.
- A PostToolUse hook ([`tools/hooks/guard.py`](tools/hooks/guard.py)) rejects
  forbidden constructs (`malloc`, `goto`, `printf`, floating point in the
  application layers) on every edit.
- Skills cover MISRA review, test writing from requirements, FMEA impact
  analysis, traceability checks and module scaffolding.

Every change goes through tests, static analysis and review before it is
committed. The AI writes code; the process decides whether it is accepted.

### Use of AI assistance

GasGuard was written with Claude, an AI coding assistant from Anthropic. The
assistant drafted code, tests and documents. The author decided the
requirements, the safety design, the acceptance of each change and every action
on the hardware. Each change must pass `make check` before it is committed;
code changes also go through a MISRA review and, when they touch a safety
mechanism, a safety-impact review.

The full description (scope, decisions, controls, limits) is in
[`docs/06-ai-usage.md`](docs/06-ai-usage.md).

## Documentation

| Document | Content |
|---|---|
| [Requirements](docs/01-requirements.md) | 46 requirements in 7 areas: function, alarm, safety, interface, configuration, log, non-functional |
| [Architecture](docs/02-architecture.md) | Layers, pin mapping, wiring, state machine, scheduler, memory budget, ADRs |
| [Safety analysis](docs/03-safety-analysis.md) | FMEA with severity, occurrence, detection and residual risk |
| [Coding standard](docs/04-coding-standard.md) | MISRA C:2012 subset, project rules R01–R20, tooling, deviations |
| [Verification plan](docs/05-verification-plan.md) | Four verification levels, scenarios SC-01 to SC-16, bench procedure |
| [Traceability](docs/traceability.md) | Generated matrix, requirement → code → test |
| [Test matrix](docs/test-matrix.md) | For each requirement, its positive, negative, false-positive, false-negative and limit tests, and the open findings |
| [Hardware](docs/07-hardware.md) | Bill of materials with models and connections, supply, tools, bench safety notes |
| [Test report](docs/test-report.md) | Hardware bench results, one section per session, with the steps still open |
| [AI assistance](docs/06-ai-usage.md) | How an AI coding assistant was used, why, and the controls on its output |
| [Plan](PLAN.md) | Five-day implementation plan |

## License

Released under the MIT License. See [`LICENSE`](LICENSE).

Third-party code:

- [Unity](https://github.com/ThrowTheSwitch/Unity) (MIT), vendored in
  [`third_party/unity/`](third_party/unity/LICENSE.txt).
