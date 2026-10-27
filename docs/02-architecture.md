# GasGuard — Software Architecture

## 1. Layered design

```
┌──────────────────────────────────────────────────────────┐
│ main.c        boot sequence, cooperative scheduler, WDT  │
├──────────────────────────────────────────────────────────┤
│ app/          detector state machine, alarm logic,       │
│               HMI logic, maintenance, self-test          │  ← pure C, no AVR headers
├──────────────────────────────────────────────────────────┤
│ services/     filter, calibration, diagnostics, config   │
│               (CRC, A/B copies), event log, CLI parser   │  ← pure C, no AVR headers
├──────────────────────────────────────────────────────────┤
│ hal/          interfaces (.h only): adc, gpio, i2c,      │
│               eeprom, timer, uart, wdt, reset_cause      │
├───────────────────────────┬──────────────────────────────┤
│ hal/avr/   ATmega328P     │ hal/host/  fakes for tests   │
│ register-level drivers    │ (scriptable ADC, fake EEPROM │
│                           │  in RAM, fake clock…)        │
├───────────────────────────┴──────────────────────────────┤
│ drivers/      lcd_rgb (Grove LCD over hal_i2c), buzzer,  │
│               relay, button (debounce)                   │
└──────────────────────────────────────────────────────────┘
```

**Rule:** dependencies only go downwards. `app/` and `services/` never include
`<avr/*.h>`. This is what makes ≥ 90 % of the logic testable on a PC
(REQ-NFR-001) and is the main design argument of the project.

## 2. Hardware mapping (Grove Base Shield v2.1)

| Grove port | Pin | Module | Use |
|---|---|---|---|
| A0 | PC0 / ADC0 | MQ-5 generic module (AO) | Natural gas / LPG channel |
| A1 | PC1 / ADC1 | MQ-2 generic module (AO) | Combustible gas / smoke channel |
| A2 | PC2 / ADC2 | Rotary potentiometer | Gas simulator (simulation build) |
| A3 | PC3 / ADC3 | Temperature sensor | Compensation |
| D2 | PD2 | Button | Ack / reset / functional test |
| D3 | PD3 | Touch sensor | Maintenance mode entry |
| D5 | PD5 | Relay | Alarm contact output (fail-safe) |
| D6 | PD6 (OC0A) | Buzzer | Audible alarm |
| I2C | PC4/PC5 | Grove LCD RGB Backlight v4.0 | LCD @0x3E, RGB @0x62 |
| UART | PD0/PD1 (USB) | — | Log dump, CLI, debug |

> Verify the LCD RGB controller address with an I2C scan on day 3:
> v4.0 boards use 0x62; v5.0 boards use 0x30.

### 2.1 Gas sensor wiring (generic MQ modules, VCC / GND / DO / AO)

The MQ modules are not Grove modules. Each one is connected to its Grove
analog port with a Grove → female Dupont cable, **wire by wire by colour**,
through the breadboard:

| Grove wire | Signal on the shield port | MQ module pin |
|---|---|---|
| Yellow | SIG1 → A0 (port A0) / A1 (port A1) | **AO** |
| Red | 5 V | **VCC** |
| Black | GND | **GND** |
| White | SIG2 → A1 (port A0) / A2 (port A1) | **not connected — insulate** |

- **White wire:** on port A0 it is wired to the A1 pin, on port A1 to the A2
  pin. Connecting it to the module's DO output would fight the other channel
  or the simulator. It stays unconnected (HW-002).
- **DO output unused:** it is a fixed analog comparator; thresholds,
  filtering and hysteresis are done in software.
- **Pull-down 100 kΩ** between each gas input (A0, A1) and GND, placed on the
  **Arduino side** of the cable (HW-001). Without it, an unplugged cable leaves
  the ADC input floating and the reading is random instead of ≈ 0, which
  defeats REQ-SAF-002. 100 kΩ is large compared with the module's load
  resistor, so the measurement error is negligible.
- **Power:** each MQ heater draws ≈ 150 mA. Two sensors + LCD + relay fit within
  the USB 500 mA budget; never connect more than 2 MQ modules at once (HW-003).

### 2.2 Hardware requirements

These are verified on the bench (`docs/05-verification-plan.md` §4), not in
code, so they use the `HW-` prefix instead of `REQ-`.

| ID | Requirement |
|---|---|
| HW-001 | Each gas sensor input shall have a 100 kΩ pull-down to GND on the Arduino side of the cable. |
| HW-002 | The second signal wire of each Grove cable shall be left unconnected and insulated. |
| HW-003 | No more than 2 MQ sensors shall be powered from the board at the same time. |

## 3. Detector state machine

```
            power-on
               │
             INIT ──(reset cause, config load)──► SELF_TEST
                                                     │ pass        fail
                                                     ▼              │
                                                  WARMUP            │
                                                     │ timer        │
                                                     ▼              ▼
     MAINTENANCE ◄──touch 3 s── NORMAL ──≥AL1 x3──► ALARM_1     FAULT
          │  (10 min max)        ▲  ▲                  │          ▲
          └──────────────────────┘  └──<AL1-hyst x10───┘          │
                                                       │≥AL2 x3   │
                                                       ▼          │
                                                    ALARM_2 ──────┘
                                                  (latched, reset
                                                   by long press)
   Any state ──(all sensors faulty / ADC fault / overrun x3 / stack)──► FAULT
```

The state machine is a single `switch` on an `enum`, with one function per
transition guard. Each transition writes one event-log entry.

## 4. Scheduling

Cooperative, time-triggered loop driven by a 1 ms Timer0/Timer2 tick:

| Task | Period | Content |
|---|---|---|
| `task_sample` | 100 ms | ADC read all channels, filter, diagnostics |
| `task_detector` | 100 ms | State machine step |
| `task_hmi` | 50 ms | Button debounce, buzzer pattern |
| `task_display` | 250 ms | LCD + backlight refresh |
| `task_health` | 1 s | Stack watermark, bandgap check (every 60 s), overrun counter |
| `task_cli` | 10 ms | UART command parsing (non-blocking) |

No task may block. The watchdog is kicked once per loop, only if every task due
in that loop has returned (REQ-SAF-005).

## 5. Memory budget (targets)

| Resource | Budget |
|---|---|
| Flash | ≤ 25.6 KB (80 %) |
| SRAM static | ≤ 1.2 KB (60 %), leaves ≥ 800 B for stack |
| EEPROM | Config A @0x000 (64 B), Config B @0x040 (64 B), Log @0x100 (32 × 8 B) |

## 6. Key design decisions (ADR summary)

| # | Decision | Reason |
|---|---|---|
| ADR-1 | Bare-metal C, no Arduino framework | Shows register-level understanding; Arduino core is C++ |
| ADR-2 | Cooperative scheduler, no RTOS | Deterministic, analysable, fits 2 KB RAM |
| ADR-3 | HAL with host fakes | Testability on PC, CI without hardware |
| ADR-4 | Normally-energised relay | Power loss or firmware crash = alarm side (fail-safe) |
| ADR-5 | Max of valid channels | Conservative: never under-report |
| ADR-6 | Fixed-point arithmetic (0.1 %LEL units, `int16_t`) | No float on AVR: smaller, faster, deterministic |
