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
- [Features and status](#features-and-status)
- [Architecture](#architecture)
- [Detector behaviour](#detector-behaviour)
- [Safety design](#safety-design)
- [Hardware](#hardware)
- [Getting started](#getting-started)
- [Verification and quality gates](#verification-and-quality-gates)
- [Repository layout](#repository-layout)
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

## Features and status

| Area | Feature | Status |
|---|---|---|
| Foundations | Fixed-width status codes and 0.1 %LEL concentration type | Done (Day 1) |
| Foundations | Build, unit test, static analysis and memory budget gates | Done (Day 1) |
| Foundations | Hardware interfaces (ADC, timer, GPIO, EEPROM, UART, I2C, watchdog, reset cause), each with a PC fake and unit tests | Done (Day 1); AVR drivers Day 3 |
| Sensing | Moving average over the last 8 samples per channel (`services/filter`) | Done (Day 2); 100 ms sampling loop Day 4 |
| Sensing | Linear calibration to %LEL in Q8 fixed point, temperature compensation when the sensor is valid | Done (Day 2) |
| Diagnostics | Open/short circuit (5 samples), stuck sensor (10 min, outside warm-up), bandgap (±10 %) | Done (Day 2) |
| Diagnostics | Task overrun and stack watermark checks | Day 4 |
| Alarms | Two-level alarm with hysteresis, latched ALARM_2, warm-up inhibition, max of valid channels | Done (Day 2) |
| Alarms | Relay normally energised: energised in WARMUP and NORMAL only, de-energised on alarm and fault (state logic done; driving the pin is Day 3) | Logic done (Day 2); pin Day 3 |
| Storage | A/B configuration with CRC-16 and safe defaults | Day 4 |
| Storage | Event log in a 32-entry EEPROM ring buffer | Day 4 |
| Interface | 16×2 LCD with backlight colours, buzzer patterns, button actions | Day 3–4 |
| Interface | Maintenance mode with automatic exit after 10 minutes | Day 4 |
| Interface | UART command line (`log`, `status`, `cfg`) | Day 4 |
| Simulation | Potentiometer replaces the MQ-5 channel for bench tests | Day 3 |

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
│               event log, CLI parser                         │  pure C
├──────────────────────────────────────────────────────────────┤
│ hal/          interfaces (.h only): adc, gpio, i2c, eeprom,  │
│               timer, uart, wdt, reset cause                  │
├──────────────────────────────┬───────────────────────────────┤
│ hal/avr/   ATmega328P        │ hal/host/  PC fakes           │
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
  and the display says so.
- **FAULT** de-energises the relay and sounds the fault pattern.
- **MAINTENANCE** holds the relay in its last healthy state, shows alarms without
  outputting them, and always ends by itself.

### Alarm thresholds

| Parameter | Default | Rule |
|---|---|---|
| AL1 (ALARM_1 threshold) | 10 %LEL | Entered after 3 consecutive filtered samples at or above AL1 |
| AL2 (ALARM_2 threshold) | 25 %LEL | Entered after 3 consecutive filtered samples at or above AL2 |
| Hysteresis | 2 %LEL | ALARM_1 clears below AL1 − 2 %LEL for 10 consecutive samples |
| ALARM_2 latch | — | Clears only on an operator reset, and only when the gas is below AL1 − 2 %LEL |
| Validity | 0 < AL1 < AL2 ≤ 60 %LEL | Any configuration outside this range is rejected |

### Buzzer and display

| Condition | Backlight | Buzzer |
|---|---|---|
| NORMAL | Green | Silent |
| WARMUP, ALARM_1, MAINTENANCE | Yellow | ALARM_1: slow beep (1 Hz) |
| ALARM_2 | Red | Fast beep (4 Hz) |
| FAULT | Blinking amber | One beep every 5 s |

A short press (under 1 s) silences ALARM_1. ALARM_2 cannot be silenced for more
than 60 s. A long press (3 s or more) requests the ALARM_2 reset. In NORMAL, a
long press runs the functional test: buzzer, every backlight colour, relay
toggle.

## Safety design

The design goal is **fail-safe behaviour**: when the device cannot trust its own
measurement, it says so and moves to a safe state.

| Hazard | Principle | Mechanism |
|---|---|---|
| Missed alarm when the sensor is unplugged | Open input must not look like clean air | 100 kΩ pull-down on the Arduino side of each gas input; open or short detected in software (REQ-SAF-002) |
| Missed alarm from a dead or stuck sensor | A frozen reading must be detected | Stuck-value check over 10 minutes outside WARMUP (REQ-SAF-003) |
| Wrong readings from a cold sensor | Do not trust what has not warmed up | Warm-up period with alarms inhibited and shown (REQ-ALM-006) |
| Undetected firmware hang | Hardware watchdog | 500 ms watchdog, kicked only after every task has run (REQ-SAF-005) |
| Stack overflow corrupts memory | No dynamic memory, no recursion | Stack painted at boot, watermark checked every second (REQ-SAF-009/010) |
| Corrupted configuration | Redundant copies and checksums | Two EEPROM copies with CRC-16, safe defaults if both fail (REQ-CFG-001/002) |
| Power loss or crash | Fail-safe output | Normally-energised relay: the alarm side is the default (REQ-SAF-001) |
| Alarm silenced and forgotten | Latch the severe alarm | ALARM_2 is latched and not silenceable for more than 60 s (REQ-ALM-004, REQ-HMI-004) |

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
| Button, touch sensor | Operator input |
| Buzzer, relay module | Audible alarm, alarm contact |
| Temperature sensor, potentiometer | Compensation input, gas simulator |
| 100 kΩ resistors (×2) | Pull-downs on the gas inputs |

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

Full rationale and hardware requirements HW-001 to HW-003 are in
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
| `make test` | Builds and runs every `tests/test_*.c` on the PC, with AddressSanitizer and UBSan |
| `make lint` | cppcheck with the MISRA addon, clang-tidy, lizard, header self-containment, R20 grep |
| `make coverage` | Line coverage report in `build/coverage/index.html` |
| `make avr` | Cross-compiles `build/avr/gasguard.hex` for the ATmega328P |
| `make size` | Checks flash and RAM against the budget of REQ-NFR-005 |
| `make trace` | Regenerates `docs/traceability.md`; `--allow-gaps` until Day 5 |
| `make clean` | Removes `build/` |

### Flash the board

Flashing is done by hand from Windows, never from the build system:

```powershell
tools\flash.ps1 -Port COM3
```

Open the serial monitor at 38400 baud. The boot banner, reset cause and
configuration CRC status appear there once the firmware is complete (Day 4).

## Verification and quality gates

Verification follows four levels, defined in
[`docs/05-verification-plan.md`](docs/05-verification-plan.md).

| Level | Where | What | Tool |
|---|---|---|---|
| L1 Static | CI | Warnings, MISRA subset, complexity, AVR header isolation | gcc, cppcheck, clang-tidy, lizard |
| L2 Unit | CI (PC) | Each `services/` and `app/` module with host fakes | Unity, gcov |
| L3 Scenario | CI (PC) | Full detector driven by scripted ADC sequences, time-stepped | Unity, `hal/host` |
| L4 Hardware | Bench | Real board, potentiometer, then butane from an unlit lighter | Manual, logic analyser |

Scenario tests SC-01 to SC-10 and SC-13 are implemented (Day 2); SC-11, SC-12 and SC-14 to SC-16 follow on Day 4. They cover boot, alarms,
latching, sensor faults, configuration recovery, maintenance timeout, bandgap
failure and task overruns.

### Gates

| Gate | Requirement | Limit |
|---|---|---|
| Compiler | REQ-NFR-003 | Zero warnings with `-Wall -Wextra -Werror` and the extra flags of the standard |
| Static analysis | Coding standard §6 | Zero findings outside the register of deviations |
| Unit test coverage | REQ-NFR-004 | At least 90 % of lines in `app/` and `services/` (enforced from Day 2) |
| Flash | REQ-NFR-005 | At most 80 % of 32 KB |
| RAM | REQ-NFR-005 | At most 60 % of 2 KB |
| Traceability | `trace.py` | Every requirement has code and a test (enforced on Day 5) |

Accepted deviations from the coding standard are listed in
[`docs/04-coding-standard.md`](docs/04-coding-standard.md#5-deviations-register).

## Repository layout

```
common/      shared types and status codes, no hardware dependency
hal/         hardware interfaces (.h); hal/host/ holds the PC fakes
app/         detector state machine, alarm logic, channel selection (Day 2, done)
services/    filter, calibration, diagnostics (Day 2, done); config, event log (Day 4)
drivers/     LCD, buzzer, relay, button                          (Day 3)
main.c       boot sequence and scheduler                         (Day 4)
tests/       Unity unit and scenario tests, one file per module
docs/        requirements, architecture, FMEA, coding standard, verification plan
tools/       trace.py, size_check.awk, hook guard, flash script
.claude/     project rules and Claude Code skills
```

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

## Documentation

| Document | Content |
|---|---|
| [Requirements](docs/01-requirements.md) | 45 requirements in 7 areas: function, alarm, safety, interface, configuration, log, non-functional |
| [Architecture](docs/02-architecture.md) | Layers, pin mapping, wiring, state machine, scheduler, memory budget, ADRs |
| [Safety analysis](docs/03-safety-analysis.md) | FMEA with severity, occurrence, detection and residual risk |
| [Coding standard](docs/04-coding-standard.md) | MISRA C:2012 subset, project rules R01–R20, tooling, deviations |
| [Verification plan](docs/05-verification-plan.md) | Four verification levels, scenarios SC-01 to SC-16, bench procedure |
| [Traceability](docs/traceability.md) | Generated matrix, requirement → code → test |
| [Plan](PLAN.md) | Five-day implementation plan |

## License

Released under the MIT License. See [`LICENSE`](LICENSE).

Third-party code:

- [Unity](https://github.com/ThrowTheSwitch/Unity) (MIT), vendored in
  [`third_party/unity/`](third_party/unity/LICENSE.txt).
