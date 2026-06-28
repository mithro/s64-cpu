#ifndef S64_H
#define S64_H

/*
 * s64.h — S64 ISA master header
 * SISC (Simple Instruction Set Computer) architecture
 *
 * Instruction format (32-bit fixed width):
 *  31      24 23    19 18    14 13     9 8      1  0
 * [ opcode  ] [  Rd  ] [ Rs1  ] [ Rs2  ] [  imm8 ] [M]
 * [  8 bits ] [5 bits] [5 bits] [5 bits] [ 8bits ] [1]
 *
 * M=0: Rs2 is used as register, imm8 ignored
 * M=1: imm8 is used as immediate, Rs2 ignored
 *
 * Endianness: little-endian (S64-BEXT big-endian stub reserved)
 */

#include <stdint.h>
#include <stddef.h>

/* ─── basic types ─────────────────────────────────────────────── */
typedef uint8_t   u8;
typedef uint16_t  u16;
typedef uint32_t  u32;
typedef uint64_t  u64;
typedef int8_t    s8;
typedef int16_t   s16;
typedef int32_t   s32;
typedef int64_t   s64_t;

/* ─── ISA constants ───────────────────────────────────────────── */
#define S64_NUM_REGS        32
#define S64_INSTR_WIDTH     4       /* bytes per instruction */
#define S64_REG_WIDTH       8       /* bytes per register (64-bit) */
#define S64_REG_WIDTH_32    4       /* bytes per register (32-bit mode) */

/* ─── instruction field extraction ───────────────────────────── */
#define INSTR_OPCODE(i)   (((i) >> 24) & 0xFF)
#define INSTR_RD(i)       (((i) >> 19) & 0x1F)
#define INSTR_RS1(i)      (((i) >> 14) & 0x1F)
#define INSTR_RS2(i)      (((i) >>  9) & 0x1F)
#define INSTR_IMM8(i)     (((i) >>  1) & 0xFF)
#define INSTR_M(i)        (((i)      ) & 0x01)

/* sign-extend imm8 to 64 bits */
#define INSTR_SIMM8(i)    ((s64_t)(s8)INSTR_IMM8(i))

/* ─── instruction encoding ────────────────────────────────────── */
#define ENCODE_INSTR(op, rd, rs1, rs2, imm8, m) \
    ( ((u32)(op)   << 24) | \
      ((u32)(rd)   << 19) | \
      ((u32)(rs1)  << 14) | \
      ((u32)(rs2)  <<  9) | \
      ((u32)(imm8) <<  1) | \
      ((u32)(m)         ) )

/* ─── opcodes ─────────────────────────────────────────────────── */

/* integer ALU — 0x00–0x0F */
#define OP_ADD      0x00    /* Rd = Rs1 + Rs2 */
#define OP_SUB      0x01    /* Rd = Rs1 - Rs2 */
#define OP_MUL      0x02    /* Rd = Rs1 * Rs2 (low 64) */
#define OP_DIV      0x03    /* Rd = Rs1 / Rs2 (signed) */
#define OP_MOD      0x04    /* Rd = Rs1 % Rs2 */
#define OP_ADDI     0x05    /* Rd = Rs1 + imm8 (M=1) */
#define OP_SUBI     0x06    /* Rd = Rs1 - imm8 (M=1) */
#define OP_MULI     0x07    /* Rd = Rs1 * imm8 (M=1) */

/* bitwise & shift — 0x10–0x1F */
#define OP_AND      0x10    /* Rd = Rs1 & Rs2 */
#define OP_OR       0x11    /* Rd = Rs1 | Rs2 */
#define OP_XOR      0x12    /* Rd = Rs1 ^ Rs2 */
#define OP_NOT      0x13    /* Rd = ~Rs1 */
#define OP_SHL      0x14    /* Rd = Rs1 << Rs2 */
#define OP_SHR      0x15    /* Rd = Rs1 >> Rs2 (logical) */
#define OP_SAR      0x16    /* Rd = Rs1 >> Rs2 (arithmetic) */
#define OP_SHLI     0x17    /* Rd = Rs1 << imm8 (M=1) */
#define OP_SHRI     0x18    /* Rd = Rs1 >> imm8 logical (M=1) */

