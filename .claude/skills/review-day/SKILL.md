---
name: review-day
description: Write the end-of-day review for a GasGuard plan day in the five-part format required by CLAUDE.md (done, checks, gaps against PLAN.md, proposals with effort and risk, decisions to confirm), then ask the author which proposals to implement. Use at the end of every plan day, before any push.
---

# End-of-day review

The review hands the day over to the author. It proposes; it does not
implement. Implement nothing until the author answers, as CLAUDE.md requires.

## Steps
1. Gather facts, do not guess:
   - `git log --oneline main..HEAD` for the commits of the day, with hashes.
   - `make check` (read the summary: tests, coverage, lint, flash and RAM
     size, traceability count). Report failures verbatim.
   - The CI status only if it was run (`gh run list --branch main --limit 1`).
   - `PLAN.md`: the items of the day and their tick state.
2. Write the review with exactly these five headings, in this order:
   1. **Done**: what the day delivered, with commit hashes and the REQ IDs.
   2. **Checks**: the `make check` result, and the CI status if run.
   3. **Gaps against `PLAN.md`**: unchecked items, missing tests, missing docs.
   4. **Proposals**: each one concrete, with an effort (S / M / L) and a risk
      (low / medium / high). Number them so the author can answer by number.
   5. **Decisions to confirm**: anything that changes a requirement, a
      deviation in `docs/04-coding-standard.md` §5, or the history of the
      branch (rebase, squash, force).
3. End with one question: which numbered proposals should be implemented, and
   whether the day can be validated for `day-close`.

## Rules
- Facts only: no "should pass" when the check has not been run.
- Open items that depend on hardware (wiring, flashing, bench measurements)
  go in **Gaps**, labelled as hardware, not as code defects.
- Keep it short enough to read in one sitting; detail goes in the docs, not
  in the review.
- The review is written for the author, in the language the author uses.
