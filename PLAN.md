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
- [ ] Wire the MQ modules per `docs/02-architecture.md` §2.1 (pull-downs, white wire insulated). Partly done: MQ5 on A0 with its pull-down, A1 channel and MQ on A1 still to wire; the supply limit (4.5 V with MQ5 on USB) is open, see `docs/test-report.md`
- [x] I2C scan → confirm LCD RGB address: 0x3E and 0x62 answer (0x70 also answers, to be identified)
- [x] `drivers/`: lcd_rgb, buzzer, relay, button (debounce), with host tests
- [x] First flash: boot banner on UART, LCD shows "GasGuard" (green backlight)
- [ ] **Learn:** registers and bit masks, `volatile`, ISRs, atomic access (quiz on the timer and channel_select done; drivers pending)

## Day 4 — Integration and robustness
- [x] `main.c` scheduler, watchdog kick policy, overrun + stack watermark — REQ-SAF-005..009 (stack painting and check: AVR code compiled, not yet measured on the board)
- [x] `services/config` A/B + CRC-16, `services/eventlog` ring buffer, CLI `log` — REQ-CFG-*, REQ-LOG-* (`status` and `cfg` not done: not required by a REQ; planned)
- [x] HMI: patterns, ack/reset, functional test, maintenance mode, LCD concentration — REQ-HMI-* (FMEA F36 decision pending)
- [x] Scenario tests SC-11, SC-12, SC-14, SC-16 (SC-15 in unit tests)
- [ ] Bench test with potentiometer (simulation build), then real gas sensors
- [ ] **Learn:** CRC, EEPROM wear, watchdog design, why fail-safe relay
- [~] **Remove DEV-05** (docs/04 §5): removed where `app/` or the tests call the function; kept on the prototypes that only `main.c` calls (cppcheck does not lint `main.c`)
- [ ] **Replace the TODO in `app/detector/detector.c`** (`default` branch of `step_state`) with `diag_raise()`, once the event log exists

## Day 5 — Evidence and presentation
- [ ] `make check` without `--allow-gaps`: 100 % traceability
- [ ] `misra-review` on the whole tree, fix or register deviations
- [ ] Hardware test report `docs/test-report.md` + logic-analyzer screenshots
- [ ] README: photo, wiring, demo GIF/video, quality dashboard (badges: CI, coverage)
- [ ] Short section "How this project was built with Claude Code" (the `.claude/` setup as quality system)
- [ ] Tag `v1.0`
- [ ] **Learn:** full `explain-module` pass on `detector` and `diag` — mock interview

## Version 2 (during the hiring process)
Modbus RTU over UART (industrial standard), MQ7 CO channel with heater cycling,
PC simulator with plots, low-power mode, HIL test with a second board.

---

## Day 1 kick-off prompt for Claude Code

> Read CLAUDE.md and all files in docs/. Then do Day 1 of PLAN.md.
> Start by proposing the directory tree and the Makefile design, wait for my
> approval, then implement. Use the `new-module` skill for each HAL interface.
> At the end run `make check` (traceability with --allow-gaps) and summarise.
