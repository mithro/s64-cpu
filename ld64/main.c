#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/s64.h"
#include "obj.h"
#include "resolve.h"
#include "reloc.h"
#include "emit.h"

static void usage(const char *prog) {
    fprintf(stderr,
        "usage: %s [options] file1.o64 [file2.o64 ...] -o output.x64\n"
        "options:\n"
        "  -o <file>        output file (default: out.x64)\n"
        "  --flat           emit flat binary\n"
        "  --base <addr>    load base address (default: 0x8000)\n"
        "  --entry <sym>    entry point symbol (default: _start)\n"
        "  --map            print linker map\n"
        "  --strip          strip symbols from output\n"
        "  --s32            32-bit output\n"
        "  --be             big-endian stub\n"
        "  -h               show this help\n",
        prog);
}

#define MAX_INPUTS 64

int main(int argc, char **argv) {
    if (argc < 2) { usage(argv[0]); return 1; }

    const char *inputs[MAX_INPUTS];
    int         input_count  = 0;
    const char *output       = "out.x64";
    const char *entry_sym    = "_start";
    uint64_t    base         = SXF_DEFAULT_BASE;
    int         opt_flat     = 0;
    int         opt_map      = 0;
    int         opt_strip    = 0;
    int         opt_s32      = 0;
    int         opt_be       = 0;

    for (int i = 1; i < argc; i++) {
        if      (!strcmp(argv[i], "-o") && i+1 < argc)
            output = argv[++i];
        else if (!strcmp(argv[i], "--flat"))   opt_flat  = 1;
        else if (!strcmp(argv[i], "--map"))    opt_map   = 1;
        else if (!strcmp(argv[i], "--strip"))  opt_strip = 1;
        else if (!strcmp(argv[i], "--s32"))    opt_s32   = 1;
        else if (!strcmp(argv[i], "--be"))     opt_be    = 1;
        else if (!strcmp(argv[i], "--base") && i+1 < argc)
            base = (uint64_t)strtoull(argv[++i], NULL, 0);
        else if (!strcmp(argv[i], "--entry") && i+1 < argc)
            entry_sym = argv[++i];
        else if (!strcmp(argv[i], "-h"))
            { usage(argv[0]); return 0; }
        else if (argv[i][0] != '-') {
            if (input_count >= MAX_INPUTS) {
                fprintf(stderr, "ld64: too many input files\n"); return 1;
            }
            inputs[input_count++] = argv[i];
        } else {
            fprintf(stderr, "ld64: unknown flag '%s'\n", argv[i]); return 1;
        }
    }

    if (input_count == 0) {
        fprintf(stderr, "ld64: no input files\n");
        usage(argv[0]); return 1;
    }

    if (opt_be) {
        fprintf(stderr, "ld64: S64-BEXT big-endian not yet implemented\n");
        return 1;
    }

    /* load all object files */
    ObjFile *objs = calloc(input_count, sizeof(ObjFile));
    if (!objs) { fprintf(stderr, "ld64: OOM\n"); return 1; }

    for (int i = 0; i < input_count; i++) {
        if (obj_load(&objs[i], inputs[i])) {
            fprintf(stderr, "ld64: failed to load '%s'\n", inputs[i]);
            free(objs); return 1;
        }
        printf("ld64: loaded '%s'  (%d sections, %d symbols)\n",
               inputs[i], objs[i].sect_count, objs[i].sym_count);
    }

    /* resolve symbols */
    SymMap sm;
    symmap_init(&sm);
    int errs = symmap_resolve(&sm, objs, input_count);
    if (errs) {
        fprintf(stderr, "ld64: %d unresolved symbol(s)\n", errs);
        free(objs); return 1;
    }

    /* patch relocations */
    errs = reloc_patch(objs, input_count, &sm);
    if (errs) {
        fprintf(stderr, "ld64: %d relocation error(s)\n", errs);
        free(objs); return 1;
    }

    /* print map if requested */
    if (opt_map) emit_map(objs, input_count, &sm);

    /* emit output */
    LinkOpts opts = {
        .base      = base,
        .entry_sym = entry_sym,
        .strip     = opt_strip,
        .flat      = opt_flat,
        .mode32    = opt_s32,
    };

    int rc;
    if (opt_flat)
        rc = emit_flat(objs, input_count, output, &opts);
    else
        rc = emit_x64(objs, input_count, &sm, output, &opts);

    /* cleanup */
    for (int i = 0; i < input_count; i++) obj_free(&objs[i]);
    free(objs);

    return rc ? 1 : 0;
}
