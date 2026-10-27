---
name: explain-module
description: Teach a GasGuard module to the author (who is learning C) and quiz him with technical-interview questions. Use when the author asks to understand a file, a concept, or to prepare for an interview.
---

# Explain a module and quiz the author

The author is new to C and will be interviewed by an embedded team at a gas
detector manufacturer. Goal: he can explain and defend every line.

## Steps
1. Read the module (`.h`, `.c`, its tests) and its requirements / FMEA rows.
2. Explain in this order, in plain English, short paragraphs:
   1. **Why it exists**: which hazard or requirement it addresses.
   2. **How it works**: data flow, state, timing — draw an ASCII diagram if useful.
   3. **C concepts used**, each with a 3-line example from the actual code:
      e.g. `static`, `volatile`, pointers, `const` correctness, bit masks,
      fixed-point, integer promotion, `enum`, struct, ISR, atomic access.
   4. **Safety reasoning**: what would go wrong without it, how it fails safe.
   5. **What I would improve** in a real product.
3. Quiz: ask **5 questions**, one at a time, waiting for each answer:
   - 2 on C mechanics ("what happens if `v_tick` is not volatile?")
   - 2 on the design/safety ("why is the relay energised when healthy?")
   - 1 on hardware ("what does the ADC return if the sensor is unplugged, and why?")
4. Correct each answer kindly but precisely; give the model answer an
   interviewer expects in 2–3 sentences.
5. End with a score /5 and the 2 concepts to review, appended to `docs/learning-log.md`
   (date, module, score, concepts to review).

## Rules
- Do not modify source code in this skill.
- If the author asks in French, answer in French; keep code and terms in English.
