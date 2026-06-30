# S64 — Instruction Set Architecture

A 64-bit (with 32-bit compatibility) instruction set, designed under the SISC philosophy.

---

## 1. Overview

- **Registers:** 32 general purpose registers
  - 64-bit mode: `X0`–`X31`
  - 32-bit mode: `W0`–`W31` (lower 32 bits of the same physical register)
  - Generic/architectural name: `R0`–`R31`
- **Instruction width:** fixed 32 bits, one format, always
- **Architecture style:** load/store (no memory operands in ALU instructions)
- **Flags:** none — comparisons write 0/1 into a general purpose register
- **Hardware-special registers:** only `R31` (hardwired zero register). All other register roles (SP, FP, LR, etc.) are SABI software convention, not hardware behavior.

---

## 2. Instruction Format

One format, used by every instruction (unused fields are ignored):

```
 31        24 23    19 18    14 13     9 8        1  0
[  opcode   ] [  Rd  ] [ Rs1  ] [ Rs2  ] [  imm8   ] [M]
[   8 bits  ] [5 bits] [5 bits] [5 bits] [  8 bits ] [1]
```

| Field  | Bits | Width | Meaning |
|--------|------|-------|---------|
| opcode | 31–24 | 8 | instruction operation |
| Rd     | 23–19 | 5 | destination register (R0–R31) |
| Rs1    | 18–14 | 5 | source register 1 |
| Rs2    | 13–9  | 5 | source register 2 (used when M=0) |
| imm8   | 8–1   | 8 | 8-bit immediate (used when M=1) |
| M      | 0     | 1 | mode bit |

**Mode bit (M):**
- `M=0` → register mode: `Rs2` is used as a register, `imm8` is ignored
- `M=1` → immediate mode: `imm8` is used, `Rs2` field is ignored

Total: 8 + 5 + 5 + 5 + 8 + 1 = **32 bits**, all power-of-2 field widths.

---

## 3. Register Set

| Register | 64-bit name | 32-bit name | Role |
|----------|-------------|-------------|------|
| R0–R31   | X0–X31      | W0–W31      | general purpose |
| R31      | X31         | W31         | **hardwired zero register** (only hardware-special register) |

All other roles below (R8–R30) are **SABI convention only** — the CPU has no opinion about them.

---

## 4. Opcode Table

256 opcodes available (8-bit field). Organized by group:

| Range | Group |
|-------|-------|
| 0x00–0x1F | ALU (integer arithmetic / logic) |
| 0x20–0x3F | Memory (load/store) |
| 0x40–0x5F | Control flow (jump/branch/call) |
| 0x60–0x7F | System / misc |
| 0x80–0xFF | Reserved for future extensions (e.g. S64-VEXT, S64-BEXT) |

### ALU (0x00–0x1F)

| Opcode | Mnemonic | Operation |
|--------|----------|-----------|
| 0x00 | ADD | Rd = Rs1 + Rs2 / imm8 |
| 0x01 | SUB | Rd = Rs1 - Rs2 / imm8 |
| 0x02 | MUL | Rd = Rs1 * Rs2 / imm8 |
| 0x03 | DIV | Rd = Rs1 / Rs2 / imm8 |
| 0x04 | AND | Rd = Rs1 & Rs2 / imm8 |
| 0x05 | OR  | Rd = Rs1 \| Rs2 / imm8 |
| 0x06 | XOR | Rd = Rs1 ^ Rs2 / imm8 |
| 0x07 | SHL | Rd = Rs1 << Rs2 / imm8 |
| 0x08 | SHR | Rd = Rs1 >> Rs2 / imm8 |
| 0x09 | FADD | Rd = Rs1 + Rs2 (float) |
| 0x0A | FSUB | Rd = Rs1 - Rs2 (float) |
| 0x0B | FMUL | Rd = Rs1 * Rs2 (float) |
| 0x0C | FDIV | Rd = Rs1 / Rs2 (float) |
| 0x0D | EQ  | Rd = (Rs1 == Rs2) ? 1 : 0 |
| 0x0E | NE  | Rd = (Rs1 != Rs2) ? 1 : 0 |
| 0x0F | LT  | Rd = (Rs1 <  Rs2) ? 1 : 0 |
| 0x10 | GT  | Rd = (Rs1 >  Rs2) ? 1 : 0 |

