#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "../include/s64.h"
#include "../lib/cpu.h"
#include "../lib/memory.h"
#include "../lib/loader.h"
#include "../lib/disasm.h"

/* ─── breakpoints ─────────────────────────────────────────────── */
#define MAX_BREAKPOINTS 64

static u64 breakpoints[MAX_BREAKPOINTS];
static int bp_count = 0;

static int bp_add(u64 addr) {
    if (bp_count >= MAX_BREAKPOINTS) return -1;
    breakpoints[bp_count++] = addr;
    printf("breakpoint %d set at 0x%016llx\n", bp_count,
           (unsigned long long)addr);
    return 0;
}

static int bp_hit(u64 addr) {
    for (int i = 0; i < bp_count; i++)
        if (breakpoints[i] == addr) return i + 1;
    return 0;
}

static void bp_list(void) {
    if (bp_count == 0) { printf("no breakpoints\n"); return; }
    for (int i = 0; i < bp_count; i++)
        printf("  [%d] 0x%016llx\n", i + 1,
               (unsigned long long)breakpoints[i]);
}

static void bp_del(int idx) {
    if (idx < 1 || idx > bp_count) { printf("invalid breakpoint\n"); return; }
    for (int i = idx - 1; i < bp_count - 1; i++)
        breakpoints[i] = breakpoints[i + 1];
    bp_count--;
    printf("breakpoint %d deleted\n", idx);
}

/* ─── syscall handler ─────────────────────────────────────────── */
void cpu_syscall(S64CPU *cpu, u8 num) {
    switch (num) {
    case 1: { /* write(fd, buf, len) */
        u64 fd  = cpu->regs[0];
        u64 buf = cpu->regs[1];
        u64 len = cpu->regs[2];
        if (fd == 1 || fd == 2) {
            for (u64 i = 0; i < len; i++)
                fputc(mem_read8(buf + i), fd == 1 ? stdout : stderr);
        }
        cpu->regs[0] = len;
        break;
    }
    case 60: /* exit(code) */
        printf("\n[s64emu] exit(%llu)\n",
               (unsigned long long)cpu->regs[0]);
        cpu->halted = 1;
        break;
    default:
        fprintf(stderr, "s64emu: unknown syscall %u\n", num);
        cpu->regs[0] = (u64)-1;
        break;
    }
}

/* ─── print one instruction at pc ────────────────────────────── */
static void print_instr(S64CPU *cpu) {
    u32 instr = mem_read32(cpu->pc);
    char buf[DISASM_BUF_SIZE];
    disasm_instr(instr, cpu->pc, buf, sizeof(buf));
    printf("0x%016llx:  %08x  %s\n",
           (unsigned long long)cpu->pc, instr, buf);
}

