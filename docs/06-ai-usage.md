# 06 — Use of AI assistance

Status: in force from Day 3. Owner: the project author.

This document explains how an AI coding assistant was used to build GasGuard,
why, and how its output was controlled. The short version is in the README.

## 1. Scope

The firmware was written with Claude (Anthropic's models) through Claude Code,
a command-line coding agent. The assistant drafted:

- C code, headers and host fakes,
- unit and scenario tests,
- Doxygen blocks and traceability tags,
- documents in `docs/` and `README.md`,
- review notes.

The assistant did not decide:

- the requirements (`docs/01-requirements.md`),
- the safety design and the FMEA scores (`docs/03-safety-analysis.md`),
- the architecture decisions (`docs/02-architecture.md`, ADR table),
- the acceptance of a change, the push of a day, or any action on hardware.

## 2. Why

GasGuard is a learning project: the author is learning C for embedded safety
work. The assistant reduces the repetitive work (test boilerplate, tags,
documentation upkeep) so that more time goes to the safety reasoning, which is
the part the author must be able to defend.

An assistant can also produce code that looks right and is not. The project is
built so that its output is never trusted on its own: every change must pass
the checks described in section 4.

## 3. Human decisions

| Decision | Who decides | How |
|---|---|---|
| Requirement wording | Author | Changes are proposed, approved, then written in the same commit. Example: REQ-ALM-005 (threshold validation), option A chosen by the author among several. |
| Safety score (FMEA) | Author | The assistant proposes S, O, D values with reasoning; the author accepts or changes them. |
| Deviation from the standard | Author | Recorded in `docs/04-coding-standard.md` §5 with a reason. A deviation is never added silently. |
| Push of a day | Author | Only after the author validates the day. |
| Flashing the board | Author | `tools/flash.ps1`, run from Windows. |
| Bench measurements | Author | Procedure in `docs/05-verification-plan.md`. |

## 4. Controls on the assistant's output

### 4.1 Rules and workflow

[`CLAUDE.md`](../CLAUDE.md) is read at the start of each session. It sets the
coding rules (R01 to R20), the test-first workflow, the checks before each
commit and each push, and the end-of-day review.

### 4.2 Automatic guard

A PostToolUse hook, [`tools/hooks/guard.py`](../tools/hooks/guard.py), runs
after every edit of a C file. It rejects `malloc`, `goto`, `printf`-family
calls, and floating point in the application layers. It is a fast regex check;
the full analysis is `make lint`.

### 4.3 Permissions

[`.claude/settings.json`](../.claude/settings.json) limits what the assistant
may run. Among the denied actions: flashing with `avrdude`, `rm -rf`, and
force-pushes (`--force`, `-f`). Pushes are allowed only for day branches and
`main`, after validation.

### 4.4 Tests and gates

Tests are written before the code: a red commit, then a green one. Each change
must pass `make check`:

- unit and scenario tests on the host,
- coverage (gcovr),
- cppcheck with the MISRA addon,
- clang-tidy,
- lizard (complexity ≤ 10, length ≤ 60),
- traceability (`make trace`).

### 4.5 Skills and independent review

Before each commit that touches `hal/`, `app/`, `services/` or a safety
mechanism, the skills `misra-review` and `safety-impact` run on the staged
files. Before each push, `trace-check` runs. A separate read-only agent,
[`.claude/agents/code-reviewer.md`](../.claude/agents/code-reviewer.md), checks
the staged diff with a fresh context, which it does not share with the author.

### 4.6 Transparency

Commits written with the assistant end with a `Co-Authored-By` line, so the
history shows which changes it drafted.

### 4.7 Learning check

The author answers quiz questions on each module (`explain-module` skill). The
results and the concepts to review are kept in
[`learning-log.md`](learning-log.md). The aim is that the author can explain
and defend every line in an interview.

## 5. Limits

- The assistant can be confidently wrong. The controls above catch many errors,
  not all of them.
- Hardware behaviour is not verified by the assistant: relay state at reset,
  sensor wiring, timing on the bench. Those statements are hypotheses until the
  measurements in `docs/05-verification-plan.md` are done by the author.
- The quality of the review depends on the documents: an error in a requirement
  or in a FMEA score is not caught by the tests that follow from it.
- The assistant's suggestions are reviewed like any other contribution. A
  suggestion is never accepted because it was produced by the assistant.
