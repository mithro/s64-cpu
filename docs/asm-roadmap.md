# Learning S64 Assembly — A Staged Roadmap

A path through `S64.md` §9 for actually *learning* to write S64 assembly, not just reading the
reference. Each stage introduces a small set of mnemonics, gives you something to write, and tells
you how to check it on `s64emu`. Do them in order — each stage assumes everything before it.

---

## Stage 0 — Mental Model (no code yet)

Before writing anything, get these three things solid, because they're what make S64 different
from x86/ARM and where beginners trip up:

1. **No flags register.** There is no "carry flag" or "zero flag" sitting around after an
   instruction. `EQ`/`NE`/`LT`/`GT` write a `1` or `0` straight into a register you choose. If you
   don't capture it, it's gone.
2. **One format, every instruction.** `opcode | Rd | Rs1 | Rs2/imm8 | M`. There's no instruction
   that "looks different" — once you can read one, you can read all of them.
3. **Load/store only.** ALU instructions (`ADD`, `SUB`, ...) never touch memory. If a value is in
   RAM, you `LD` it into a register first, operate, then `ST` it back if needed.

If those three sentences make sense, you're ready to write code.

---

## Stage 1 — Registers and Immediates

**New mnemonics:** `MOVI`, `HLT`

**Goal:** get a value into a register and stop cleanly.

```asm
        MOVI  R0, #42
        HLT
```

Run it, dump registers, confirm `R0 = 42`. This is the "hello world" of S64 — if this doesn't work,
nothing downstream will.

**Try yourself:** load three different registers with three different values, in one program.

---

## Stage 2 — Arithmetic, Both Modes

**New mnemonics:** `ADD`, `SUB`, `MUL`, `DIV`, `AND`, `OR`, `XOR`, `SHL`, `SHR`

**Goal:** understand the register-vs-immediate split that the `M` bit controls.

```asm
        MOVI  R0, #10
        MOVI  R1, #3
        ADD   R2, R0, R1   ; register mode: R2 = 10 + 3 = 13
        SUB   R3, R0, #4   ; immediate mode: R3 = 10 - 4 = 6
        HLT
```

This is the §9.5 worked example's whole trick, generalized: the **third operand decides the mode**
— a register operand means `M=0`, a `#imm` operand means `M=1`. Same opcode either way.

**Try yourself:**
- Write a program using every integer ALU op once.
- Predict the register dump on paper first, then run it and check.
- Try `SHL R1, R0, #3` (multiply by 8 via shift) and confirm it matches `MUL R1, R0, #8`.

---

## Stage 3 — Comparisons (and the habit they require)

**New mnemonics:** `EQ`, `NE`, `LT`, `GT`

**Goal:** internalize that comparisons are just another ALU op that happens to output 0/1 — there
is no implicit "did the comparison happen" state anywhere else.

```asm
        MOVI  R0, #5
        MOVI  R1, #9
        LT    R2, R0, R1   ; R2 = 1, because 5 < 9
        GT    R3, R0, R1   ; R3 = 0, because 5 is not > 9
        HLT
```

**Try yourself:** write all four comparisons (`EQ`, `NE`, `LT`, `GT`) on the same pair of registers
and check that the four results make sense together (e.g. `EQ` and `NE` should always be opposite).

This stage doesn't *do* anything with the result yet — that's deliberate. Get comfortable with
comparisons as pure data first, before mixing in control flow.

---

## Stage 4 — Control Flow: the Two-Instruction `if`

**New mnemonics:** `JMP`, `JEQ`, `JNE`

**Goal:** chain a Stage 3 comparison into an actual branch. This is the single most important
pattern in S64 — get it cold before moving on.

```asm
        MOVI  R0, #5
        MOVI  R1, #9
        LT    R2, R0, R1     ; R2 = (R0 < R1) ? 1 : 0
        JEQ   R2, less_than  ; jump there if R2 == 1
        ; ... falls through here if R0 was NOT less than R1
        JMP   end
less_than:
        ; ... this runs if R0 was less than R1
end:
        HLT
```

Notice the shape: **comparison, then jump-on-the-result.** Every `if` in S64 is this same two-line
pattern. There's no single "branch if less than" instruction — you build it from `LT` + `JEQ`.

**Try yourself:**
- Write the same logic using `GT` instead of `LT`, flipping which branch runs.
- Write a tiny "max of two numbers" program: load two values, compare, jump to whichever branch
  copies the bigger one into a result register.

---

## Stage 5 — Loops

**New concept:** none — this is Stage 4's pattern, aimed backward instead of forward.

**Goal:** see that a loop is just a branch whose label points *up* the program instead of down.

```asm
        MOVI  R0, #0       ; counter
        MOVI  R1, #5       ; limit
loop:
        ADD   R0, R0, #1   ; R0++
        EQ    R2, R0, R1   ; R2 = (R0 == R1) ? 1 : 0
        JNE   R2, loop     ; if R2 != 1 (i.e. not done), jump back to loop... 
```

