#ifndef S64_LOADER_H
#define S64_LOADER_H

#include "../include/s64.h"
#include "cpu.h"

int loader_load(const char *path, S64CPU *cpu);
int loader_load_flat(const char *path, S64CPU *cpu, u64 base);

#endif /* S64_LOADER_H */
