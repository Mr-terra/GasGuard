# 07 — Hardware

The parts, supplies and tools used to build and test GasGuard. The pin table
and the wiring rules are in [`02-architecture.md`](02-architecture.md) §2; the
bench results are in [`test-report.md`](test-report.md). A part marked
**to confirm** has a model or a value that the author still has to read on the
part itself.

## 1. Bill of materials

| # | Part | Model | Qty | Connected to | Status | Notes |
|---|---|---|---|---|---|---|
| 1 | Microcontroller board | Elegoo Uno R3 (ATmega328P, 16 MHz, 32 KB flash, 2 KB SRAM, 1 KB EEPROM), optiboot bootloader | 1 | — | In use | The bootloader clears the reset flags, so the reset cause reads as unknown (FMEA F23) |
| 2 | Connector shield | Seeed Grove Base Shield v2.1 | 1 | On the Uno | In use | Its VCC switch (3.3 V / 5 V) selects the supply of every Grove port: it is on 5 V: 5 V was measured between MQ5 - VCC and MQ5 - GND, which are fed through a Grove port |
| 3 | Gas sensor, natural gas / LPG | Generic MQ-5 module (pins VCC, GND, DO, AO) | 1 | Port A0 (AO), see §2 | In use | Heater about 150 mA. Zero offset 113 (provisional, REQ-FUN-003) |
| 4 | Gas sensor, combustible gas / smoke | Generic MQ-2 module (pins VCC, GND, DO, AO) | 1 | Port A1 (AO), see §2 | In use | Heater about 150 mA. Zero offset 0 until its preheat is over |
| 5 | Display | Seeed Grove LCD RGB Backlight v4.0 (16×2) | 1 | Port I2C | In use | LCD at 0x3E, backlight at 0x62 (v5.0 boards use 0x30) |
| 6 | Push button | Seeed Grove Button v1.2 (Grove Starter Kit for Arduino), output high when pressed | 1 | Port D2 (SIG) | In use | Short press: silence; long press: ALARM_2 reset or functional test |
| 7 | Touch sensor | Seeed Grove Touch Sensor v1.1 (same kit), 5 V when touched, 0 V otherwise | 1 | Port D3 (SIG) | In use | 3 s touch in NORMAL: MAINTENANCE |
| 8 | Buzzer | Seeed Grove Buzzer v1.2 (same kit), active buzzer | 1 | Port D6 (SIG) | In use | Switched on and off as a plain output; Timer0 is left free |
| 9 | Gas simulator | Seeed Grove Rotary Angle Sensor v1.2 (same kit) | 1 | Port A2 | Bench only | Read only by the simulation build (`make avr-sim`, FMEA F41) |
| 10 | Pull-down resistors | 100 kΩ, 1/4 W | 2 | A0–GND and A1–GND, Arduino side | In use | HW-001: an unplugged cable reads 0 instead of noise |
| Pull-down resistor | 1 MΩ, 1/4 W | 1 | A3–GND, Arduino side | In use | HW-005: an unplugged temperature sensor reads 0 |
| 11 | Cables | Grove to female Dupont | 2 | MQ-5, MQ-2 | In use | White wire not connected, insulated (HW-002) |
| 12 | Breadboard and jumper wires | — | 1 | Pull-downs, MQ wiring | In use | — |

No relay module is fitted. The firmware still drives the relay output on D5
(REQ-SAF-001): high when healthy, low on alarm, fault and reset. On the bench
that output is checked by its pin level with the multimeter. The
pull-down proposed for the relay input (HW-004, FMEA F21) applies only once a
relay module is fitted.

Temperature sensor: Seeed Grove Temperature Sensor v1.2 (same kit, NTC
thermistor, 100 kOhm at 25 degC, B = 4275) on A3: VCC to Arduino 5V, GND to
Arduino GND, SIG to Arduino A3 (Dupont wires). A 1 MΩ pull-down from Arduino A3
to Arduino GND (HW-005) makes an unplugged sensor read 0. The firmware reads it
every sample, accepts 0 to 50 degC (raw 200 to 750), and prints it with
`status`; its compensation coefficient stays at 0 until it is measured
(REQ-FUN-004).

## 2. Supply

| Part | Value | Notes |
|---|---|---|
| Wall adapter on the barrel jack | 9 V DC, 1 A | Measured 5.12 to 5.15 V on the 5V pin with both heaters on |
| USB cable (type B) | — | Flashing and serial log (38400 baud, 8N1); the board stays powered by the jack |

On a weaker supply the 5 V rail sagged by 0.25 V with one heater on (test
report, Day 5). No more than two MQ modules on the board (HW-003).

## 3. Tools

| Tool | Use |
|---|---|
| Multimeter | Supply voltages, sensor outputs (A0, A1), the dead-heater check |
| PC with Windows, avrdude, WSL Ubuntu | Flashing (`tools/flash.ps1`), build and checks (`make check`) |
| Butane lighter, **unlit** | Gas test of the sensors (README, "Periodic gas test") |

Not used on the bench: oscilloscope, logic analyser. Voltages are read with the
multimeter; timing is checked by ear, by counting, or from the event log.

## 4. Safety notes for the bench

- The bench never switches mains: GasGuard is a learning project, not a
  certified detector (README, limitations).
- Gas tests use a short puff from an unlit lighter, in a ventilated room, away
  from any flame.
