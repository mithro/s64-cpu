#include "symbols.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void symtab_init(SymTable *st) {
    memset(st, 0, sizeof(SymTable));
}

Symbol *symtab_find(SymTable *st, const char *name) {
    for (int i = 0; i < st->sym_count; i++)
        if (!strcmp(st->syms[i].name, name))
            return &st->syms[i];
    return NULL;
}

Symbol *symtab_define(SymTable *st, const char *name, SymbolType type,
                      uint64_t value, int section) {
    Symbol *s = symtab_find(st, name);
    if (!s) {
        if (st->sym_count >= MAX_SYMBOLS) {
            fprintf(stderr, "as64: symbol table full\n");
            return NULL;
        }
        s = &st->syms[st->sym_count++];
        strncpy(s->name, name, sizeof(s->name) - 1);
    }
    s->type    = type;
    s->value   = value;
    s->section = section;
    s->defined = 1;

    /* check if it was declared global */
    for (int i = 0; i < st->global_count; i++)
        if (!strcmp(st->globals[i], name))
            s->global = 1;

    return s;
}

void symtab_add_global(SymTable *st, const char *name) {
    if (st->global_count >= MAX_GLOBALS) return;
    strncpy(st->globals[st->global_count++], name,
            sizeof(st->globals[0]) - 1);

    /* mark existing symbol if already defined */
    Symbol *s = symtab_find(st, name);
    if (s) s->global = 1;
}

void symtab_add_extern(SymTable *st, const char *name) {
    if (!symtab_find(st, name))
        symtab_define(st, name, SYM_EXTERN, 0, -1);
}

int symtab_add_reloc(SymTable *st, const char *sym,
                     uint64_t offset, int section, RelocType type, int field) {
    if (st->reloc_count >= MAX_RELOCS) {
        fprintf(stderr, "as64: relocation table full\n");
        return -1;
    }
    Reloc *r = &st->relocs[st->reloc_count++];
    strncpy(r->sym_name, sym, sizeof(r->sym_name) - 1);
    r->offset      = offset;
    r->section     = section;
    r->type        = type;
    r->instr_field = field;
    return 0;
}

int symtab_resolve(SymTable *st) {
    int unresolved = 0;
    for (int i = 0; i < st->reloc_count; i++) {
        Reloc  *r = &st->relocs[i];
        Symbol *s = symtab_find(st, r->sym_name);
        if (!s || !s->defined) {
            fprintf(stderr, "as64: undefined symbol '%s'\n", r->sym_name);
            unresolved++;
        }
    }
    return unresolved;
}

void symtab_dump(SymTable *st) {
    printf("symbol table (%d entries):\n", st->sym_count);
    for (int i = 0; i < st->sym_count; i++) {
        Symbol *s = &st->syms[i];
        printf("  %-32s  0x%016llx  sect=%-2d  %s%s\n",
               s->name,
               (unsigned long long)s->value,
               s->section,
               s->global ? "GLOBAL " : "",
               s->type == SYM_EXTERN ? "EXTERN" :
               s->type == SYM_EQU   ? "EQU"    : "LABEL");
    }
}
