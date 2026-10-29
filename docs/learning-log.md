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
