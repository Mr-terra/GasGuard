# GasGuard — Coding Standard

Based on a **subset of MISRA C:2012** plus project rules. MISRA C itself is a
paid standard; rule numbers are used as references only. Any deviation must be
recorded in §5 with a justification.

## 1. Language and build

- C99 (`-std=c99`), no compiler extensions except `ISR()` / `PROGMEM` in `hal/avr/`.
- Flags (both targets):
  `-Wall -Wextra -Werror -Wpedantic -Wconversion -Wsign-conversion -Wshadow
   -Wcast-qual -Wstrict-prototypes -Wmissing-prototypes -Wundef -Wswitch-enum`
- AVR target adds: `-Os -ffunction-sections -fdata-sections -Wl,--gc-sections`.
- Host target adds: `-O0 -g --coverage -fsanitize=address,undefined`.

## 2. Mandatory rules

| Ref | Rule |
|---|---|
| R01 (MISRA 4.6) | Use `<stdint.h>` fixed-width types; never bare `int`, `long`, `char` for data. `bool` from `<stdbool.h>`. |
| R02 (21.3) | No `malloc`, `calloc`, `free`, `realloc`. All memory is static. |
| R03 (17.2) | No recursion. |
| R04 (15.1) | No `goto`. |
| R05 (15.5) | One return point per function, except early-return on argument check at the top. |
| R06 (16.4) | Every `switch` has a `default`; on an enum, every value is listed and `default` handles the impossible case (→ fault). |
| R07 (17.7) | Never ignore a return value; cast to `(void)` explicitly if intentional, with a comment. |
| R08 (10.x) | No implicit narrowing or signed/unsigned mixing; explicit casts after range check. |
| R09 (8.7, 8.9) | Every object/function not used outside its file is `static`. Module state lives in one `static` struct. |
| R10 (12.1) | Parenthesise mixed operators; no reliance on precedence. |
| R11 (13.5) | No side effects in the right operand of `&&` / `\|\|`. |
| R12 (14.4) | Conditions are explicitly boolean (`if (ptr != NULL)`, not `if (ptr)`). |
| R13 (21.6) | No `printf`/`scanf` family on target (code size, determinism). |
| R14 | No floating point in `app/`, `services/`, `drivers/` — fixed-point only. |
| R15 | Variables shared with an ISR are `volatile`; multi-byte reads are done with interrupts disabled (`ATOMIC_BLOCK`). |
| R16 | Every function has a Doxygen header with `@brief`, params, `@return`, and `@req` tags for the requirements it implements. |
| R17 | Every public function checks its pointer arguments (`NULL`) and ranges, and returns an error code (`gg_status_t`). |
| R18 | Magic numbers are forbidden; use named `#define`/`enum` constants in a `*_cfg.h`. |
| R19 | Functions ≤ 60 lines, cyclomatic complexity ≤ 10 (checked by `lizard`). |
| R20 | `app/` and `services/` must not include any `<avr/...>` header (checked by CI grep). |

## 3. Naming

- Files: `module_name.c/.h`; public functions `module_verb_object()` (e.g. `filter_push_sample`).
- Types: `snake_case_t`; enums `UPPER_CASE` values prefixed by module (`DET_STATE_ALARM_1`).
- Macros/constants: `UPPER_CASE`. Static module data: `s_` prefix. ISR-shared: `v_` prefix.

## 4. Error handling

```c
typedef enum {
    GG_OK = 0,
    GG_ERR_NULL,
    GG_ERR_RANGE,
    GG_ERR_CRC,
    GG_ERR_TIMEOUT,
    GG_ERR_HW
} gg_status_t;
```

An unexpected internal state is never ignored: it calls
`diag_raise(DIAG_INTERNAL, code)`, which logs and moves the detector to FAULT.

## 5. Deviations register

| ID | Rule | Location | Justification |
|---|---|---|---|
| DEV-01 | R01 | `hal/avr/*` | avr-libc register macros use implementation types. |
| DEV-02 | MISRA 2.5 (unused macro) | include guards in every `*.h` (`#define X_H`) | Include guards are used only by the preprocessor, not as values; cppcheck reports them as unused macros. Each guard carries an inline `cppcheck-suppress misra-c2012-2.5` line. |
| DEV-03 | MISRA 8.6, cppcheck unusedFunction | Unity hooks `setUp`, `tearDown`, `main` in `tests/` | Unity calls these hooks and `main` by name from its runner, so they have external linkage and no caller in our code. Each one carries an inline suppression. |
| DEV-04 | MISRA 8.7 | Every prototype in `hal/hal_*.h` and `hal/host/hal_*_fake.h` | Each HAL interface is defined once on the PC (its host fake) but has a second implementation on the board (`hal/avr/`), and each fake control API is called from the tests. cppcheck analyses only the PC build, so it sees a single definition. Inline suppression on each prototype. |
| DEV-05 | MISRA 8.7 | `services/filter.h` (and every later `services/` header) | Service functions are the public API of the module. Until `app/` calls them, cppcheck sees a single caller (the test) and asks for `static`. Making them `static` would hide the API from `app/`. Inline suppression on each prototype; to be removed when `app/` uses the function. |
| *(add new deviations here — never silently)* | | | |

## 6. Tooling

| Tool | Purpose | Gate |
|---|---|---|
| `gcc` / `avr-gcc` with flags §1 | Compiler warnings | 0 warnings |
| `cppcheck --enable=all --addon=misra` | Static analysis + MISRA addon | 0 findings outside deviations |
| `clang-tidy` (bugprone, cert, misc) | Static analysis | 0 findings |
| `lizard -C 10 -L 60` | Complexity | pass |
| `Unity` + `gcov`/`gcovr` | Unit tests + coverage | 100 % pass, ≥ 90 % lines |
| `avr-size` | Memory budget | REQ-NFR-005 |
| `tools/trace.py` | Requirement traceability | every REQ has code + test |
