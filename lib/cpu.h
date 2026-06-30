#ifndef S64_CPU_H
#define S64_CPU_H

#include "../include/s64.h"

void cpu_init(S64CPU *cpu, S64ExecMode mode);
void cpu_reset(S64CPU *cpu);
int  cpu_step(S64CPU *cpu);
int  cpu_run(S64CPU *cpu);
void cpu_dump_regs(S64CPU *cpu);
void cpu_syscall(S64CPU *cpu, u8 num);  /* override in main */

#endif /* S64_CPU_H */
