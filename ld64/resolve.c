#include "resolve.h"
#include <stdio.h>
#include <string.h>

void symmap_init(SymMap *sm) {
    memset(sm, 0, sizeof(SymMap));
}

int symmap_add(SymMap *sm, const char *name, uint64_t value,
               int defined, int obj_idx, int sect_idx) {
    /* check for duplicate definition */
    LinkSym *existing = symmap_find(sm, name);
    if (existing) {
        if (existing->defined && defined) {
            fprintf(stderr, "ld64: duplicate symbol '%s'\n", name);
            return -1;
        }
        if (defined) {
            existing->value    = value;
            existing->defined  = 1;
            existing->obj_idx  = obj_idx;
            existing->sect_idx = sect_idx;
        }
        return 0;
    }

    if (sm->count >= MAX_LINK_SYMS) {
        fprintf(stderr, "ld64: symbol table full\n");
        return -1;
    }

    LinkSym *s = &sm->syms[sm->count++];
    strncpy(s->name, name, sizeof(s->name) - 1);
    s->value    = value;
    s->defined  = defined;
    s->obj_idx  = obj_idx;
    s->sect_idx = sect_idx;
    return 0;
}

LinkSym *symmap_find(SymMap *sm, const char *name) {
    for (int i = 0; i < sm->count; i++)
        if (!strcmp(sm->syms[i].name, name))
            return &sm->syms[i];
    return NULL;
}

int symmap_resolve(SymMap *sm, ObjFile *objs, int obj_count) {
    /* collect all symbols from all objects */
    for (int oi = 0; oi < obj_count; oi++) {
        ObjFile *obj = &objs[oi];
        for (int si = 0; si < obj->sym_count; si++) {
            ObjSymbol *sym = &obj->symbols[si];
            symmap_add(sm, sym->name, sym->value,
                       sym->defined, oi, sym->section);
        }
    }

    /* check for undefined symbols */
    int errors = 0;
    for (int i = 0; i < sm->count; i++) {
        if (!sm->syms[i].defined) {
            fprintf(stderr, "ld64: undefined symbol '%s'\n",
                    sm->syms[i].name);
            errors++;
        }
    }
    return errors;
}
