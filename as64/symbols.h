#ifndef AS64_SYMBOLS_H
#define AS64_SYMBOLS_H

#include <stdint.h>
#include <stddef.h>

#define MAX_SYMBOLS     4096
#define MAX_RELOCS      8192
#define MAX_GLOBALS     256

typedef enum {
    SYM_UNDEF  = 0,
    SYM_LABEL,      /* code/data label */
    SYM_EQU,        /* .equ constant */
    SYM_EXTERN,     /* .extern — defined elsewhere */
} SymbolType;

typedef struct {
    char        name[128];
    SymbolType  type;
    uint64_t    value;      /* address or constant */
    int         section;    /* section index */
    int         global;     /* exported via .global */
    int         defined;
} Symbol;

typedef enum {
    RELOC_ABS    = 1,
    RELOC_PCREL  = 2,
    RELOC_WIDE   = 3,   /* reserved for LA pseudo-op: patches a 4-instruction
                         * MOVI+3xMOVW chain as one unit. NOT YET IMPLEMENTED
                         * — symtab_resolve()/reloc_patch() don't act on this
                         * type yet. Added so the struct layout is settled
                         * before the actual backpatch logic is written. */
} RelocType;

typedef struct {
    char        sym_name[128];
    uint64_t    offset;     /* byte offset in section where patch goes */
    int         section;
    RelocType   type;
    int         instr_field; /* which field: 0=full imm8, 1=jmpi offset */
    int         patched;     /* 1 once backpatch_relocs() has written the
                              * real bytes (intra-file case) -- write_o64()
                              * skips these; only unpatched (extern) relocs
                              * get serialized into the .o64 reloc table. */
} Reloc;

typedef struct {
    Symbol  syms[MAX_SYMBOLS];
    int     sym_count;

    Reloc   relocs[MAX_RELOCS];
    int     reloc_count;

    char    globals[MAX_GLOBALS][128];
    int     global_count;
} SymTable;

void    symtab_init(SymTable *st);
Symbol *symtab_find(SymTable *st, const char *name);
Symbol *symtab_define(SymTable *st, const char *name, SymbolType type,
                      uint64_t value, int section);
void    symtab_add_global(SymTable *st, const char *name);
void    symtab_add_extern(SymTable *st, const char *name);
int     symtab_add_reloc(SymTable *st, const char *sym,
                         uint64_t offset, int section, RelocType type, int field);
int     symtab_resolve(SymTable *st); /* returns number of unresolved */
void    symtab_dump(SymTable *st);

#endif
