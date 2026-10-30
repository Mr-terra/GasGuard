# Learning log

Interview-preparation sessions run with the `explain-module` skill. One entry per
session: date, module, score out of 5, and the concepts to review before the next
session.

## 2026-10-29 — `hal/host/hal_adc_fake.c` (Day 1)

- **Score : 3,5 / 5**

| # | Theme | Result |
|---|---|---|
| 1 | `static` and linkage | Correct, with the link-time conflict added |
| 2 | `volatile` and ISR | Partial: race identified, main cause (optimisation, re-reading memory) missed |
| 3 | Relay energised when healthy | Correct principle, vocabulary to fix (energised when healthy, released on fault) |
| 4 | Watchdog kick at end of loop | Correct, with the nuance that the kick proves a full pass ran |
| 5 | ADC input and pull-down | Partial: unplugged input floats; pull-down holds it near 0 V; the module is not powered through the signal pin |

**Concepts to review**

1. `volatile` and atomicity: why `volatile` forces re-reading and why it does not make a multi-byte read atomic (`ATOMIC_BLOCK`, R15).
2. Floating inputs and pull-down resistors: why an unplugged ADC input reads noise, and how the 100 kΩ pull-down turns it into a detectable 0 (REQ-SAF-002, HW-001).

## 2026-10-29 — Revision: `volatile`, pull-down, relay (review of the session above)

Model answers to check before the next session.

**1. `volatile` and atomicity.** Without `volatile`, the compiler may keep `v_tick` in a register and never re-read memory, so a loop waiting for an ISR can spin forever. `volatile` forces a read at every access, which gives visibility. It does not make a multi-byte access atomic: a 16-bit value can still be read half-updated while an ISR writes it. Protect such reads with `ATOMIC_BLOCK` (R15).

```c
static volatile uint8_t v_tick;                  /* visible to the ISR and the loop */
ISR(TIMER0_COMPA_vect) { v_tick++; }

static uint16_t read_counter(void) {
    uint16_t copy;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { copy = v_counter; }
    return copy;
}
```

**2. Floating input and pull-down.** An unplugged ADC input has a very high impedance, so its voltage is undefined: noise, drift, or a plausible value. The 100 kΩ pull-down ties the input to GND, so an open circuit reads about 0 LSB. That 0 is then detected as an open circuit (REQ-SAF-002, `raw <= 5`). The pull-down is on the signal line, not in the power path: the module is powered from the 5 V and GND wires, and the current through the resistor is about 50 µA.

**3. Relay.** The relay is energised when the system is healthy and released on fault or power loss (REQ-SAF-001). A failure that cuts the coil drive therefore moves the output to the alarm side without any software action. Software faults are covered separately: the state machine goes to FAULT and de-energises the relay explicitly (REQ-SAF-007).

**Still open:** the "compilation chain" item of PLAN.md Day 1 is not ticked. It has not been covered by an `explain-module` session yet.

## 2026-10-29 — `compilation chain` (Day 1 learning item)

- **Score : partial at first, validated on the third attempt**

| Question | First answer | Result |
|---|---|---|
| Why a `.h` must not contain code | `.h` holds declarations; did not know the consequence | Partial: a body copied into every `.c` gives "multiple definition" at link time |
| Why the test compiles and how it links to the fake | HAL is an abstraction; fake treated as a data file | Partial: the header prototype is enough for the compiler; the linker finds the symbol in the file listed by the `Makefile` |
| Who decides which implementation is used | Expected a runtime detection | Corrected: the make target chooses the source list (`LIB_SRCS` for `make test`, `AVR_SRCS` for `make avr`) |

**Final restatement (validated):** the test compiles with the prototype from `hal_adc.h`; the linker resolves `hal_adc_read` against the file the make target provides, the fake on the PC or the AVR driver on the board. The choice is made at build time, not at run time.

**Concepts to review**

1. Linkage and `static` at the symbol level: check the output of `nm` on an object file (`T` global, `b`/`t` local).
2. Include guards and the cost of definitions in headers (flash duplication, `multiple definition`).

## 2026-10-30 — Day 2 Learn: `enum` + `switch`, fixed-point maths, integer promotion

- **Validated** on the reformulated questions 1 (integer promotion), 3 (`uint32_t` product: 1023 × 65 535 = 67 042 305 overflows `uint16_t`) and 4 (`>>` versus `/`). Question 2 was answered with corrections.

| # | Theme | Result |
|---|---|---|
| 1 | Integer promotion: `a + b > 255` with two `uint8_t` | Correct: operands are promoted to `int`, so `a + b` is 300. Wording to fix: the modulo 256 only happens when the result is stored in a `uint8_t` (`(uint8_t)300 == 44`). |
| 2 | Listing every `enum` value in a `switch` | Correct. Also keep `default` for impossible values. With `-Wswitch-enum` (in `Makefile`), a new state without a `case` is a compile error, not a silent fall-through to FAULT. |
| 3 | Why the product is `uint32_t` | Validated: 1023 × 65 535 = 67 042 305 overflows `uint16_t` (65 535), fits `uint32_t` (4 294 967 295). worst case 1023 × 65535 = 67 042 305 does not fit in `uint16_t`. Even 1023 × 2560 = 2 618 880 does not. |
| 4 | `>>` versus `/` on a negative value | Correct: `>>` on a negative value is implementation-defined (gcc floors, −5 >> 1 = −3); `/` truncates towards zero (−5 / 2 = −2), defined by C99. |

**Concepts to review**

1. Worst-case range of intermediate results: compute the maximum of every product before choosing its type (question 3).
2. Where promotion stops: the value only wraps when it is converted to a narrower type.