/* compare — 0x20–0x2F */
#define OP_EQ       0x20    /* Rd = (Rs1 == Rs2) ? 1 : 0 */
#define OP_NE       0x21    /* Rd = (Rs1 != Rs2) ? 1 : 0 */
#define OP_LT       0x22    /* Rd = (Rs1 <  Rs2) ? 1 : 0 signed */
#define OP_GT       0x23    /* Rd = (Rs1 >  Rs2) ? 1 : 0 signed */
#define OP_LTE      0x24    /* Rd = (Rs1 <= Rs2) ? 1 : 0 signed */
#define OP_GTE      0x25    /* Rd = (Rs1 >= Rs2) ? 1 : 0 signed */
#define OP_LTU      0x26    /* Rd = (Rs1 <  Rs2) ? 1 : 0 unsigned */
#define OP_GTU      0x27    /* Rd = (Rs1 >  Rs2) ? 1 : 0 unsigned */

/* control flow — 0x30–0x3F */
#define OP_JMP      0x30    /* PC = Rs1 */
#define OP_JMPI     0x31    /* PC = PC + imm8 (M=1) */
#define OP_JEQ      0x32    /* if Rs2==1: PC = Rs1 */
#define OP_JNE      0x33    /* if Rs2==0: PC = Rs1 */
#define OP_CALL     0x34    /* Rd = PC+4; PC = Rs1 */
#define OP_RET      0x35    /* PC = Rs1 */
#define OP_LOOP     0x36    /* Rd--; if Rd!=0: PC = Rs1 */

/* memory — 0x40–0x4F */
#define OP_LD64     0x40    /* Rd = mem64[Rs1 + imm8] */
#define OP_LD32     0x41    /* Rd = mem32[Rs1 + imm8] zero-ext */
#define OP_LD16     0x42    /* Rd = mem16[Rs1 + imm8] zero-ext */
#define OP_LD8      0x43    /* Rd = mem8 [Rs1 + imm8] zero-ext */
#define OP_ST64     0x44    /* mem64[Rs2 + imm8] = Rs1 */
#define OP_ST32     0x45    /* mem32[Rs2 + imm8] = Rs1[31:0] */
#define OP_ST16     0x46    /* mem16[Rs2 + imm8] = Rs1[15:0] */
#define OP_ST8      0x47    /* mem8 [Rs2 + imm8] = Rs1[7:0] */
#define OP_MOV      0x48    /* Rd = Rs1 */
#define OP_MOVI     0x49    /* Rd = imm8 zero-ext (M=1) */
#define OP_MOVW     0x4A    /* Rd = (Rd << 8) | imm8 — build wide imm */

/* floating point — 0x50–0x5F */
#define OP_FADD     0x50    /* Rd = Rs1 + Rs2 (IEEE-754 f64) */
#define OP_FSUB     0x51    /* Rd = Rs1 - Rs2 (f64) */
#define OP_FMUL     0x52    /* Rd = Rs1 * Rs2 (f64) */
#define OP_FDIV     0x53    /* Rd = Rs1 / Rs2 (f64) */
#define OP_FCMP     0x54    /* Rd = fcmp result (0/1) */
#define OP_FTOI     0x55    /* Rd = (int64) Rs1 truncate */
#define OP_ITOF     0x56    /* Rd = (f64)   Rs1 */

/* system / misc — 0x60–0x6F */
#define OP_NOP      0x60    /* no operation */
#define OP_HLT      0x61    /* halt execution */
#define OP_SYS      0x62    /* syscall imm8 = call number */
#define OP_PUSH     0x63    /* mem[Rs2]=Rs1; Rs2-=8 (convention) */
#define OP_POP      0x64    /* Rs1+=8; Rd=mem[Rs1] (convention) */

/* 0x70–0xFF reserved for future extensions */

/* ─── SABI register aliases ───────────────────────────────────── */
#define REG_R0      0       /* return value / arg 0 */
#define REG_R1      1       /* return value high / arg 1 */
#define REG_R2      2       /* arg 2 */
#define REG_R3      3       /* arg 3 */
#define REG_R4      4       /* arg 4 */
#define REG_R5      5       /* arg 5 */
#define REG_R6      6       /* arg 6 */
#define REG_R7      7       /* arg 7 */
/* R8–R15  caller-saved temporaries (t0–t7) */
/* R16–R26 callee-saved (s0–s10) */
#define REG_R27     27      /* trap link register */
#define REG_R28     28      /* stack pointer (sp) */
#define REG_R29     29      /* frame pointer (fp) */
#define REG_R30     30      /* link register (lr) */
#define REG_R31     31      /* zero register (zr) — reads 0, writes discarded */

