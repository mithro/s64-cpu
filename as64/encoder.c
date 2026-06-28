#include "encoder.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* ─── opcode table ────────────────────────────────────────────── */
typedef struct {
    const char *mnem;
    uint8_t     opcode;
    int         force_m;    /* -1=auto, 0=force M=0, 1=force M=1 */
} OpcodeEntry;

static const OpcodeEntry opcode_table[] = {
    /* integer ALU */
    { "ADD",   OP_ADD,  -1 },
    { "ADDI",  OP_ADDI,  1 },
    { "SUB",   OP_SUB,  -1 },
    { "SUBI",  OP_SUBI,  1 },
    { "MUL",   OP_MUL,  -1 },
    { "MULI",  OP_MULI,  1 },
    { "DIV",   OP_DIV,   0 },
    { "MOD",   OP_MOD,   0 },
    /* bitwise */
    { "AND",   OP_AND,   0 },
    { "OR",    OP_OR,    0 },
    { "XOR",   OP_XOR,   0 },
    { "NOT",   OP_NOT,   0 },
    { "SHL",   OP_SHL,  -1 },
    { "SHLI",  OP_SHLI,  1 },
    { "SHR",   OP_SHR,  -1 },
    { "SHRI",  OP_SHRI,  1 },
    { "SAR",   OP_SAR,   0 },
    /* compare */
    { "EQ",    OP_EQ,    0 },
    { "NE",    OP_NE,    0 },
    { "LT",    OP_LT,    0 },
    { "GT",    OP_GT,    0 },
    { "LTE",   OP_LTE,   0 },
    { "GTE",   OP_GTE,   0 },
    { "LTU",   OP_LTU,   0 },
    { "GTU",   OP_GTU,   0 },
    /* control flow */
    { "JMP",   OP_JMP,   0 },
    { "JMPI",  OP_JMPI,  1 },
    { "JEQ",   OP_JEQ,   0 },
    { "JNE",   OP_JNE,   0 },
    { "CALL",  OP_CALL,  0 },
    { "RET",   OP_RET,   0 },
    { "LOOP",  OP_LOOP,  0 },
    /* memory */
    { "LD64",  OP_LD64,  1 },
    { "LD32",  OP_LD32,  1 },
    { "LD16",  OP_LD16,  1 },
    { "LD8",   OP_LD8,   1 },
    { "ST64",  OP_ST64,  1 },
    { "ST32",  OP_ST32,  1 },
    { "ST16",  OP_ST16,  1 },
    { "ST8",   OP_ST8,   1 },
    { "MOV",   OP_MOV,   0 },
    { "MOVI",  OP_MOVI,  1 },
    { "MOVW",  OP_MOVW,  1 },
    /* float */
    { "FADD",  OP_FADD,  0 },
    { "FSUB",  OP_FSUB,  0 },
    { "FMUL",  OP_FMUL,  0 },
    { "FDIV",  OP_FDIV,  0 },
    { "FCMP",  OP_FCMP,  0 },
    { "FTOI",  OP_FTOI,  0 },
    { "ITOF",  OP_ITOF,  0 },
    /* system */
    { "NOP",   OP_NOP,   0 },
    { "HLT",   OP_HLT,   0 },
    { "SYS",   OP_SYS,   1 },
    { "PUSH",  OP_PUSH,  0 },
    { "POP",   OP_POP,   0 },
    { NULL, 0, 0 }
};

int encoder_lookup_opcode(const char *mnemonic) {
    char upper[32];
    size_t i;
    for (i = 0; mnemonic[i] && i < sizeof(upper)-1; i++)
        upper[i] = toupper((unsigned char)mnemonic[i]);
    upper[i] = 0;

    for (i = 0; opcode_table[i].mnem; i++)
        if (!strcmp(opcode_table[i].mnem, upper))
            return opcode_table[i].opcode;
    return -1;
}

