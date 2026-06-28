#include "cpu.h"
#include "memory.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

/* ─── init ────────────────────────────────────────────────────── */
void cpu_init(S64CPU *cpu, S64ExecMode mode) {
    memset(cpu, 0, sizeof(S64CPU));
    cpu->pc   = MEM_RAM_BASE;
    cpu->mode = mode;
}

void cpu_reset(S64CPU *cpu) {
    u64 mode = cpu->mode;
    memset(cpu, 0, sizeof(S64CPU));
    cpu->mode = mode;
    /* jump through reset vector */
    cpu->pc = mem_read64(MEM_RESET_VECTOR);
    if (cpu->pc == 0) cpu->pc = MEM_RAM_BASE; /* emulator default */
}

/* ─── register access — R31 is hardwired zero ─────────────────── */
static inline u64 reg_read(S64CPU *cpu, u8 r) {
    if (r == REG_R31) return 0;
    return cpu->regs[r];
}

static inline void reg_write(S64CPU *cpu, u8 r, u64 val) {
    if (r == REG_R31) return;   /* writes to R31 discarded */
    if (cpu->mode == S64_MODE_32BIT)
        val &= 0xFFFFFFFFULL;   /* upper 32 bits zeroed in S32 mode */
    cpu->regs[r] = val;
}

/* ─── fault ───────────────────────────────────────────────────── */
static void cpu_fault(S64CPU *cpu, int code) {
    cpu->fault  = code;
    cpu->halted = 1;
    /* jump to fault vector */
    u64 handler = mem_read64(MEM_FAULT_VECTOR);
    if (handler) {
        cpu->regs[REG_R27] = cpu->pc;  /* save PC to trap link reg */
        cpu->pc = handler;
        cpu->halted = 0;
        cpu->fault  = S64_FAULT_NONE;
    }
}

