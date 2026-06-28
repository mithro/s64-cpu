#ifndef LD64_RESOLVE_H
#define LD64_RESOLVE_H

#include "obj.h"

#define MAX_LINK_SYMS  16384

typedef struct {
    char     name[128];
    uint64_t value;
    int      defined;
    int      obj_idx;
    int      sect_idx;
} LinkSym;

typedef struct {
    LinkSym  syms[MAX_LINK_SYMS];
    int      count;
} SymMap;

void symmap_init(SymMap *sm);
int  symmap_add(SymMap *sm, const char *name, uint64_t value,
                int defined, int obj_idx, int sect_idx);
LinkSym *symmap_find(SymMap *sm, const char *name);
int  symmap_resolve(SymMap *sm, ObjFile *objs, int obj_count); /* returns errors */

#endif
