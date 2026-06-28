#ifndef LD64_EMIT_H
#define LD64_EMIT_H

#include "obj.h"
#include "resolve.h"

typedef struct {
    uint64_t base;          /* load base address */
    const char *entry_sym;  /* entry point symbol name */
    int strip;              /* strip symbols */
    int flat;               /* flat binary output */
    int mode32;
} LinkOpts;

int emit_x64(ObjFile *objs, int obj_count, SymMap *sm,
             const char *outpath, LinkOpts *opts);
int emit_flat(ObjFile *objs, int obj_count,
              const char *outpath, LinkOpts *opts);
void emit_map(ObjFile *objs, int obj_count, SymMap *sm);

#endif
