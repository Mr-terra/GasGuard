---
name: write-tests
description: Write Unity unit or scenario tests for a GasGuard module, derived from its requirements, before or alongside the implementation; every requirement gets positive, negative, false-positive, false-negative and limit cases. Use when creating or changing any module in app/, services/ or drivers/.
---

# Write tests from requirements

## Steps
1. Find the requirements the module implements (`grep -rn '@req' <layer>/<module>/<module>.c`,
   or the REQ IDs given by the user) and read them in `docs/01-requirements.md`.
2. For each requirement, list test cases in **all five categories** below.
   None may be skipped. If a category truly does not apply, the matrix of
   step 6 says why in one line.
   - **Positive**: the expected action happens when its condition is met
     (gas above AL1 for 3 samples gives ALARM_1).
   - **Negative**: nothing happens when the condition is not met (gas under
     AL1, a gesture in the wrong state, a `NULL` pointer or an out-of-range
     value returns the expected `gg_status_t` and changes nothing).
   - **False positive**: a near miss must not trigger. This covers noise, a single
     spike, a short pulse, a condition that clears just before its time, or a
     neighbouring input (a button alone where button + touch is needed). It
     guards against nuisance alarms (hazard H2).
   - **False negative**: a real case must not be missed. This covers a condition
     interrupted and restarted, a reset in the middle, the other channel or a
     fault hiding the event, or a slow change. It guards against a missed alarm
     (hazard H1), so it comes first in a safety review.
   - **Limits**: exact boundary values on both sides (threshold − 1,
     threshold, threshold + 1; time limit − 1 tick, limit; min and max of the
     type, 0 and 1023 for raw values). Timing counters and timeouts are
     limits too.

   Use the `hal/*/*_fake.c` fakes (ADC script, corrupted EEPROM, clock jump) to
   inject the faults that the false-negative and limit cases need.
3. Write tests in `tests/<layer>/<module>/test_<module>.c` with Unity:
   - one behaviour per test, name `test_<module>_<behaviour>`
   - comment `/* @verifies REQ-XXX-NNN */` above each test
   - `setUp()` resets the module and fakes to a known state
4. For state-machine behaviour spanning time, write a **scenario test** in
   `tests/scenarios/` using the step helpers (`sim_step_ms()`, `sim_set_adc()`).
5. Run `make test` and `make coverage`. Report coverage for the module; add
   tests for uncovered branches until ≥ 90 % lines.
6. Report a **case matrix**: one row per requirement, one column per
   category, each cell naming the test function(s), or "n/a: <reason>".

   | Requirement | Positive | Negative | False positive | False negative | Limits |
   |---|---|---|---|---|---|

   An empty cell is a gap: write the test, or ask the author.

## Rules
- Never weaken an assertion to make a test pass: if the code is wrong, say so.
- Tests must be deterministic: no real time, no randomness without fixed seed.
- If a requirement is untestable or ambiguous, report it instead of guessing.
- The five categories apply to every new or changed requirement, including
  updates of existing tests: a changed threshold or delay needs its limit
  cases rewritten, not only the nominal test.
- Name a test after the behaviour it checks, not after its category
  (`test_detector_single_spike_does_not_alarm`, not `test_false_positive_1`).

## Gotchas
- Unity test function names must be unique within one test binary. A helper
  defined `static` in two test files can collide at link time or break
  MISRA 5.9.
- `setUp()` must reset the fakes too. A fake left in a faulty state by one
  test makes the next test fail for no visible reason.
- An unreachable `default:` branch is not a reason to skip coverage. Reach it
  through a fake, or document it as a deviation; never loosen the assertion.
