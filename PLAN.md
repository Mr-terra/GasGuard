# GasGuard — 5-day plan (≈ 4 h/day)

Each day ends with: `make check` green (gaps allowed with `--allow-gaps` until
day 5), `trace-check` skill, one `explain-module` session (30–45 min), commit.

## Day 1 — Foundations (no hardware needed)
- [x] Toolchain in WSL2: `sudo apt install build-essential gcc-avr avr-libc binutils-avr cppcheck clang-tidy gcovr python3-pip && pip install lizard`
- [x] Makefile with targets `test coverage lint avr size trace check`
- [x] Add Unity (git submodule or vendored in `third_party/unity`)
- [x] `common/gg_types/gg_types.h` (`gg_status_t`, units), HAL interfaces in `hal/*.h`, host fakes
- [x] GitHub Actions CI running `make check` on every push (green on `main` after pinning the runner to Ubuntu 22.04)
- [x] **Learn:** compilation chain (preprocessor → compiler → linker), headers vs sources, `static`

## Day 2 — Core logic, host-tested
- [x] `services/filter` (moving average) — REQ-FUN-002
- [x] `services/calib` (raw → 0.1 %LEL fixed-point, temp compensation) — REQ-FUN-003/004
- [x] `services/diag` (range, stuck-at, bandgap) — REQ-SAF-002..004
- [x] `app/detector` state machine + alarm logic — REQ-ALM-*, REQ-FUN-005 (`app/channel_select` for FUN-005)
- [x] Scenario tests SC-01..SC-10, SC-13
- [x] **Learn:** `enum` + `switch` state machines, fixed-point maths, integer promotion

## Day 3 — Hardware layer (board arrived)
- [x] Board drivers `hal/<peripheral>/<peripheral>_avr.c`: timer tick 1 ms, ADC (+ bandgap), GPIO, UART, watchdog, reset cause, EEPROM, I2C (TWI)
- [x] Wire the MQ modules per `docs/02-architecture.md` §2.1 (pull-downs, white wire insulated): MQ-5 on A0 and MQ-2 on A1, each with its 100 kΩ pull-down; 9 V jack fitted (5.12 V with both heaters, Day 5)
- [x] I2C scan → confirm LCD RGB address: 0x3E and 0x62 answer (0x70 also answers, to be identified)
- [x] `drivers/`: lcd_rgb, buzzer, relay, button (debounce), with host tests
- [x] First flash: boot banner on UART, LCD shows "GasGuard" (green backlight)
- [ ] **Learn:** registers and bit masks, `volatile`, ISRs, atomic access (quiz on the timer and channel_select done; drivers pending)

## Day 4 — Integration and robustness
- [x] `main.c` scheduler, watchdog kick policy, overrun + stack watermark — REQ-SAF-005..009 (stack painting and check: AVR code compiled, not yet measured on the board)
- [x] `services/config` A/B + CRC-16, `services/eventlog` ring buffer, CLI `log` — REQ-CFG-*, REQ-LOG-* (`status` and `cfg` not done: not required by a REQ; planned)
- [x] HMI: patterns, ack/reset, functional test, maintenance mode, LCD concentration — REQ-HMI-* (FMEA F36 accepted by the author)
- [x] Scenario tests SC-11, SC-12, SC-14, SC-16 (SC-15 in unit tests)
- [~] Bench test with potentiometer (simulation build), then real gas sensors: real sensors done on Day 5 (lighter gas, ALARM_1/ALARM_2, latch, silence, maintenance, functional test); the simulation build was never flashed
- [ ] **Learn:** CRC, EEPROM wear, watchdog design, why fail-safe relay
- [~] **Remove DEV-05** (docs/04 §5): removed where `app/` or the tests call the function; kept on the prototypes that only `main.c` calls (cppcheck does not lint `main.c`)
- [x] **Replace the TODO in `app/detector/detector.c`** (`default` branch of `step_state`) with `diag_raise()`, once the event log exists

## Day 5 — Evidence and presentation
- [x] `make check` without `--allow-gaps`: 100 % traceability (45/45, strict run of `tools/trace.py`)
- [~] `misra-review` on the whole tree: cppcheck MISRA and clang-tidy pass on every library and test file; `main.c` is outside the cppcheck scope and was reviewed by hand only
- [~] Hardware test report `docs/test-report.md` + logic-analyzer screenshots: Day 5 board session recorded (alarms, latch, HMI, log, bandgap fix, calibration); open: relay, watchdog trip, stack value. Screenshots: none so far, no logic analyser on the bench; one can be added if a timing check needs it
- [ ] README: photo, demo GIF/video, quality badges (needs the bench for the photo and demo; wiring section exists)
- [x] Short section "How this project was built with Claude Code" (the `.claude/` setup as quality system)
- [x] Tag `v1.0` (after the author validates Day 5): on `cc7e4f5`
- [ ] **Learn:** full `explain-module` pass on `detector` and `diag` — mock interview

## Day 6 — On-board calibration (decided by the author on 2026-11-17)
Calibrate the gas zero offsets on the board itself, with no PC, and warn when
they are out of date. Requirement texts approved by the author: REQ-FUN-007,
REQ-HMI-011; the calibration requirements are written from the design below and
shown to the author before the code.
- [ ] Requirements in `docs/01`: REQ-FUN-007 (`ZERO_LOW` event: filtered raw more than 5 LSB under the zero for 1 h, once per channel per boot), REQ-HMI-011 (`RECAL` next to `NORMAL` on line 1, no backlight or buzzer change, hidden in ALARM_1, ALARM_2 and FAULT), and the calibration requirements
- [ ] Calibration design: start with button + touch held together for 5 s in NORMAL; 72 h with detection fully active on the old offsets, `CAL` and the hours left on the LCD, lowest filtered raw kept per channel; cancelled (old offsets kept, logged) on ALARM, FAULT, MAINTENANCE, an invalid channel or a reset; at the end the LCD shows old and new offsets, stored only on a long press (≥ 3 s) within 10 min
- [ ] Guards against calibrating a slow leak as clean air: ceiling per channel (MQ-5 160, MQ-2 80) and at most ±30 raw of change per calibration; a result outside is refused and logged
- [ ] `services/config`: zero offsets stored in the A/B copies with their CRC (REQ-CFG-001, completes REQ-FUN-003); the compiled-in offsets (MQ-5 111, MQ-2 35) become the defaults; handling of the copies written by the older, shorter layout
- [ ] Tests first: new unit and scenario tests for `ZERO_LOW`, `RECAL`, the calibration (start, cancel cases, guards, accept, timeout), and the update of the existing tests (config size and layout, LCD line 1, firmware zero offsets read from the configuration)
- [ ] `safety-impact` (new FMEA rows: slow leak calibrated as clean air, wrong gesture, offsets lost in EEPROM), docs/02 state machine and LCD, docs/05, README, `test-report` bench steps
- [ ] Bench: full 72 h calibration on the board, `RECAL` forced with a raised offset

## Version 2 (during the hiring process)
Modbus RTU over UART (industrial standard), MQ7 CO channel with heater cycling,
PC simulator with plots, low-power mode, HIL test with a second board.

---

## Day 1 kick-off prompt for Claude Code

> Read CLAUDE.md and all files in docs/. Then do Day 1 of PLAN.md.
> Start by proposing the directory tree and the Makefile design, wait for my
> approval, then implement. Use the `new-module` skill for each HAL interface.
> At the end run `make check` (traceability with --allow-gaps) and summarise.
