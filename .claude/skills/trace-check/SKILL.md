---
name: trace-check
description: Check requirement traceability (REQ → code → tests) for GasGuard and regenerate docs/traceability.md. Use before commits, at the end of each day of the plan, and before tagging a release.
---

# Traceability check

## Steps
1. Run `python3 tools/trace.py` (same as `make trace`).
2. Read `docs/traceability.md` and the script's console output.
3. Report three lists:
   - **Not implemented**: REQ with no `@req` tag in a `.c` file of the code folders (`common/`, `hal/`, `app/`, `services/`, `drivers/`, `main.c`). Host fakes in `hal/host/` are test doubles and never count as implementation.
   - **Not verified**: REQ with code but no `@verifies` tag in `tests/`
   - **Orphans**: tags in code/tests pointing to a REQ that does not exist (typo or removed requirement)
4. Separate gaps that are **expected for the current day** of `PLAN.md` from
   real gaps.

## Output
A short table per list, then: "Traceability: X/Y requirements fully traced".
Propose the next tests or implementations to close the gaps, ordered by the
FMEA severity of the related failure mode.
