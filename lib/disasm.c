#include "disasm.h"
#include <stdio.h>
#include <string.h>

/* register name table — SABI aliases */
static const char *reg_names[S64_NUM_REGS] = {
    "R0",  "R1",  "R2",  "R3",  "R4",  "R5",  "R6",  "R7",
    "R8",  "R9",  "R10", "R11", "R12", "R13", "R14", "R15",
    "R16", "R17", "R18", "R19", "R20", "R21", "R22", "R23",
    "R24", "R25", "R26", "R27", "SP",  "FP",  "LR",  "ZR"
};

static const char *rn(u8 r) {
    return reg_names[r & 0x1F];
}

int disasm_instr(u32 instr, u64 pc, char *buf, size_t bufsz) {
    u8  op   = INSTR_OPCODE(instr);
    u8  rd   = INSTR_RD(instr);
    u8  rs1  = INSTR_RS1(instr);
    u8  rs2  = INSTR_RS2(instr);
    u8  imm8 = INSTR_IMM8(instr);
    u8  m    = INSTR_M(instr);
    s8  simm = (s8)imm8;

#define OUT(...) snprintf(buf, bufsz, __VA_ARGS__)
#define RR(mnem) OUT("%-8s %s, %s, %s",  mnem, rn(rd), rn(rs1), rn(rs2))
#define RI(mnem) OUT("%-8s %s, %s, #%d", mnem, rn(rd), rn(rs1), (int)simm)
#define RX(mnem) (m ? RI(mnem) : RR(mnem))

    switch (op) {
    /* integer ALU */
    case OP_ADD:  case OP_ADDI: return RX("ADD");
    case OP_SUB:  case OP_SUBI: return RX("SUB");
    case OP_MUL:  case OP_MULI: return RX("MUL");
    case OP_DIV:  return RR("DIV");
    case OP_MOD:  return RR("MOD");

    /* bitwise */
    case OP_AND:  return RR("AND");
    case OP_OR:   return RR("OR");
    case OP_XOR:  return RR("XOR");
    case OP_NOT:  return OUT("%-8s %s, %s", "NOT", rn(rd), rn(rs1));
    case OP_SHL:  case OP_SHLI: return RX("SHL");
    case OP_SHR:  case OP_SHRI: return RX("SHR");
    case OP_SAR:  return RR("SAR");

    /* compare */
    case OP_EQ:   return RR("EQ");
    case OP_NE:   return RR("NE");
    case OP_LT:   return RR("LT");
    case OP_GT:   return RR("GT");
    case OP_LTE:  return RR("LTE");
    case OP_GTE:  return RR("GTE");
    case OP_LTU:  return RR("LTU");
    case OP_GTU:  return RR("GTU");

    /* control flow */
    case OP_JMP:
        return OUT("%-8s %s", "JMP", rn(rs1));
    case OP_JMPI: {
        u64 target = pc + (s64_t)simm * S64_INSTR_WIDTH;
        return OUT("%-8s #%d  ; -> 0x%llx", "JMPI",
                   (int)simm, (unsigned long long)target);
    }
    case OP_JEQ:
        return OUT("%-8s %s, %s", "JEQ", rn(rs1), rn(rs2));
    case OP_JNE:
        return OUT("%-8s %s, %s", "JNE", rn(rs1), rn(rs2));
    case OP_CALL:
        return OUT("%-8s %s, %s", "CALL", rn(rd), rn(rs1));
    case OP_RET:
        return OUT("%-8s %s", "RET", rn(rs1));
    case OP_LOOP:
        return OUT("%-8s %s, %s", "LOOP", rn(rd), rn(rs1));

    /* memory */
    case OP_LD64:
        return OUT("%-8s %s, [%s + #%d]", "LD64", rn(rd), rn(rs1), (int)simm);
    case OP_LD32:
        return OUT("%-8s %s, [%s + #%d]", "LD32", rn(rd), rn(rs1), (int)simm);
    case OP_LD16:
        return OUT("%-8s %s, [%s + #%d]", "LD16", rn(rd), rn(rs1), (int)simm);
    case OP_LD8:
        return OUT("%-8s %s, [%s + #%d]", "LD8",  rn(rd), rn(rs1), (int)simm);
    case OP_ST64:
        return OUT("%-8s %s, [%s + #%d]", "ST64", rn(rd), rn(rs2), (int)simm);
    case OP_ST32:
        return OUT("%-8s %s, [%s + #%d]", "ST32", rn(rd), rn(rs2), (int)simm);
    case OP_ST16:
        return OUT("%-8s %s, [%s + #%d]", "ST16", rn(rd), rn(rs2), (int)simm);
    case OP_ST8:
        return OUT("%-8s %s, [%s + #%d]", "ST8",  rn(rd), rn(rs2), (int)simm);
    case OP_MOV:
        return OUT("%-8s %s, %s", "MOV", rn(rd), rn(rs1));
    case OP_MOVI:
        return OUT("%-8s %s, #%u", "MOVI", rn(rd), (unsigned)imm8);
    case OP_MOVW:
        return OUT("%-8s %s, #0x%02x", "MOVW", rn(rd), (unsigned)imm8);

    /* float */
    case OP_FADD: return RR("FADD");
    case OP_FSUB: return RR("FSUB");
    case OP_FMUL: return RR("FMUL");
    case OP_FDIV: return RR("FDIV");
    case OP_FCMP: return RR("FCMP");
    case OP_FTOI: return OUT("%-8s %s, %s", "FTOI", rn(rd), rn(rs1));
    case OP_ITOF: return OUT("%-8s %s, %s", "ITOF", rn(rd), rn(rs1));

    /* system */
    case OP_NOP:  return OUT("NOP");
    case OP_HLT:  return OUT("HLT");
    case OP_SYS:  return OUT("%-8s #%u", "SYS", (unsigned)imm8);
    case OP_PUSH: return OUT("%-8s %s, %s", "PUSH", rn(rs1), rn(rs2));
    case OP_POP:  return OUT("%-8s %s, %s", "POP",  rn(rd),  rn(rs1));

    default:
        return OUT("???     0x%08x", instr);
    }
#undef OUT
#undef RR
#undef RI
#undef RX
}
