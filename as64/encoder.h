#ifndef AS64_ENCODER_H
#define AS64_ENCODER_H

#include <stdint.h>
#include "../include/s64.h"

/* operand types */
typedef enum {
    OPT_NONE = 0,
    OPT_REG,
    OPT_IMM,
    OPT_LABEL,
    OPT_MEM_REG_IMM,   /* [Rs + #imm] */
} OperandType;

typedef struct {
    OperandType type;
    int         reg;        /* register number 0–31 */
    int64_t     imm;        /* immediate value */
    char        label[128]; /* label name for relocation */
    int         mem_reg;    /* base register for MEM_REG_IMM */
    int64_t     mem_off;    /* offset for MEM_REG_IMM */
} Operand;

typedef struct {
    char    mnemonic[32];
    Operand ops[4];
    int     op_count;
    int     line;
} Instruction;

/* encode one instruction — returns 32-bit word or 0xFFFFFFFF on error */
uint32_t encoder_encode(const Instruction *ins, const char *filename);

/* lookup opcode by mnemonic — returns opcode or -1 */
int encoder_lookup_opcode(const char *mnemonic);

#endif
