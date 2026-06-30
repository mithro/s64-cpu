# S64 CPU

A complete computer architecture designed from scratch.

**S64** is a 64-bit processor based on **SISC** (Simple Instruction Set Computer) — a design philosophy where simplicity is axiomatic, not the result of cutting things down from something complex.

---

## What's in here

```
s64/
├── include/        s64.h — master header, all opcodes, types, memory map
├── lib/            libS64.a — emulator core (cpu, memory, loader, disasm)
├── emu/            s64emu — debugger + emulator CLI
├── as64/           as64 — assembler (.s64 → .o64)
├── ld64/           ld64 — linker (.o64 → .x64)
├── rtl/            Verilog RTL — 5-stage pipeline + SoC
├── tests/          test programs in S64 assembly
└── docs/           ISA reference, SABI calling convention, SXF format
```

---

## The Architecture

### SISC Philosophy
- One instruction format, always — fixed 32-bit width, no exceptions
- No flags register — comparisons write result (0 or 1) to a general register
- No implicit operands — everything named explicitly in the encoding
- Load/store only — ALU ops never touch memory
- Effect-determinism — same inputs always produce same output, no hidden state

### S64 ISA
- 32 general purpose registers (R0–R31), R31 hardwired zero
- 64-bit registers, 32-bit compatibility mode (W0–W31)
- 32-bit fixed instruction width
- 8-bit opcode (256 slots), 5-bit register fields × 3, 8-bit immediate, 1 mode bit

```
31      24 23    19 18    14 13     9 8      1  0
[ opcode  ] [  Rd  ] [ Rs1  ] [ Rs2  ] [  imm8 ] [M]
```

M=0 → use Rs2 as register  
M=1 → use imm8 as immediate

### Instruction Groups
| Range | Group |
|-------|-------|
| 0x00–0x0F | Integer ALU (ADD, SUB, MUL, DIV, MOD) |
| 0x10–0x1F | Bitwise & Shift (AND, OR, XOR, NOT, SHL, SHR, SAR) |
| 0x20–0x2F | Compare (EQ, NE, LT, GT, LTE, GTE, LTU, GTU) |
| 0x30–0x3F | Control Flow (JMP, JMPI, JEQ, JNE, CALL, RET, LOOP) |
| 0x40–0x4F | Memory (LD64/32/16/8, ST64/32/16/8, MOV, MOVI, MOVW) |
| 0x50–0x5F | Floating Point (FADD, FSUB, FMUL, FDIV, FCMP, FTOI, ITOF) |
| 0x60–0x6F | System (NOP, HLT, SYS, PUSH, POP) |
| 0x70–0xFF | Reserved for extensions |

### SABI Calling Convention
| Register | Role |
|----------|------|
| R0–R7 | Arguments / return values |
| R8–R15 | Caller-saved temporaries |
| R16–R26 | Callee-saved |
| R27 | Trap link register |
| R28 | Stack pointer (by convention) |
| R29 | Frame pointer (by convention) |
| R30 | Link register (by convention) |
| R31 | Zero register (hardwired 0) |

### Memory Map
| Address | Region |
|---------|--------|
| 0x0000000000000000 | Reset vector |
| 0x0000000000000008 | NMI vector |
| 0x0000000000000010 | Fault vector |
| 0x0000000000000018 | IRQ vector |
| 0x0000000000001000 | Bootloader |
| 0x0000000000008000 | RAM base (default load address) |
| 0xFFFFFFFFFFFE0000 | Firmware ROM |

---

## The Hardware (RTL)

5-stage in-order pipeline targeting **sky130A** at **50MHz**:

```
IF → ID → EX → MEM → WB
```

- Data forwarding (EX→EX, MEM→EX)
- Load-use stall detection
- Branch resolved in EX stage, 2-cycle flush on taken branch
- Separate IBUS and DBUS via arbiter over shared SRAM

### SoC Components
- S64 CPU core
- 8KB SRAM
- UART (115200 8N1)
- GPIO (8-bit)
- Memory mapped IO decoder

### Simulate
```bash
cd rtl
iverilog -g2005 s64_tb.v s64_cpu_top.v s64_if.v s64_id.v s64_ex.v \
    s64_mem.v s64_wb.v s64_regfile.v s64_alu.v \
    s64_sram.v s64_arbiter.v s64_uart.v s64_soc.v -o s64_sim
./s64_sim
```

Expected output:
```
S64 pipeline simulation started
Program: MOVI R0,#10 | MOVI R1,#32 | ADD R2,R0,R1 | HLT
Expected: R2 = 42 (0x2A)
...
PASS: R2 = 42 correct!
```

---

## The Toolchain

### Build
```bash
# assembler
gcc -std=gnu99 -O2 -I include \
    as64/main.c as64/lexer.c as64/parser.c as64/encoder.c as64/symbols.c \
    lib/disasm.c -o as64

# linker
gcc -std=gnu99 -O2 -I include \
    ld64/main.c ld64/obj.c ld64/resolve.c ld64/reloc.c ld64/emit.c \
    -o ld64

# emulator
gcc -std=gnu99 -O2 -I include \
    lib/memory.c lib/cpu.c lib/disasm.c lib/loader.c \
    emu/main.c -lm -o s64emu
```

### Write a program
```asm
; hello.s64
.section .text
.global _start
_start:
    MOVI  R0, #10      ; R0 = 10
    MOVI  R1, #32      ; R1 = 32
    ADD   R2, R0, R1   ; R2 = 42
    HLT
```

### Assemble, link, run
```bash
./as64 hello.s64 -o hello.o64
./ld64 hello.o64 -o hello.x64
./s64emu --dump-regs hello.x64
```

### Debug
```bash
./s64emu --step hello.x64
# commands: s=step, c=continue, r=regs, m=memdump, b=breakpoint, d=disasm, q=quit
```

---

## SXF Binary Format

Custom executable format (S64 eXecutable Format):

```
[0-2]   magic "SXF"
[3]     version
[4]     mode      0x01=64bit  0x02=32bit
[5]     endian    0x00=LE     0x01=BE (stub)
[6-13]  entry address
[14-15] section count
[16-23] symbol table offset
[24-31] reserved
```

File extensions: `.s64` source · `.o64` object · `.x64` executable · `.a64` static library

---


## License

MIT License — free to use, modify, and build on.

---

*Designed independently from scratch — ISA, pipeline, assembler, linker, emulator, SoC.*
