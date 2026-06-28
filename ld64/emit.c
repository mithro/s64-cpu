#include "emit.h"
#include "../include/s64.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ─── section merge order ─────────────────────────────────────── */
static const char *sect_order[] = {
    ".text", ".rodata", ".data", ".bss", NULL
};

typedef struct {
    const char *name;
    uint8_t    *data;
    uint32_t    size;
    uint64_t    virt_addr;
    uint8_t     flags;
} MergedSection;

#define MAX_MERGED 16

static int build_merged(ObjFile *objs, int obj_count,
                        MergedSection *out, int *out_count,
                        uint64_t base) {
    *out_count = 0;
    uint64_t cur_addr = base;

    for (int si = 0; sect_order[si]; si++) {
        const char *want = sect_order[si];
        uint32_t total = 0;

        /* measure total size for this section name */
        for (int oi = 0; oi < obj_count; oi++) {
            ObjFile *obj = &objs[oi];
            for (int i = 0; i < obj->sect_count; i++) {
                if (!strcmp(obj->sections[i].name, want))
                    total += obj->sections[i].size;
            }
        }
        if (total == 0) continue;

        if (*out_count >= MAX_MERGED) {
            fprintf(stderr, "ld64: too many sections\n");
            return -1;
        }

        MergedSection *ms = &out[(*out_count)++];
        ms->name      = want;
        ms->size      = total;
        ms->virt_addr = cur_addr;
        ms->flags     = 0;
        ms->data      = calloc(1, total);
        if (!ms->data) { fprintf(stderr, "ld64: OOM\n"); return -1; }

        /* copy section data from each object, record virt_addrs */
        uint32_t off = 0;
        for (int oi = 0; oi < obj_count; oi++) {
            ObjFile *obj = &objs[oi];
            for (int i = 0; i < obj->sect_count; i++) {
                ObjSection *s = &obj->sections[i];
                if (strcmp(s->name, want)) continue;
                ms->flags = s->flags;
                s->virt_addr = cur_addr + off;
                if (s->data)
                    memcpy(ms->data + off, s->data, s->size);
                off += s->size;
            }
        }

        cur_addr += total;
        /* align to 8 bytes */
        while (cur_addr % 8) cur_addr++;
    }
    return 0;
}

/* ─── patch symbol values with final virt addrs ───────────────── */
static void patch_sym_addrs(ObjFile *objs, int obj_count, SymMap *sm) {
    for (int i = 0; i < sm->count; i++) {
        LinkSym *ls = &sm->syms[i];
        if (!ls->defined || ls->obj_idx < 0) continue;
        ObjFile *obj = &objs[ls->obj_idx];
        if (ls->sect_idx < 0 || ls->sect_idx >= obj->sect_count) continue;
        ObjSection *sect = &obj->sections[ls->sect_idx];
        ls->value = sect->virt_addr + ls->value;
    }
}

/* ─── emit .x64 SXF ──────────────────────────────────────────── */
int emit_x64(ObjFile *objs, int obj_count, SymMap *sm,
             const char *outpath, LinkOpts *opts) {
    MergedSection merged[MAX_MERGED];
    int mc = 0;
    if (build_merged(objs, obj_count, merged, &mc, opts->base)) return -1;
    patch_sym_addrs(objs, obj_count, sm);

    /* find entry point */
    const char *esym = opts->entry_sym ? opts->entry_sym : "_start";
    LinkSym *entry = symmap_find(sm, esym);
    uint64_t entry_addr = entry ? entry->value : opts->base;

    FILE *f = fopen(outpath, "wb");
    if (!f) { perror(outpath); return -1; }

    /* header */
    SXFHeader hdr;
    memset(&hdr, 0, sizeof(hdr));
    memcpy(hdr.magic, SXF_MAGIC, SXF_MAGIC_SIZE);
    hdr.version       = SXF_VERSION;
    hdr.mode          = opts->mode32 ? SXF_MODE_32 : SXF_MODE_64;
    hdr.endian        = SXF_ENDIAN_LE;
    hdr.entry         = entry_addr;
    hdr.section_count = (uint16_t)mc;

    uint64_t data_start = sizeof(SXFHeader)
                        + (uint64_t)mc * sizeof(SXFSection);
    uint64_t data_total = 0;
    for (int i = 0; i < mc; i++) data_total += merged[i].size;
    hdr.symtab_offset = opts->strip ? 0 : data_start + data_total;

    fwrite(&hdr, sizeof(hdr), 1, f);

    /* section descriptors */
    uint64_t cur_off = data_start;
    for (int i = 0; i < mc; i++) {
        MergedSection *ms = &merged[i];
        SXFSection sd;
        memset(&sd, 0, sizeof(sd));
        memcpy(sd.name, ms->name, strnlen(ms->name, 7));
        sd.file_offset = cur_off;
        sd.file_size   = ms->size;
        sd.virt_addr   = ms->virt_addr;
        sd.mem_size    = ms->size;
        sd.flags       = ms->flags;
        fwrite(&sd, sizeof(sd), 1, f);
        cur_off += ms->size;
    }

    /* section data */
    for (int i = 0; i < mc; i++)
        fwrite(merged[i].data, 1, merged[i].size, f);

    /* symbol table */
    if (!opts->strip) {
        for (int i = 0; i < sm->count; i++) {
            LinkSym *s = &sm->syms[i];
            fprintf(f, "%s 0x%llx %d %d\n",
                    s->name, (unsigned long long)s->value,
                    s->obj_idx, s->defined);
        }
    }

    fclose(f);

    /* free merged */
    for (int i = 0; i < mc; i++) free(merged[i].data);

    printf("ld64: linked '%s'  entry=0x%llx  %d section(s)\n",
           outpath, (unsigned long long)entry_addr, mc);
    return 0;
}

/* ─── emit flat binary ────────────────────────────────────────── */
int emit_flat(ObjFile *objs, int obj_count,
              const char *outpath, LinkOpts *opts) {
    MergedSection merged[MAX_MERGED];
    int mc = 0;
    if (build_merged(objs, obj_count, merged, &mc, opts->base)) return -1;

    FILE *f = fopen(outpath, "wb");
    if (!f) { perror(outpath); return -1; }

    uint32_t total = 0;
    for (int i = 0; i < mc; i++) {
        if (strcmp(merged[i].name, ".bss") == 0) continue;
        fwrite(merged[i].data, 1, merged[i].size, f);
        total += merged[i].size;
    }
    fclose(f);

    for (int i = 0; i < mc; i++) free(merged[i].data);

    printf("ld64: flat binary '%s'  %u bytes\n", outpath, total);
    return 0;
}

/* ─── print linker map ────────────────────────────────────────── */
void emit_map(ObjFile *objs, int obj_count, SymMap *sm) {
    printf("\n=== S64 linker map ===\n");
    for (int oi = 0; oi < obj_count; oi++) {
        ObjFile *obj = &objs[oi];
        printf("\n%s\n", obj->path);
        for (int i = 0; i < obj->sect_count; i++) {
            ObjSection *s = &obj->sections[i];
            printf("  %-12s  vaddr=0x%016llx  size=%u\n",
                   s->name,
                   (unsigned long long)s->virt_addr,
                   s->size);
        }
    }
    printf("\nsymbols:\n");
    for (int i = 0; i < sm->count; i++) {
        LinkSym *s = &sm->syms[i];
        printf("  0x%016llx  %s\n",
               (unsigned long long)s->value, s->name);
    }
    printf("\n");
}
