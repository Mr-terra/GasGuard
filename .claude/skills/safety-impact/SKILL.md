---
name: safety-impact
description: Analyse the safety impact of a code or requirement change against the GasGuard FMEA (docs/03-safety-analysis.md) and update it. Use whenever a change touches alarms, diagnostics, the relay, the watchdog, configuration storage, timing, or the state machine.
---

# Safety impact analysis

## Steps
1. Describe the change in one sentence and list the files and REQ IDs touched.
2. Read `docs/03-safety-analysis.md`. For each FMEA row whose mitigation or
   requirement is touched, answer:
   - Is the mitigation still effective? Could the change disable it in some state
     (warm-up, maintenance, fault, during a config write, after a reset)?
   - Does the change create a **new failure mode**? Consider: wrong value,
     no value, late value, stuck value, spurious value, corrupted memory.
3. Check the fail-safe principles:
   - Power loss / reset / hang → relay de-energised (alarm side)?
   - Any "cannot trust measurement" path → FAULT?
   - Any operator action that can permanently inhibit protection?
4. Verify each affected mitigation still has a test (`make trace`); propose a
   missing scenario test if needed.

## Output
- **Impact summary**: NONE / LOW / HIGH, one paragraph why.
- Updated FMEA rows (or new rows F19…) with S/O/D and RPN, written into the doc.
- List of tests to add or update.
- If any residual RPN > 20 without justification: say **"BLOCKED — needs a
  decision from the author"** and do not proceed with the commit.
