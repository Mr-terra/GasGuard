# GasGuard

**Fail-safe combustible gas detector firmware — bare-metal C99 on ATmega328P.**

GasGuard monitors natural gas / LPG and combustible gases with two
semiconductor sensors, raises a two-level alarm with a latched high alarm,
drives a fail-safe relay output and continuously checks its own health
(sensor open/short/stuck, ADC reference, watchdog, task timing, stack, EEPROM
integrity).

> ⚠️ Demonstration project — **not a certified safety device.** Do not rely on
> it to protect people or property.

## Why this project

Built to show how I approach safety-related embedded software: requirements
first, a documented FMEA, a MISRA-inspired coding standard, host-based unit and
scenario tests, and full requirement-to-test traceability.

## Highlights

- **Fail-safe by design** — normally-energised alarm relay: power loss, crash or
  fault all end on the alarm side.
- **Self-diagnostics** — sensor open/short/stuck detection, ADC bandgap check,
  watchdog, overrun and stack-watermark monitoring.
- **Robust storage** — A/B configuration copies with CRC-16 and safe defaults,
  EEPROM ring-buffer event log.
- **Testable architecture** — application logic has zero AVR dependency and
  runs on a PC with hardware fakes (≥ 90 % coverage target).
- **Traceability** — every requirement is linked to code and tests,
  generated in [`docs/traceability.md`](docs/traceability.md).

## Hardware

Elegoo Uno R3 (ATmega328P), Seeed Grove Base Shield v2.1, MQ-5 and MQ-2 gas
sensor modules (with 100 kΩ pull-downs for open-circuit detection), Grove LCD
RGB Backlight, button, touch sensor, buzzer, relay, temperature sensor,
potentiometer (gas simulator), 8-channel logic analyzer (PulseView) for bus
verification. Pin mapping and wiring:
[`docs/02-architecture.md`](docs/02-architecture.md#2-hardware-mapping-grove-base-shield-v21).

## Documentation

| Document | Content |
|---|---|
| [Requirements](docs/01-requirements.md) | 45 numbered requirements |
| [Architecture](docs/02-architecture.md) | Layers, state machine, scheduler, ADRs |
| [Safety analysis](docs/03-safety-analysis.md) | FMEA with residual risk |
| [Coding standard](docs/04-coding-standard.md) | MISRA C:2012 subset + tooling |
| [Verification plan](docs/05-verification-plan.md) | Unit, scenario and bench tests |

## Status

Day 1 of a 5-day plan ([`PLAN.md`](PLAN.md)). Done so far:

- Build and test harness (Makefile, vendored [Unity](https://github.com/ThrowTheSwitch/Unity) v2.6.0).
- Fixed-width status and concentration types (`common/gg_types.h`).
- Host-testable ADC interface with a scriptable fake (`hal/`).
- Interface headers for timer, GPIO, EEPROM, UART, I2C, watchdog and reset cause.
- Static analysis gates (cppcheck MISRA, clang-tidy, lizard) and a memory budget check.
- Continuous integration on `main`.

Not started yet: the state machine, alarm logic, filtering and calibration
(Day 2), the AVR drivers (Day 3), and the scheduler and event log (Day 4).
Requirement coverage is tracked in [`docs/traceability.md`](docs/traceability.md).

## Repository layout

```
common/      shared types and status codes (no hardware dependency)
hal/         hardware interfaces (.h); hal/host/ holds the PC fakes
app/         detector state machine and alarm logic      (Day 2)
services/    filter, calibration, diagnostics, config    (Day 2+)
drivers/     LCD, buzzer, relay, button                  (Day 3)
main.c       boot sequence and scheduler                 (Day 4)
tests/       Unity unit tests, one file per module
docs/        requirements, architecture, FMEA, coding standard, test plan
tools/       trace.py, size_check.awk, hook guard, flash script
```

Layers depend only downwards. `app/` and `services/` never include AVR
headers, so their logic runs on the PC unchanged.

## Build

```bash
# WSL2 / Ubuntu
sudo apt install build-essential gcc-avr avr-libc binutils-avr cppcheck clang-tidy gcovr
pip install --user lizard
make check        # everything below, in order
```

| Target | What it does |
|---|---|
| `make test` | Builds and runs every `tests/test_*.c` on the PC (with ASan and UBSan) |
| `make lint` | cppcheck with the MISRA addon, clang-tidy, lizard, header self-containment, R20 grep |
| `make coverage` | Line coverage report in `build/coverage/index.html` |
| `make avr` | Cross-compiles `build/avr/gasguard.hex` for the ATmega328P |
| `make size` | Checks flash and RAM against the budget of REQ-NFR-005 |
| `make trace` | Regenerates `docs/traceability.md` (`--allow-gaps` until Day 5) |

Flashing is done by hand from Windows, never from the build:
`tools/flash.ps1 -Port COM3`.

Coding rules and the list of accepted deviations are in
[`docs/04-coding-standard.md`](docs/04-coding-standard.md).

## Development workflow

- One branch per plan day (`day-1`, `day-2`, …), merged into `main` with a
  fast-forward after rebase, once the day is validated.
- Conventional commits, one logical change each. Tests are written before the
  code (TDD), so a `test(...)` commit may be red on a day branch.
- CI runs `make check` on every push to `main`. Other branches are checked on
  demand, with the manual trigger or a `ci-*` tag.

## How this project was built

Code was produced with Claude Code inside a quality system I designed: the
[`.claude/`](.claude) folder defines the project rules (`CLAUDE.md`), a hook
that rejects forbidden constructs on every edit, and skills for MISRA review,
requirement-based test writing, FMEA impact analysis and traceability checks.
Every change goes through tests, static analysis and review before commit —
the AI writes code; the process decides whether it is accepted.

## License

MIT
