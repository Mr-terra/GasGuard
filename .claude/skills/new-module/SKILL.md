---
name: new-module
description: Scaffold a new GasGuard module the project way (public header, source with module state struct, host fake if HAL, Unity test file, Makefile entry). Use when the user asks to create a module, driver or service.
---

# Scaffold a module

## Inputs
Module name (snake_case), layer (`app` | `services` | `drivers` | `hal`), REQ IDs.

## Steps
1. Check `docs/02-architecture.md`: the module must belong to a layer and only
   depend on layers below. If it does not fit, stop and propose an architecture
   update first.
2. Create:
   - `<layer>/<name>.h` — include guard `GG_<NAME>_H`, Doxygen file header,
     public types, `gg_status_t` functions, no globals.
   - `<layer>/<name>.c` — "How it works" comment block (filled after
     implementation), one `static` state struct `s_<name>`, `static` helpers,
     `<name>_init()` that resets state.
   - For `hal/`: interface in `hal/<name>.h`, AVR implementation in
     `hal/avr/<name>_avr.c`, fake in `hal/host/<name>_fake.c` with
     control functions for tests (`<name>_fake_set_…()`).
   - `tests/test_<name>.c` — Unity skeleton with `setUp`/`tearDown` and one
     failing placeholder test per REQ (`TEST_IGNORE_MESSAGE("todo REQ-…")`).
3. Register the files in the Makefile source lists.
4. Run `make test` (placeholders ignored, build green) and `make lint`.

## Output
List of created files and the next step: run `write-tests`, then implement.
