---
name: pin-map-check
description: Check that the pin mapping in docs/02-architecture.md section 2 matches the code (GPIO bit defines, ADC channels, timer, UART and I2C pins) and the wiring notes. Use after any change to a pin, a driver's pin constant, or the Grove wiring, and before any hardware bring-up.
---

# Pin-map check

The pin table in `docs/02-architecture.md` (section 2, "Hardware mapping") is
the reference. The code must agree with it. A mismatch is a silent defect:
the firmware compiles and runs, but drives the wrong pin.

## Steps
1. Read the table in `docs/02-architecture.md` section 2 and the wiring notes
   of the same section (MQ modules, Grove cables, white wire).
2. Collect the pins from the code:
   - `hal/gpio/gpio_avr.c`: the `GPIO_BIT_*` defines (port and bit).
   - `hal/adc/adc_avr.c`: the channel mapping (ADC channel number = `PCn`).
   - `hal/timer/timer_avr.c`: the timer used (must be Timer2, not Timer0,
     which is reserved for the buzzer PWM on PD6 / OC0A).
   - `hal/uart/uart_avr.c`: the UART pins (PD0/PD1), and the baud rate.
   - `hal/i2c/i2c_avr.c`: the TWI pins (PC4/PC5, from the hardware).
3. Compare each row with the code and classify each one:
   - **OK**: the same port, bit and function.
   - **MISMATCH**: different port, bit, or function. Report file and line.
   - **UNDOCUMENTED**: a pin used in code that is not in the table.
   - **UNUSED**: a row in the table that no code uses yet (expected before
     the module is written; say which day it belongs to, from `PLAN.md`).
4. Check the rule that the buzzer pin is shared with Timer0 OC0A: the buzzer
   must not be driven as plain GPIO while the PWM runs. Flag any code that
   writes `PORTD` bit 6 outside the buzzer driver.
5. Check the relay pin against REQ-SAF-001 and FMEA F21: the relay driver
   must be initialised de-energised, before anything else, and the pull-down
   proposal (HW-004) must not be assumed to exist.

## Output
A table: `Function | Docs (section 2) | Code (file:line) | Status`, then the
count per status and one verdict: **PASS** (no mismatch) or **FAIL**.

## Rules
- Read the code; do not trust the defines' comments to describe the pin.
- A MISMATCH is a blocker: fix the code or the table in the same commit, and
  say which one was wrong. Never change the docs to match a wrong code without
  asking the author.
- This skill checks consistency only. It does not validate the schematic or
  the physical wiring; those need the hardware bring-up (`docs/05`).
