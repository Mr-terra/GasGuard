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

## Build

```bash
# WSL2 / Ubuntu
sudo apt install build-essential gcc-avr avr-libc binutils-avr cppcheck clang-tidy gcovr
pip install lizard
make check        # tests, coverage, static analysis, traceability, AVR build
```

Flash from Windows: `tools/flash.ps1 -Port COM3`.

## How this project was built

Code was produced with Claude Code inside a quality system I designed: the
[`.claude/`](.claude) folder defines the project rules (`CLAUDE.md`), a hook
that rejects forbidden constructs on every edit, and skills for MISRA review,
requirement-based test writing, FMEA impact analysis and traceability checks.
Every change goes through tests, static analysis and review before commit —
the AI writes code; the process decides whether it is accepted.

## License

MIT