static const OpcodeEntry *find_entry(const char *mnemonic) {
    char upper[32];
    size_t i;
    for (i = 0; mnemonic[i] && i < sizeof(upper)-1; i++)
        upper[i] = toupper((unsigned char)mnemonic[i]);
    upper[i] = 0;
    for (i = 0; opcode_table[i].mnem; i++)
        if (!strcmp(opcode_table[i].mnem, upper))
            return &opcode_table[i];
    return NULL;
}

static uint32_t encode(uint8_t op, uint8_t rd, uint8_t rs1,
                       uint8_t rs2, uint8_t imm8, uint8_t m) {
    return ENCODE_INSTR(op, rd, rs1, rs2, imm8, m);
}

#define ERR(fmt, ...) do { \
    fprintf(stderr, "%s:%d: error: " fmt "\n", \
            filename, ins->line, ##__VA_ARGS__); \
    return 0xFFFFFFFF; \
} while(0)

#define REQ_OPS(n) do { if (ins->op_count < (n)) \
    ERR("'%s' requires %d operand(s)", ins->mnemonic, (n)); } while(0)

#define IS_REG(i) (ins->ops[i].type == OPT_REG)
#define IS_IMM(i) (ins->ops[i].type == OPT_IMM || ins->ops[i].type == OPT_LABEL)
#define IS_MEM(i) (ins->ops[i].type == OPT_MEM_REG_IMM)

#define RD   ((uint8_t)ins->ops[0].reg)
#define RS1  ((uint8_t)ins->ops[1].reg)
#define RS2  ((uint8_t)ins->ops[2].reg)
#define IMM2 ((uint8_t)(ins->ops[2].imm & 0xFF))
#define IMM1 ((uint8_t)(ins->ops[1].imm & 0xFF))
#define IMM0 ((uint8_t)(ins->ops[0].imm & 0xFF))