/* ─── debug REPL ──────────────────────────────────────────────── */
static void debug_repl(S64CPU *cpu) {
    char line[256];
    printf("s64emu debugger — type 'h' for help\n");
    print_instr(cpu);

    while (!cpu->halted) {
        printf("(s64) ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) break;

        /* strip newline */
        line[strcspn(line, "\n")] = 0;

        char cmd = line[0];
        char *arg = line[1] ? line + 2 : NULL;

        switch (cmd) {
        case 's': case '\0': case '\n': {
            /* step */
            int bp = bp_hit(cpu->pc);
            if (bp) printf("[breakpoint %d hit]\n", bp);
            cpu_step(cpu);
            if (!cpu->halted) print_instr(cpu);
            else printf("[halted]\n");
            break;
        }
        case 'c': {
            /* continue until breakpoint or halt */
            while (!cpu->halted) {
                cpu_step(cpu);
                if (bp_hit(cpu->pc)) {
                    printf("[breakpoint hit at 0x%016llx]\n",
                           (unsigned long long)cpu->pc);
                    print_instr(cpu);
                    break;
                }
            }
            if (cpu->halted) printf("[halted]\n");
            break;
        }
        case 'r':
            /* dump registers */
            cpu_dump_regs(cpu);
            break;
        case 'b':
            /* breakpoint */
            if (!arg) { bp_list(); break; }
            if (arg[0] == 'd') {
                bp_del(atoi(arg + 2));
            } else {
                u64 addr = (u64)strtoull(arg, NULL, 0);
                bp_add(addr);
            }
            break;
        case 'm': {
            /* memory dump: m <addr> [len] */
            if (!arg) { printf("usage: m <addr> [len]\n"); break; }
            char *sp = strchr(arg, ' ');
            u64 addr = (u64)strtoull(arg, NULL, 0);
            u64 len  = sp ? (u64)strtoull(sp + 1, NULL, 0) : 64;
            mem_dump(addr, len);
            break;
        }
        case 'd': {
            /* disassemble: d <addr> [count] */
            if (!arg) { printf("usage: d <addr> [count]\n"); break; }
            char *sp = strchr(arg, ' ');
            u64 addr  = (u64)strtoull(arg, NULL, 0);
            int count = sp ? atoi(sp + 1) : 8;
            char dbuf[DISASM_BUF_SIZE];
            for (int i = 0; i < count; i++) {
                u32 ins = mem_read32(addr);
                disasm_instr(ins, addr, dbuf, sizeof(dbuf));
                printf("0x%016llx:  %08x  %s\n",
                       (unsigned long long)addr, ins, dbuf);
                addr += 4;
            }
            break;
        }
        case 'p': {
            /* print register: p R5 */
            if (!arg) { printf("usage: p <Rn>\n"); break; }
            int n = atoi(arg + 1);
            if (n >= 0 && n < S64_NUM_REGS)
                printf("R%d = 0x%016llx (%lld)\n", n,
                       (unsigned long long)cpu->regs[n],
                       (long long)cpu->regs[n]);
            break;
        }
        case 'w': {
            /* write register: w R5 0x42 */
            if (!arg) { printf("usage: w <Rn> <val>\n"); break; }
            int n = atoi(arg + 1);
            char *vp = strchr(arg, ' ');
            if (vp && n >= 0 && n < S64_NUM_REGS) {
                cpu->regs[n] = (u64)strtoull(vp + 1, NULL, 0);
                printf("R%d = 0x%016llx\n", n,
                       (unsigned long long)cpu->regs[n]);
            }
            break;
        }
        case 'i':
            /* info */
            printf("PC=0x%016llx  cycles=%llu  instrs=%llu  fault=%d\n",
                   (unsigned long long)cpu->pc,
                   (unsigned long long)cpu->cycles,
                   (unsigned long long)cpu->instrs,
                   cpu->fault);
            break;
        case 'q':
            printf("quit\n");
            return;
        case 'h': default:
            printf(
                "s64emu debugger commands:\n"
                "  s / <enter>     step one instruction\n"
                "  c               continue until breakpoint or halt\n"
                "  r               dump all registers\n"
                "  p <Rn>          print register value\n"
                "  w <Rn> <val>    write register value\n"
                "  b               list breakpoints\n"
                "  b <addr>        set breakpoint at address\n"
                "  b d <n>         delete breakpoint n\n"
                "  m <addr> [len]  hex dump memory (default 64 bytes)\n"
                "  d <addr> [n]    disassemble n instructions (default 8)\n"
                "  i               show PC, cycles, fault\n"
                "  q               quit\n"
            );
            break;
        }
    }
}

/* ─── usage ───────────────────────────────────────────────────── */
static void usage(const char *prog) {
    fprintf(stderr,
        "usage: %s [options] <file.x64 | file.bin>\n"
        "options:\n"
        "  --flat [addr]   load as flat binary at addr (default 0x8000)\n"
        "  --step          start in debugger\n"
        "  --s32           force 32-bit mode\n"
        "  --be            big-endian (stub — not yet implemented)\n"
        "  --dump-regs     dump registers after execution\n"
        "  --max <n>       stop after n instructions\n"
        "  -h              show this help\n",
        prog);
}

/* ─── main ────────────────────────────────────────────────────── */
int main(int argc, char **argv) {
    if (argc < 2) { usage(argv[0]); return 1; }

    int opt_step     = 0;
    int opt_flat     = 0;
    int opt_s32      = 0;
    int opt_be       = 0;
    int opt_dumpregs = 0;
    u64 opt_flatbase = SXF_DEFAULT_BASE;
    u64 opt_max      = 0;
    const char *file = NULL;

    for (int i = 1; i < argc; i++) {
        if      (!strcmp(argv[i], "--step"))     opt_step = 1;
        else if (!strcmp(argv[i], "--flat"))  {
            opt_flat = 1;
            if (i + 1 < argc && argv[i+1][0] != '-')
                opt_flatbase = (u64)strtoull(argv[++i], NULL, 0);
        }
        else if (!strcmp(argv[i], "--s32"))      opt_s32 = 1;
        else if (!strcmp(argv[i], "--be"))       opt_be  = 1;
        else if (!strcmp(argv[i], "--dump-regs"))opt_dumpregs = 1;
        else if (!strcmp(argv[i], "--max") && i+1 < argc)
            opt_max = (u64)strtoull(argv[++i], NULL, 0);
        else if (!strcmp(argv[i], "-h")) { usage(argv[0]); return 0; }
        else file = argv[i];
    }

    if (!file) { usage(argv[0]); return 1; }

    if (opt_be) {
        fprintf(stderr, "s64emu: S64-BEXT big-endian not yet implemented\n");
        return 1;
    }

    /* init */
    if (mem_init()) return 1;

    S64CPU cpu;
    cpu_init(&cpu, opt_s32 ? S64_MODE_32BIT : S64_MODE_64BIT);

    /* load */
    int rc;
    if (opt_flat)
        rc = loader_load_flat(file, &cpu, opt_flatbase);
    else
        rc = loader_load(file, &cpu);

    if (rc) { mem_free(); return 1; }

    /* run */
    if (opt_step) {
        debug_repl(&cpu);
    } else {
        while (!cpu.halted) {
            if (opt_max && cpu.instrs >= opt_max) {
                printf("[s64emu] max instructions reached (%llu)\n",
                       (unsigned long long)opt_max);
                break;
            }
            if (bp_hit(cpu.pc)) {
                printf("[breakpoint at 0x%016llx]\n",
                       (unsigned long long)cpu.pc);
                debug_repl(&cpu);
            }
            cpu_step(&cpu);
        }
    }

    if (opt_dumpregs) cpu_dump_regs(&cpu);

    mem_free();
    return cpu.fault ? 1 : 0;
}
