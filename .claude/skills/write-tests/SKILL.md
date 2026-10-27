---
name: write-tests
description: Write Unity unit or scenario tests for a GasGuard module, derived from its requirements, before or alongside the implementation. Use when creating or changing any module in app/, services/ or drivers/.
---

# Write tests from requirements

## Steps
1. Find the requirements the module implements (`grep -n '@req' src/<module>.c`,
   or the REQ IDs given by the user) and read them in `docs/01-requirements.md`.
2. For each requirement, list test cases in this order:
   - **Nominal** behaviour
   - **Boundaries**: exactly at threshold, threshold − 1, threshold + 1, min, max
   - **Invalid inputs**: `NULL` pointers, out-of-range values → expected `gg_status_t`
   - **Timing**: counters (3 samples to trigger, 10 to clear…), timeouts
   - **Fault injection** using `hal/host` fakes (ADC script, corrupted EEPROM, clock jump)
3. Write tests in `tests/test_<module>.c` with Unity:
   - one behaviour per test, name `test_<module>_<behaviour>`
   - comment `/* @verifies REQ-XXX-NNN */` above each test
   - `setUp()` resets the module and fakes to a known state
4. For state-machine behaviour spanning time, write a **scenario test** in
   `tests/scenarios/` using the step helpers (`sim_step_ms()`, `sim_set_adc()`).
5. Run `make test` and `make coverage`. Report coverage for the module; add
   tests for uncovered branches until ≥ 90 % lines.

## Rules
- Never weaken an assertion to make a test pass: if the code is wrong, say so.
- Tests must be deterministic: no real time, no randomness without fixed seed.
- If a requirement is untestable or ambiguous, report it instead of guessing.