uint32_t encoder_encode(const Instruction *ins, const char *filename) {
    const OpcodeEntry *e = find_entry(ins->mnemonic);
    if (!e) ERR("unknown mnemonic '%s'", ins->mnemonic);

    uint8_t op = e->opcode;

    switch (op) {

    /* ── 3-register: Rd, Rs1, Rs2  or  Rd, Rs1, #imm ─────────── */
    case OP_ADD: case OP_SUB: case OP_MUL:
    case OP_SHL: case OP_SHR:
    case OP_ADDI: case OP_SUBI: case OP_MULI:
    case OP_SHLI: case OP_SHRI: {
        REQ_OPS(3);
        if (IS_REG(2))
            return encode(op, RD, RS1, RS2, 0, 0);
        if (IS_IMM(2)) {
            uint8_t imm = IMM2;
            if (ins->ops[2].imm < -128 || ins->ops[2].imm > 255)
                ERR("immediate %lld out of range (-128..255)",
                    (long long)ins->ops[2].imm);
            return encode(op, RD, RS1, 0, imm, 1);
        }
        ERR("'%s': bad operand 3", ins->mnemonic);
    }

    /* ── 2-register only ──────────────────────────────────────── */
    case OP_DIV: case OP_MOD:
    case OP_AND: case OP_OR: case OP_XOR: case OP_SAR:
    case OP_EQ:  case OP_NE:
    case OP_LT:  case OP_GT: case OP_LTE: case OP_GTE:
    case OP_LTU: case OP_GTU:
    case OP_FADD: case OP_FSUB: case OP_FMUL: case OP_FDIV:
    case OP_FCMP:
        REQ_OPS(3);
        if (!IS_REG(2)) ERR("'%s' requires register as operand 3", ins->mnemonic);
        return encode(op, RD, RS1, RS2, 0, 0);

    /* ── NOT, FTOI, ITOF: Rd, Rs1 ─────────────────────────────── */
    case OP_NOT: case OP_FTOI: case OP_ITOF:
        REQ_OPS(2);
        return encode(op, RD, RS1, 0, 0, 0);

    /* ── MOV: Rd, Rs1 ─────────────────────────────────────────── */
    case OP_MOV:
        REQ_OPS(2);
        return encode(op, RD, RS1, 0, 0, 0);

    /* ── MOVI: Rd, #imm ───────────────────────────────────────── */
    case OP_MOVI:
        REQ_OPS(2);
        return encode(op, RD, 0, 0, IMM1, 1);

    /* ── MOVW: Rd, #imm  (build wide immediate) ───────────────── */
    case OP_MOVW:
        REQ_OPS(2);
        return encode(op, RD, 0, 0, IMM1, 1);

    /* ── JMP: Rs1 ─────────────────────────────────────────────── */
    case OP_JMP:
        REQ_OPS(1);
        return encode(op, 0, (uint8_t)ins->ops[0].reg, 0, 0, 0);

    /* ── JMPI: #imm ───────────────────────────────────────────── */
    case OP_JMPI:
        REQ_OPS(1);
        return encode(op, 0, 0, 0, IMM0, 1);

    /* ── JEQ / JNE: Rs1 (target), Rs2 (flag) ─────────────────── */
    case OP_JEQ: case OP_JNE:
        REQ_OPS(2);
        return encode(op, 0, (uint8_t)ins->ops[0].reg,
                      (uint8_t)ins->ops[1].reg, 0, 0);

    /* ── CALL: Rd, Rs1 ────────────────────────────────────────── */
    case OP_CALL:
        REQ_OPS(2);
        return encode(op, RD, RS1, 0, 0, 0);

    /* ── RET: Rs1 ─────────────────────────────────────────────── */
    case OP_RET:
        REQ_OPS(1);
        return encode(op, 0, (uint8_t)ins->ops[0].reg, 0, 0, 0);

    /* ── LOOP: Rd, Rs1 ────────────────────────────────────────── */
    case OP_LOOP:
        REQ_OPS(2);
        return encode(op, RD, RS1, 0, 0, 0);

    /* ── loads: Rd, [Rs1 + #off] ──────────────────────────────── */
    case OP_LD64: case OP_LD32: case OP_LD16: case OP_LD8:
        REQ_OPS(2);
        if (IS_MEM(1)) {
            return encode(op, RD,
                          (uint8_t)ins->ops[1].mem_reg, 0,
                          (uint8_t)(ins->ops[1].mem_off & 0xFF), 1);
        }
        ERR("'%s' requires [Rs + #off] as operand 2", ins->mnemonic);

    /* ── stores: Rs1, [Rs2 + #off] ────────────────────────────── */
    case OP_ST64: case OP_ST32: case OP_ST16: case OP_ST8:
        REQ_OPS(2);
        if (IS_MEM(1)) {
            return encode(op, RD,
                          0,
                          (uint8_t)ins->ops[1].mem_reg,
                          (uint8_t)(ins->ops[1].mem_off & 0xFF), 1);
        }
        ERR("'%s' requires [Rs + #off] as operand 2", ins->mnemonic);

    /* ── NOP / HLT ────────────────────────────────────────────── */
    case OP_NOP: case OP_HLT:
        return encode(op, 0, 0, 0, 0, 0);

    /* ── SYS: #imm ────────────────────────────────────────────── */
    case OP_SYS:
        REQ_OPS(1);
        return encode(op, 0, 0, 0, IMM0, 1);

    /* ── PUSH: Rs1, Rs2 ───────────────────────────────────────── */
    case OP_PUSH:
        REQ_OPS(2);
        return encode(op, 0, RS1, RS2, 0, 0);

    /* ── POP: Rd, Rs1 ─────────────────────────────────────────── */
    case OP_POP:
        REQ_OPS(2);
        return encode(op, RD, RS1, 0, 0, 0);

    default:
        ERR("unhandled opcode 0x%02x", op);
    }
}
