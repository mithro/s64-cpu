# SISC — Simple Instruction Set Computer

A computer architecture philosophy.

## What SISC is

RISC reduces complexity down from a pre-existing complex (CISC) instruction set.
SISC never starts from a complex set to begin with — simplicity is axiomatic,
not the result of cutting things down. SISC is a set of *rules*, not an opcode
table. Any ISA that follows these rules is a SISC machine, the same way RV64I,
ARMv8, and a hypothetical hand-built ISA can all be RISC machines while having
completely different instructions.

This document defines the philosophy (the rules) and then, separately,
**ASM64** — one specific 64-bit reference implementation of SISC. ASM64 is an
example, not "the" definition of SISC. Other implementations could make
different choices (different register count, different opcode count, 32-bit
instead of 64-bit) and still be valid SISC machines, as long as they obey the
rules below.

---

## The Rules

### Rule 0 — Hand-traceable

A human, given nothing but a register file and memory written on paper, must
be able to execute any program by hand, one instruction at a time, using only
arithmetic a person can reasonably do without a calculator, and never needing
to remember anything that isn't visibly written down. No implicit flags, no
hidden carry, no mode bits sitting off-page.

This is the governing rule. If any rule below ever conflicts with a human's
ability to trace a program on paper, Rule 0 wins.

*Known tension:* IEEE-754 floating point (see below) does not fully satisfy
Rule 0 in the strictest sense — bit-exact rounding by hand is impractical.
SISC accepts this as a deliberate, named exception rather than pretending it
doesn't exist. A human can still trace the *algorithm* (align exponents, add
mantissas, normalize) step by step, even if reproducing hardware-exact
rounding by hand is impractical. Every other instruction category fully
satisfies Rule 0.

### Rule 1 — One instruction, one architectural effect

Every instruction modifies exactly one piece of visible state: a single
register, a single memory location, or the program counter. Never a
combination. This rules out auto-increment addressing, compare-and-branch
fusion in a single opcode, load-with-side-effect, and any flags register
written as a byproduct of an unrelated operation. If two effects are needed,
that's two instructions — SISC trades instruction count for effect-purity.

### Rule 2 — No implicit operands

Anything an instruction touches must be named explicitly in its encoding. No
special-cased stack pointer, no hidden accumulator, no flags register written
silently behind the scenes. If a register plays a conventional role (e.g.
acting as a stack pointer), that is a *software convention* enforced by the
calling convention or ABI — never a hardware behavior baked into specific
opcodes.

A fixed, universal, always-true behavior documented once in the spec (e.g.
"all load instructions sign-extend to full register width, with no
exceptions") is *not* a violation of this rule. Implicit means "behavior that
varies based on hidden context," not "behavior that isn't re-stated in every
single instruction's encoding." A rule that is constant everywhere is an
architectural property, not hidden state.

### Rule 3 — One instruction format

Every instruction shares an identical field layout and width, regardless of
what category of operation it performs — same opcode position, same operand
positions, every time. This is stricter than RISC, which typically uses
several instruction formats (R-type, I-type, J-type, etc). SISC pays for this
in wasted bits on simpler instructions, in exchange for a decoder that never
branches on instruction type to find out where its fields live.

### Rule 4 — Fixed width, always

No variable-length encoding under any circumstance. Every instruction is the
same number of bits. This is a precondition for Rule 3 to make sense.

### Rule 5 — Effect-determinism

Given the same operand values, an instruction always produces the same single
effect — no instruction's behavior depends on hidden machine mode, privilege
state, or history, unless that mode is itself an explicit operand named in
the encoding.

---
