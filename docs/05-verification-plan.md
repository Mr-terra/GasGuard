# GasGuard — Verification Plan

## 1. Levels

| Level | Where | What | Tool |
|---|---|---|---|
| L1 Static | CI | Warnings, MISRA subset, complexity, AVR-header isolation | gcc, cppcheck, clang-tidy, lizard, grep |
| L2 Unit | CI (PC) | Each `services/` and `app/` module with host fakes | Unity, gcov |
| L3 Scenario | CI (PC) | Full detector fed with scripted ADC sequences (time-stepped simulation) | Unity + `hal/host` |
| L4 Hardware | Bench | Real board, potentiometer, then real gas (butane from an unlit lighter) | Manual + logic analyzer |

## 2. Test naming and tracing

- Test function name: `test_<module>_<behaviour>()`.
- Each test carries a comment tag `/* @verifies REQ-XXX-NNN */`.
- `tools/trace.py` builds `docs/traceability.md` (REQ → code → tests) and fails
  CI if a requirement has no implementation or no test.

## 3. Mandatory scenario tests (L3)

| ID | Scenario | Expected |
|---|---|---|
| SC-01 | Boot, healthy sensors | INIT → SELF_TEST → WARMUP → NORMAL after warm-up time |
| SC-02 | Gas ramp 0 → 30 %LEL | ALARM_1 at ≥ 10 % (3 samples), ALARM_2 at ≥ 25 % |
| SC-03 | Single spike 50 %LEL for 1 sample | No alarm |
| SC-04 | Oscillation 9–11 %LEL | ALARM_1 does not chatter |
| SC-05 | ALARM_2 then gas drops, no reset | Stays ALARM_2 |
| SC-06 | ALARM_2, gas gone, long press | NORMAL |
| SC-07 | ALARM_2, gas still present, long press | Stays ALARM_2 |
| SC-08 | MQ5 unplugged (raw 0) | Channel fault, MQ2 still used, no FAULT |
| SC-09 | Both sensors unplugged | FAULT, relay off |
| SC-10 | Sensor stuck 10 min | Channel fault |
| SC-11 | Config A corrupted | Config B used, A repaired, event logged |
| SC-12 | Config A and B corrupted | Defaults used, event logged |
| SC-13 | Gas during warm-up | No alarm, "WARMUP" shown |
| SC-14 | Maintenance for 11 min | Auto-exit at 10 min |
| SC-15 | Bandgap out of range | FAULT |
| SC-16 | 3 consecutive task overruns | FAULT |

## 4. Hardware test procedure (L4) — summary

1. Flash, open UART at 38400. Check boot banner, reset cause, config CRC OK.
2. Functional test (long press in NORMAL): buzzer, colours, relay click.
3. Simulation build: turn potentiometer → check thresholds, hysteresis, latch.
4. Unplug each sensor cable → raw value ≈ 0 on UART (`status`), channel fault
   on display and log (verifies HW-001). Check with a multimeter that the white
   Grove wires are not connected (HW-002).
5. Unplug both → FAULT, relay released (measure with multimeter / LED).
6. Real gas: briefly release butane from an **unlit** lighter near the sensor,
   in a ventilated room, far from any flame → ALARM_1/2.
7. Capture I2C frames to the LCD with the logic analyzer; screenshot in `docs/img/`.
8. Dump the log (`log` command) and attach it to the test report.

Results are recorded in `docs/test-report.md` (date, firmware hash, pass/fail per step).
