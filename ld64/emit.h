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

#define MAX_MERGED 16

typedef struct {
    const char *name;
    uint8_t    *data;
    uint32_t    size;
    uint64_t    virt_addr;
    uint8_t     flags;
} MergedSection;

typedef struct {
    MergedSection sections[MAX_MERGED];
    int count;
} Layout;

/* Computes final section addresses (build_merged) and patches symbol
 * values (patch_sym_addrs) exactly once. Must be called before emit_map,
 * emit_x64, or emit_flat so the map reflects real, final addresses
 * instead of pre-link zeros. Returns -1 on error. */
int emit_compute_layout(ObjFile *objs, int obj_count, SymMap *sm,
                         uint64_t base, Layout *out);
void emit_resync_layout(ObjFile *objs, int obj_count, Layout *layout);

int emit_x64(ObjFile *objs, int obj_count, SymMap *sm,
             const char *outpath, LinkOpts *opts, Layout *layout);
int emit_flat(ObjFile *objs, int obj_count,
              const char *outpath, LinkOpts *opts, Layout *layout);
void emit_map(ObjFile *objs, int obj_count, SymMap *sm, Layout *layout);

#endif