/* ─── single step ─────────────────────────────────────────────── */
int cpu_step(S64CPU *cpu) {
    if (cpu->halted) return -1;

    /* FETCH */
    u32 instr = mem_read32(cpu->pc);
    u64 next_pc = cpu->pc + S64_INSTR_WIDTH;

    /* DECODE */
    u8  op   = INSTR_OPCODE(instr);
    u8  rd   = INSTR_RD(instr);
    u8  rs1  = INSTR_RS1(instr);
    u8  rs2  = INSTR_RS2(instr);
    u8  imm8 = INSTR_IMM8(instr);
    u8  m    = INSTR_M(instr);

    u64 v1   = reg_read(cpu, rs1);
    u64 v2   = m ? (u64)(s64_t)(s8)imm8 : reg_read(cpu, rs2);
    u64 uimm = m ? (u64)imm8 : reg_read(cpu, rs2);

    /* EXECUTE */
    switch (op) {

    /* ── integer ALU ─────────────────────────────────────────── */
    case OP_ADD:  case OP_ADDI: reg_write(cpu, rd, v1 + v2); break;
    case OP_SUB:  case OP_SUBI: reg_write(cpu, rd, v1 - v2); break;
    case OP_MUL:  case OP_MULI: reg_write(cpu, rd, v1 * v2); break;
    case OP_DIV:
        if (v2 == 0) { cpu_fault(cpu, S64_FAULT_DIV_ZERO); return -1; }
        reg_write(cpu, rd, (u64)((s64_t)v1 / (s64_t)v2));
        break;
    case OP_MOD:
        if (v2 == 0) { cpu_fault(cpu, S64_FAULT_DIV_ZERO); return -1; }
        reg_write(cpu, rd, (u64)((s64_t)v1 % (s64_t)v2));
        break;

    /* ── bitwise & shift ─────────────────────────────────────── */
    case OP_AND:  reg_write(cpu, rd, v1 & v2); break;
    case OP_OR:   reg_write(cpu, rd, v1 | v2); break;
    case OP_XOR:  reg_write(cpu, rd, v1 ^ v2); break;
    case OP_NOT:  reg_write(cpu, rd, ~v1);      break;
    case OP_SHL:  case OP_SHLI: reg_write(cpu, rd, v1 << (v2 & 63)); break;
    case OP_SHR:  case OP_SHRI: reg_write(cpu, rd, v1 >> (v2 & 63)); break;
    case OP_SAR:
        reg_write(cpu, rd, (u64)((s64_t)v1 >> (v2 & 63)));
        break;

    /* ── compare ─────────────────────────────────────────────── */
    case OP_EQ:  reg_write(cpu, rd, v1 == v2 ? 1 : 0); break;
    case OP_NE:  reg_write(cpu, rd, v1 != v2 ? 1 : 0); break;
    case OP_LT:  reg_write(cpu, rd, (s64_t)v1 <  (s64_t)v2 ? 1 : 0); break;
    case OP_GT:  reg_write(cpu, rd, (s64_t)v1 >  (s64_t)v2 ? 1 : 0); break;
    case OP_LTE: reg_write(cpu, rd, (s64_t)v1 <= (s64_t)v2 ? 1 : 0); break;
    case OP_GTE: reg_write(cpu, rd, (s64_t)v1 >= (s64_t)v2 ? 1 : 0); break;
    case OP_LTU: reg_write(cpu, rd, v1 <  v2 ? 1 : 0); break;
    case OP_GTU: reg_write(cpu, rd, v1 >  v2 ? 1 : 0); break;

    /* ── control flow ────────────────────────────────────────── */
    case OP_JMP:
        next_pc = v1;
        break;
    case OP_JMPI:
        next_pc = cpu->pc + (s64_t)(s8)imm8 * S64_INSTR_WIDTH;
        break;
    case OP_JEQ:
        if (reg_read(cpu, rs2) == 1) next_pc = v1;
        break;
    case OP_JNE:
        if (reg_read(cpu, rs2) == 0) next_pc = v1;
        break;
    case OP_CALL:
        reg_write(cpu, rd, next_pc);
        next_pc = v1;
        break;
    case OP_RET:
        next_pc = v1;
        break;
    case OP_LOOP:
        v1 = reg_read(cpu, rd) - 1;
        reg_write(cpu, rd, v1);
        if (v1 != 0) next_pc = reg_read(cpu, rs1);
        break;

    /* ── memory ──────────────────────────────────────────────── */
    case OP_LD64: reg_write(cpu, rd, mem_read64(v1 + uimm)); break;
    case OP_LD32: reg_write(cpu, rd, mem_read32(v1 + uimm)); break;
    case OP_LD16: reg_write(cpu, rd, mem_read16(v1 + uimm)); break;
    case OP_LD8:  reg_write(cpu, rd, mem_read8 (v1 + uimm)); break;
    case OP_ST64: mem_write64(v2 + uimm, reg_read(cpu, rd)); break;
    case OP_ST32: mem_write32(v2 + uimm, reg_read(cpu, rd) & 0xFFFFFFFF); break;
    case OP_ST16: mem_write16(v2 + uimm, reg_read(cpu, rd) & 0xFFFF); break;
    case OP_ST8:  mem_write8 (v2 + uimm, reg_read(cpu, rd) & 0xFF);   break;
    case OP_MOV:  reg_write(cpu, rd, v1); break;
    case OP_MOVI: reg_write(cpu, rd, uimm); break;
    case OP_MOVW:
        reg_write(cpu, rd, (reg_read(cpu, rd) << 8) | imm8);
        break;

    /* ── floating point ──────────────────────────────────────── */
    case OP_FADD: {
        double a, b, r;
        memcpy(&a, &cpu->regs[rs1], 8);
        memcpy(&b, &cpu->regs[rs2], 8);
        r = a + b;
        memcpy(&cpu->regs[rd], &r, 8);
        break;
    }
    case OP_FSUB: {
        double a, b, r;
        memcpy(&a, &cpu->regs[rs1], 8);
        memcpy(&b, &cpu->regs[rs2], 8);
        r = a - b;
        memcpy(&cpu->regs[rd], &r, 8);
        break;
    }
    case OP_FMUL: {
        double a, b, r;
        memcpy(&a, &cpu->regs[rs1], 8);
        memcpy(&b, &cpu->regs[rs2], 8);
        r = a * b;
        memcpy(&cpu->regs[rd], &r, 8);
        break;
    }
    case OP_FDIV: {
        double a, b, r;
        memcpy(&a, &cpu->regs[rs1], 8);
        memcpy(&b, &cpu->regs[rs2], 8);
        r = a / b;
        memcpy(&cpu->regs[rd], &r, 8);
        break;
    }
    case OP_FCMP: {
        double a, b;
        memcpy(&a, &cpu->regs[rs1], 8);
        memcpy(&b, &cpu->regs[rs2], 8);
        reg_write(cpu, rd, a < b ? (u64)-1 : (a == b ? 0 : 1));
        break;
    }
    case OP_FTOI: {
        double a;
        memcpy(&a, &cpu->regs[rs1], 8);
        reg_write(cpu, rd, (u64)(s64_t)a);
        break;
    }
    case OP_ITOF: {
        double r = (double)(s64_t)v1;
        memcpy(&cpu->regs[rd], &r, 8);
        break;
    }

    /* ── system ──────────────────────────────────────────────── */
    case OP_NOP:  break;
    case OP_HLT:  cpu->halted = 1; break;
    case OP_SYS:
        cpu_syscall(cpu, imm8);
        break;
    case OP_PUSH:
        mem_write64(reg_read(cpu, rs2), reg_read(cpu, rs1));
        reg_write(cpu, rs2, reg_read(cpu, rs2) - 8);
        break;
    case OP_POP:
        reg_write(cpu, rs1, reg_read(cpu, rs1) + 8);
        reg_write(cpu, rd, mem_read64(reg_read(cpu, rs1)));
        break;

    default:
        fprintf(stderr, "s64emu: illegal opcode 0x%02x at PC 0x%016llx\n",
                op, (unsigned long long)cpu->pc);
        cpu_fault(cpu, S64_FAULT_ILLEGAL_OP);
        return -1;
    }

    cpu->pc = next_pc;
    cpu->instrs++;
    cpu->cycles++;
    return 0;
}

/* ─── run until halt ──────────────────────────────────────────── */
int cpu_run(S64CPU *cpu) {
    while (!cpu->halted)
        if (cpu_step(cpu)) return -1;
    return 0;
}

/* ─── register dump ───────────────────────────────────────────── */
void cpu_dump_regs(S64CPU *cpu) {
    printf("S64 CPU register dump (mode=%s):\n",
           cpu->mode == S64_MODE_64BIT ? "64-bit" : "32-bit");
    for (int i = 0; i < S64_NUM_REGS; i++) {
        printf("  R%-2d = 0x%016llx", i,
               (unsigned long long)cpu->regs[i]);
        if (i % 2 == 1) printf("\n");
        else             printf("    ");
    }
    printf("  PC  = 0x%016llx\n", (unsigned long long)cpu->pc);
    printf("  cycles=%llu  instrs=%llu  halted=%d  fault=%d\n",
           (unsigned long long)cpu->cycles,
           (unsigned long long)cpu->instrs,
           cpu->halted, cpu->fault);
}

/* ─── minimal syscall stub ────────────────────────────────────── */
__attribute__((weak))
void cpu_syscall(S64CPU *cpu, u8 num) {
    /* override this in main.c for full syscall support */
    (void)cpu; (void)num;
}
