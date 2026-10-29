# CLAUDE.md — GasGuard firmware

You are working on **GasGuard**, a bare-metal C99 firmware for a combustible gas
detector on an Arduino Uno R3 (ATmega328P). It is a portfolio project whose goal
is to show **safety-oriented embedded engineering**. The author is learning C:
code must be clear, commented where non-obvious, and explainable.

## Source of truth (read before coding)

1. `docs/01-requirements.md` — what to build (REQ IDs)
2. `docs/02-architecture.md` — layers, pin mapping, state machine, scheduler
3. `docs/03-safety-analysis.md` — FMEA; why each safety mechanism exists
4. `docs/04-coding-standard.md` — rules R01–R20 (MISRA C:2012 subset)
5. `docs/05-verification-plan.md` — tests and scenarios
6. `PLAN.md` — the day-by-day plan; work only on the current day's items

If a request conflicts with these documents, **stop and ask** instead of guessing.
Never change a requirement silently: propose the change, wait for approval,
then update the doc in the same commit.

## Non-negotiable rules

- No `malloc`/`free`, no recursion, no `goto`, no floating point outside `hal/avr/`.
- `app/` and `services/` never include `<avr/...>` headers (host-testable).
- Fixed-width types only (`uint8_t`, `int16_t`…). Concentrations in `int16_t`, unit 0.1 %LEL.
- Every public function: Doxygen header with `@req` tags, argument checks, `gg_status_t` return.
- Every `switch` on an enum lists all values plus `default` → `diag_raise()`.
- Fail-safe: when in doubt, the detector goes to FAULT and de-energises the relay.
- No new requirement, deviation or dependency without saying so explicitly.

## Workflow for every change

1. Identify the REQ IDs concerned. If none exist, ask.
2. Write or update the **tests first** (`tests/`), tagged `/* @verifies REQ-… */`.
3. Implement with `/* @req REQ-… */` tags in the Doxygen header.
4. Run `make check` (build host + AVR, tests, coverage, cppcheck, clang-tidy, lizard, trace).
5. Fix every warning/finding — never disable a check to make it pass.
6. If the change affects a safety mechanism, run the `safety-impact` skill.
7. Summarise: what changed, which REQs, test results, memory usage (`make size`).

## Commands

| Command | Purpose |
|---|---|
| `make test` | Build and run unit + scenario tests on host |
| `make coverage` | gcovr report (`build/coverage/index.html`) |
| `make lint` | cppcheck (MISRA addon) + clang-tidy + lizard + header isolation grep |
| `make avr` | Cross-compile `build/avr/gasguard.hex` |
| `make size` | avr-size against the budget (REQ-NFR-005) |
| `make trace` | Regenerate `docs/traceability.md`, fail on gaps |
| `make check` | Everything above — must be green before any commit |

Flashing is done **by the human** from Windows (`tools/flash.ps1`). Never run avrdude.

## Commit style

Conventional commits, English: `feat(alarm): latch ALARM_2 until operator reset (REQ-ALM-004)`.
One logical change per commit. Never commit with `make check` failing, except
the red `test:` commit that TDD requires (see Branches below).
Commit often: at each important step (tests written and red, test green, refactor,
doc update, new module), not only at the end of a day. Small commits keep history
readable and make it easy to revert a single step.
don't mention anthropic or the adresse mail of the company in the commit.

## Branches and CI

- One branch per plan day, created from `main`: `day-1`, `day-2`… All work of
  that day happens on it. Never commit on `main` directly.
- Red commits are allowed on a day branch only for tests written before their
  implementation (`test(...)` commits). The branch must be green before merge.
- After the author validates the day: `git rebase main`, then merge with
  fast-forward only (`git merge --ff-only`), so `main` stays linear.
- Before the day ends, check that the merge is on `main` and that the CI passes.
  If not, fix it on a new branch (`day-N-fix-<topic>`), never by rewriting `main`.
- CI (`.github/workflows/ci.yml`) runs on push to `main`, and on demand on other
  branches: `workflow_dispatch` or a tag matching `ci-*`. Use the on-demand run
  sparingly.

## Teaching mode

The author must be able to defend every line in a technical interview.
After implementing a module, add a short **"How it works"** section at the top
of its `.c` file (5–10 lines, plain English) and suggest running the
`explain-module` skill.

## Skills available

- `misra-review` — review a file/diff against the coding standard
- `write-tests` — write Unity tests for a module from its requirements
- `safety-impact` — analyse a change against the FMEA, update it
- `trace-check` — verify requirement ↔ code ↔ test traceability
- `new-module` — scaffold a module (header, source, fake, test) the project way
- `explain-module` — teach a module to the author and quiz him (interview prep)