Wait — check this carefully before running it: `JNE` jumps when its register operand equals `1`,
and that register here (`R2`) holds the result of `EQ`, not `NE`. So `JNE R2, loop` would jump back
*only when `R0 == R1`* — the opposite of what you want. The correct version uses `NE` to compute
"not yet done", then `JEQ` to act on it:

```asm
        MOVI  R0, #0       ; counter
        MOVI  R1, #5       ; limit
loop:
        ADD   R0, R0, #1   ; R0++
        NE    R2, R0, R1   ; R2 = (R0 != R1) ? 1 : 0  -> "still going"
        JEQ   R2, loop     ; if R2 == 1 (still going), jump back to loop
        HLT
```

This mistake is deliberately left in as the lesson: **`JEQ`/`JNE` always test "is the register
exactly 1," never the comparison name in the line above it.** Match the comparison you write to
the jump you use, every time — don't assume `JNE` "means" not-equal at the branch site.

**Try yourself:** trace this loop by hand, instruction by instruction, writing down `R0` and `R2`
after each pass, until you can predict exactly when it halts. Then run it and confirm.

---

## Stage 6 — Memory: Load/Store

**New mnemonics:** `LD8`/`LD16`/`LD32`/`LD64`, `ST8`/`ST16`/`ST32`/`ST64`

**Goal:** move a value out to RAM and back, and get comfortable with `[Rs1 + imm8]` addressing.

```asm
        MOVI  R0, #99
        MOVI  R1, #0          ; R1 will act as a base address (pick something safe in free RAM)
        ST64  R1, R0, #0      ; store R0 to [R1 + 0]
        LD64  R2, R1, #0      ; load it back into R2
        HLT
```

**Try yourself:**
- Store three different values at offsets `#0`, `#8`, `#16` from the same base register, then
  load all three back into different registers and confirm they match.
- Deliberately use `ST8` then `LD64` from the same address and look at what garbage shows up in
  the upper bytes — this is a good way to *feel* why width matters, not just read about it.

---

## Stage 7 — Functions: `CALL` / `RET` and the Link Register

**New mnemonics:** `CALL`, `RET`

**Goal:** write a reusable block of code and jump into/out of it without hardcoding addresses.

```asm
        MOVI  R0, #4
        CALL  R30, square    ; jump to square, save return address in R30
        ; control resumes here after RET
        HLT

square:
        MUL   R0, R0, R0     ; R0 = R0 * R0
        RET   R30            ; jump back to where CALL was made
```

This is where the SABI convention (`S64.md` §8) stops being theoretical: `R30` as the link
register only works because *you* chose to use it that way in both the `CALL` and the `RET` — the
hardware doesn't enforce it.

**Try yourself:** write two different functions and call them in sequence from `main`, using `R30`
both times. Confirm the second `CALL` correctly overwrites `R30` and still returns to the right
place.

---

## Stage 8 — The Stack: `PUSH` / `POP`

**New mnemonics:** `PUSH`, `POP`

**Goal:** see why Stage 7 breaks the moment you need *nested* calls, and how the stack fixes it.

Try this thought experiment before writing code: if function `A` calls function `B`, and `B` also
uses `CALL`/`RET` to call function `C`, what happens to the return address `A` was relying on in
`R30`? (It gets overwritten by `B`'s call to `C` — `A`'s return address is lost.)

The fix is to save `R30` on the stack before making a nested call, and restore it after:

```asm
b_func:
        PUSH  R30, SP        ; save A's return address
        CALL  R30, c_func    ; now safe to clobber R30
        POP   R30, SP        ; restore A's return address
        RET   R30            ; return to A correctly
```

**Try yourself:** build the three-function chain (`A` calls `B` calls `C`) for real, with each
function doing something small and visible (e.g. incrementing a register), and confirm execution
returns correctly all the way back to `A` and halts in the right place.

---

## Stage 9 — Put It Together

You now have every piece from `S64.md` §9.4 except floating point. Write something that combines
multiple stages — a real test of whether the patterns actually stuck, not just whether you can
copy a template:

**Suggested project:** sum the numbers 1 through N (pick N via `MOVI`), using:
- a loop (Stage 5)
- accumulation with `ADD` (Stage 2)
- a comparison + branch to know when to stop (Stage 4)
- store the final result to memory (Stage 6)
- wrap the whole summing logic in a function called via `CALL`/`RET` (Stage 7)

If you can write that without re-reading the stages above, the assembly language has actually
clicked — at that point you're ready to write real S64 programs, not just exercises.

---

## What's Deliberately Skipped Here

- **`FADD`/`FSUB`/`FMUL`/`FDIV`** — same syntax pattern as integer ALU ops, no new concepts; pick
  them up directly from `S64.md` §9.4 once you want them.
