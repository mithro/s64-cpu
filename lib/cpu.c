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
    cpu->priv = S64_PRIV_SL0;  /* boot/firmware code is trusted by definition */
}

void cpu_reset(S64CPU *cpu) {
    u64 mode = cpu->mode;
    memset(cpu, 0, sizeof(S64CPU));
    cpu->mode = mode;
    cpu->priv = S64_PRIV_SL0;
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

/* ─── trap entry — shared by faults and the TRAP instruction ─────
 * Saves PC and current privilege into dedicated trap-state (NOT a
 * GPR -- the old code used R27 as a "trap link register" by pure
 * convention, which meant any userspace code could read or clobber
 * it). Always raises privilege to SL0; never lowers it -- a TRAP or
 * fault from SL2 still lands at SL0, same as real hardware. */
static void cpu_trap_enter(S64CPU *cpu, u8 cause) {
    cpu->trap_saved_pc   = cpu->pc;
    cpu->trap_saved_priv = cpu->priv;
    cpu->trap_cause       = cause;
    cpu->priv             = S64_PRIV_SL0;

    u64 handler = mem_read64(MEM_FAULT_VECTOR);
    if (handler) {
        cpu->pc = handler;
    } else {
        /* no handler installed -- nothing to dispatch to, so this is
         * unrecoverable for now. Distinct from "ran off the end of
         * memory": this is "trapped with nowhere to go". */
        cpu->halted = 1;
    }
}

/* ─── fault ───────────────────────────────────────────────────── */
static void cpu_fault(S64CPU *cpu, int code) {
    cpu->fault = code;
    cpu_trap_enter(cpu, (u8)code);
    if (mem_read64(MEM_FAULT_VECTOR))
        cpu->fault = S64_FAULT_NONE; /* handler will run; not a hard halt */
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
        if (v2 == 0) {
            cpu_fault(cpu, S64_FAULT_DIV_ZERO);
            if (cpu->halted) return -1;
            next_pc = cpu->pc;
            break;
        }
        reg_write(cpu, rd, (u64)((s64_t)v1 / (s64_t)v2));
        break;
    case OP_MOD:
        if (v2 == 0) {
            cpu_fault(cpu, S64_FAULT_DIV_ZERO);
            if (cpu->halted) return -1;
            next_pc = cpu->pc;
            break;
        }
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
    case OP_ST64: {
        /* Address is base register (rs2) + immediate offset (when M=1).
         * NOT v2/uimm -- those are only valid for register-register ALU
         * forms; here rs2 is always a real base register regardless of
         * M, so it must always be read directly. Pre-existing bug: this
         * used to compute v2+uimm, which for M=1 (the only form the
         * assembler's [Rs2 + #imm] syntax ever encodes) evaluates to
         * imm+imm -- silently dropping the base register and doubling
         * the offset. Found while testing TRAP/SRET against a non-zero
         * base register; affects every ST64/32/16/8 with a non-zero
         * base and non-zero offset, likely since before this session. */
        u64 addr = reg_read(cpu, rs2) + (m ? (u64)imm8 : 0);
        if (addr == MEM_FAULT_VECTOR && cpu->priv != S64_PRIV_SL0) {
            cpu_fault(cpu, S64_FAULT_PRIV);
            if (cpu->halted) return -1;
            next_pc = cpu->pc;
            break;
        }
        mem_write64(addr, reg_read(cpu, rd));
        break;
    }
    case OP_ST32:
        mem_write32(reg_read(cpu, rs2) + (m ? (u64)imm8 : 0),
                    reg_read(cpu, rd) & 0xFFFFFFFF);
        break;
    case OP_ST16:
        mem_write16(reg_read(cpu, rs2) + (m ? (u64)imm8 : 0),
                    reg_read(cpu, rd) & 0xFFFF);
        break;
    case OP_ST8:
        mem_write8(reg_read(cpu, rs2) + (m ? (u64)imm8 : 0),
                   reg_read(cpu, rd) & 0xFF);
        break;
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

    case OP_TRAP:
        /* imm8 = cause code, software-defined (e.g. syscall number,
         * or any app-specific reason). cpu_trap_enter() saves the
         * resume-after address (next_pc, not cpu->pc -- matches
         * CALL's Rd=PC+4 convention: TRAP resumes after itself, not
         * by re-executing), raises priv to SL0, jumps to the vector. */
        cpu->pc = next_pc;
        cpu_trap_enter(cpu, imm8);
        next_pc = cpu->pc;
        break;

    case OP_SRET:
        /* privileged: only SL0 may drop privilege back down */
        if (cpu->priv != S64_PRIV_SL0) {
            cpu_fault(cpu, S64_FAULT_PRIV);
            next_pc = cpu->pc;
            break;
        }
        cpu->priv = cpu->trap_saved_priv;
        next_pc   = cpu->trap_saved_pc;
        break;

    case OP_SETPRIV:
        /* privileged: only SL0 may set up a demotion target */
        if (cpu->priv != S64_PRIV_SL0) {
            cpu_fault(cpu, S64_FAULT_PRIV);
            next_pc = cpu->pc;
            break;
        }
        cpu->trap_saved_pc   = v1;            /* Rs1 = target entry PC */
        cpu->trap_saved_priv = imm8 & 0x03;   /* low 2 bits = target level */
        break;

    case OP_ILLEGAL:
        fprintf(stderr, "s64emu: illegal opcode 0x00 (OP_ILLEGAL) at PC 0x%016llx"
                " — likely jumped into unmapped/zeroed memory\n",
                (unsigned long long)cpu->pc);
        cpu_fault(cpu, S64_FAULT_ILLEGAL_OP);
        if (cpu->halted) return -1;
        next_pc = cpu->pc; /* handler ran, vector address already in pc */
        break;

    default:
        fprintf(stderr, "s64emu: illegal opcode 0x%02x at PC 0x%016llx\n",
                op, (unsigned long long)cpu->pc);
        cpu_fault(cpu, S64_FAULT_ILLEGAL_OP);
        if (cpu->halted) return -1;
        next_pc = cpu->pc;
        break;
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
    printf("  priv=SL%d  trap_cause=%u  trap_saved_pc=0x%016llx  trap_saved_priv=SL%d\n",
           cpu->priv, cpu->trap_cause,
           (unsigned long long)cpu->trap_saved_pc,
           cpu->trap_saved_priv);
}

/* ─── minimal syscall stub ────────────────────────────────────── */
__attribute__((weak))
void cpu_syscall(S64CPU *cpu, u8 num) {
    /* overriding this in main.c for full syscall support */
    (void)cpu; (void)num;
}
