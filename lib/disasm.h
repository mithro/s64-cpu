#ifndef S64_DISASM_H
#define S64_DISASM_H

#include "../include/s64.h"

/* disassemble one instruction into buf, returns snprintf-style length */
int disasm_instr(u32 instr, u64 pc, char *buf, size_t bufsz);

#define DISASM_BUF_SIZE 64

#endif /* S64_DISASM_H */
