# Test report

Hardware test results, one section per bench session. Each entry gives the
date, the firmware (branch and commit), and the result of each step. Steps not
yet run are listed as open, so nothing is reported as passed by default.

## Day 3 — first bench session (2026-11-05)

Firmware: bring-up program on branch `day-3-bringup` (commit `d577f30` and later),
with the I²C STOP fix from `day-3-fix-i2c-stop` (`2d0d056`).

### Power

| Check | Setup | Result |
|---|---|---|
| 5V-GND on the Uno, USB port | Only USB | 4.53 V (other USB port: same) — below the 4.75 V USB minimum |
| 5V-GND on the breadboard rail | Wall adapter, no sensor | 4.75 V |
| 5V-GND with the MQ5 heater | Wall adapter, MQ5 on | **4.50 V** — the supply sags by 0.25 V with one heater |
| Supply for the Uno | 9 V barrel jack, 1 A | **Open**: not yet bought, required before any gas measurement |

### Hardware pull-downs and inputs (HW-001)

| Check | Result |
|---|---|
| A0 to GND with the 100 kΩ pull-down, no sensor | 0 V |
| A1 to GND with the 100 kΩ pull-down, no sensor | 0 V |
| A0 with the MQ5 AO wire removed | 0 V |
| A0 with the MQ5 AO wire connected | stable voltage (value depends on the gas) |

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