/* ─── memory map ──────────────────────────────────────────────── */
#define MEM_RESET_VECTOR    0x0000000000000000ULL
#define MEM_NMI_VECTOR      0x0000000000000008ULL
#define MEM_FAULT_VECTOR    0x0000000000000010ULL
#define MEM_IRQ_VECTOR      0x0000000000000018ULL
#define MEM_SCRATCH_BASE    0x0000000000000100ULL  /* 4KB early boot scratch */
#define MEM_BOOT_BASE       0x0000000000001000ULL  /* bootloader 0x1000–0x7FFF */
#define MEM_RAM_BASE        0x0000000000008000ULL  /* free RAM — OS loads here */
#define MEM_FW_BASE         0xFFFFFFFFFFFE0000ULL  /* firmware ROM 128KB */

/* default SXF load base */
#define SXF_DEFAULT_BASE    MEM_RAM_BASE

/* ─── SXF binary format ───────────────────────────────────────── */
#define SXF_MAGIC           "SXF"
#define SXF_MAGIC_SIZE      3
#define SXF_VERSION         1
#define SXF_MODE_64         0x01
#define SXF_MODE_32         0x02
#define SXF_ENDIAN_LE       0x00
#define SXF_ENDIAN_BE       0x01    /* S64-BEXT stub — not yet implemented */

#define SXF_SECT_FLAG_R     0x01
#define SXF_SECT_FLAG_W     0x02
#define SXF_SECT_FLAG_X     0x04

/* SXF file header — 32 bytes */
typedef struct __attribute__((packed)) {
    u8  magic[3];           /* "SXF" */
    u8  version;            /* SXF_VERSION */
    u8  mode;               /* SXF_MODE_64 or SXF_MODE_32 */
    u8  endian;             /* SXF_ENDIAN_LE or SXF_ENDIAN_BE */
    u8  _pad0[2];
    u64 entry;              /* entry point virtual address */
    u16 section_count;
    u8  _pad1[2];
    u64 symtab_offset;      /* offset to symbol table in file */
    u8  _reserved[8];
} SXFHeader;

/* SXF section descriptor — 32 bytes each, immediately after header */
typedef struct __attribute__((packed)) {
    u8  name[8];            /* section name null-padded e.g. ".text\0\0\0" */
    u64 file_offset;        /* offset into file */
    u64 file_size;          /* size in file (0 for .bss) */
    u64 virt_addr;          /* virtual address to load at */
    u64 mem_size;           /* size in memory (>= file_size, .bss zero-fills) */
    u8  flags;              /* SXF_SECT_FLAG_R/W/X */
    u8  _pad[7];
} SXFSection;

/* SXF relocation entry */
#define SXF_RELOC_ABS       0x01    /* absolute address */
#define SXF_RELOC_PCREL     0x02    /* PC-relative */

typedef struct __attribute__((packed)) {
    u64 offset;             /* offset in section to patch */
    u64 symbol_index;       /* index into symbol table */
    u8  type;               /* SXF_RELOC_ABS or SXF_RELOC_PCREL */
    u8  _pad[7];
} SXFReloc;

/* ─── execution mode ──────────────────────────────────────────── */
typedef enum {
    S64_MODE_64BIT = 0,
    S64_MODE_32BIT = 1,
} S64ExecMode;

/* ─── cpu state ───────────────────────────────────────────────── */
typedef struct {
    u64          regs[S64_NUM_REGS];   /* R0–R31 (R31 always reads 0) */
    u64          pc;                    /* program counter */
    S64ExecMode  mode;                  /* 64 or 32 bit */
    int          halted;                /* HLT was executed */
    int          fault;                 /* fault code, 0 = ok */
    u64          cycles;                /* cycle counter */
    u64          instrs;                /* instruction counter */
} S64CPU;

/* fault codes */
#define S64_FAULT_NONE          0
#define S64_FAULT_ILLEGAL_OP    1
#define S64_FAULT_DIV_ZERO      2
#define S64_FAULT_BAD_ALIGN     3
#define S64_FAULT_BAD_ADDR      4

#endif /* S64_H */