*(No flags register — comparison results land in a general register; branches test that register's value.)*

### Memory (0x20–0x3F)

| Opcode | Mnemonic | Operation |
|--------|----------|-----------|
| 0x20 | LD8  | load byte |
| 0x21 | LD16 | load halfword |
| 0x22 | LD32 | load word |
| 0x23 | LD64 | load doubleword |
| 0x24 | ST8  | store byte |
| 0x25 | ST16 | store halfword |
| 0x26 | ST32 | store word |
| 0x27 | ST64 | store doubleword |
| 0x28 | PUSH | software convention — ST64 + SP decrement (no hardware SP) |
| 0x29 | POP  | software convention — LD64 + SP increment (no hardware SP) |

### Control flow (0x40–0x5F)

| Opcode | Mnemonic | Operation |
|--------|----------|-----------|
| 0x40 | JMP  | unconditional jump |
| 0x41 | JEQ  | jump if Rs1 == 1 (i.e. result of EQ) |
| 0x42 | JNE  | jump if Rs1 == 1 (i.e. result of NE) |
| 0x43 | CALL | jump + write return address to specified link register |
| 0x44 | RET  | jump to address in specified link register |

### System / misc (0x60–0x7F)

| Opcode | Mnemonic | Operation |
|--------|----------|-----------|
| 0x49 | MOVI | Rd = imm8 (immediate mode load) |
| 0x61 | HLT  | halt execution |

*(Opcode numbering above reflects what was locked in conversation; remaining slots in each range are open for future instructions — see `s64.h` for the authoritative, currently-implemented set.)*

---

## 5. Assembler Directives (not ISA instructions)

Handled by `as64`, not the CPU:

- `.text` — code section
- `.data` — initialized data section
- `.rodata` — read-only data section
- `.bss` — uninitialized data section

---

## 6. Endianness

- **Default:** little-endian
- **Extension:** `S64-BEXT` (big-endian extension) — reserved as a stub across the whole toolchain
  - `as64 --be` / `ld64 --be` — accepted flags, currently emit "not yet implemented"
  - SXF header byte 5: `0x00` = little-endian, `0x01` = big-endian
  - Endianness is a **fixed property set at load time**, never a runtime toggle (consistent with SISC's no-hidden-state rule)

---

## 7. Memory Map

```
Address                 Region          Size      Description
─────────────────────────────────────────────────────────────
0x0000000000000000      RESET VECTOR    8 bytes   CPU starts here, jumps to FW
0x0000000000000008      NMI VECTOR      8 bytes   non-maskable interrupt
0x0000000000000010      FAULT VECTOR    8 bytes   CPU fault / exception
0x0000000000000018      IRQ VECTOR      8 bytes   hardware interrupt
0x0000000000000020      RESERVED        224 bytes pad to 0xFF

0x0000000000000100      SCRATCH         4KB       early boot scratch RAM

0x0000000000001000      BOOTLOADER      28KB      0x1000–0x7FFF
0x0000000000008000      FREE RAM        OS / userspace load base

0xFFFFFFFFFFFE0000      FIRMWARE (ROM)  128KB     top of address space
0xFFFFFFFFFFFFFFFF      top of space
```

**Boot sequence:**
1. CPU powers on, PC = `0x0000000000000000`
2. Reset vector holds address of firmware → CPU jumps to `0xFFFFFFFFFFFE0000`
3. Firmware initializes hardware, locates boot device
4. Firmware copies bootloader to `0x0000000000001000`, jumps there
5. Bootloader loads kernel/OS to `0x0000000000008000+`, jumps in

**SXF default load base:** `0x0000000000008000`

---

## 8. SABI — S64 Calling Convention

Pure software convention — the CPU has no opinion about any register's role except R31.

| Register | Role |
|----------|------|
| R0–R7    | arguments / return values |
| R8–R15   | caller-saved temporaries |
| R16–R26  | callee-saved |
| R27      | trap link register (saved PC on exception) |
| R28      | stack pointer (convention) |
| R29      | frame pointer (convention) |
| R30      | link register (convention) — `CALL R30, fn` then `RET R30` |
| R31      | **hardwired zero register** (hardware-enforced) |

---

## 9. Toolchain & File Formats

| Tool | Role |
|------|------|
| `as64` | assembler: `.s64` source → `.o64` object file |
| `ld64` | linker: `.o64` files → `.x64` executable, or flat binary |

**File extensions:** `.s64` (source), `.o64` (object), `.x64` (executable), `.a64` (archive)

**SXF — S64 eXecutable Format** (magic bytes `SXF`, modeled on ELF's `\x7FELF`):

```
[0-2]   magic "SXF"
[3]     version
[4]     mode      0x01 = 64-bit, 0x02 = 32-bit
[5]     endian    0x00 = LE, 0x01 = BE
[6-13]  entry address
[14-15] section count
[16-23] symbol table offset
[24-31] reserved
```
