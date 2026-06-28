#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/s64.h"
#include "lexer.h"
#include "parser.h"
#include "symbols.h"
#include "encoder.h"

/* ─── usage ───────────────────────────────────────────────────── */
static void usage(const char *prog) {
    fprintf(stderr,
        "usage: %s [options] input.s64 -o output.o64\n"
        "options:\n"
        "  -o <file>    output file (default: out.o64)\n"
        "  --flat       emit raw flat binary instead of .o64\n"
        "  --s32        assemble in 32-bit mode\n"
        "  --be         big-endian (stub — not yet implemented)\n"
        "  --list       emit listing to stdout\n"
        "  --syms       dump symbol table after assembly\n"
        "  -g           embed debug info (stub)\n"
        "  -h           show this help\n",
        prog);
}

/* ─── read whole file into buffer ─────────────────────────────── */
static char *read_file(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); return NULL; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(sz + 1);
    if (!buf) { fclose(f); return NULL; }
    (void)fread(buf, 1, sz, f);
    buf[sz] = 0;
    fclose(f);
    *out_len = (size_t)sz;
    return buf;
}

/* ─── emit listing ────────────────────────────────────────────── */
static void emit_listing(Parser *p) {
    printf("\n=== S64 assembler listing ===\n");
    for (int i = 0; i < p->sect_count; i++) {
        Section *s = &p->sections[i];
        printf("\nsection %-12s  %u bytes  flags=%02x\n",
               s->name, s->size, s->flags);
        for (uint32_t off = 0; off < s->size; off += 4) {
            uint32_t word = (uint32_t)s->data[off]
                          | (uint32_t)s->data[off+1] << 8
                          | (uint32_t)s->data[off+2] << 16
                          | (uint32_t)s->data[off+3] << 24;
            char dbuf[64];
            extern int disasm_instr(uint32_t, uint64_t, char*, size_t);
            disasm_instr(word, (uint64_t)off, dbuf, sizeof(dbuf));
            printf("  %08x:  %08x  %s\n", off, word, dbuf);
        }
    }
    printf("\n");
}

/* ─── write SXF .o64 object file ──────────────────────────────── */
static int write_o64(Parser *p, const char *path,
                     int mode32, const char *src_path) {
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return -1; }

    /* count non-empty sections */
    uint16_t sect_count = (uint16_t)p->sect_count;

    /* calculate section data offsets —
       header + section descriptors, then section data */
    uint64_t data_start = sizeof(SXFHeader)
                        + (uint64_t)sect_count * sizeof(SXFSection);

    /* build header */
    SXFHeader hdr;
    memset(&hdr, 0, sizeof(hdr));
    memcpy(hdr.magic, SXF_MAGIC, SXF_MAGIC_SIZE);
    hdr.version       = SXF_VERSION;
    hdr.mode          = mode32 ? SXF_MODE_32 : SXF_MODE_64;
    hdr.endian        = SXF_ENDIAN_LE;
    hdr.section_count = sect_count;

    /* entry = value of _start symbol if defined, else 0 */
    Symbol *entry_sym = symtab_find(&p->syms, "_start");
    hdr.entry = entry_sym ? entry_sym->value : 0;

    /* symbol table goes after section data */
    uint64_t sect_data_total = 0;
    for (int i = 0; i < p->sect_count; i++)
        sect_data_total += p->sections[i].size;
    hdr.symtab_offset = data_start + sect_data_total;

    fwrite(&hdr, sizeof(hdr), 1, f);

    /* write section descriptors */
    uint64_t cur_offset = data_start;
    uint64_t cur_vaddr  = SXF_DEFAULT_BASE;

    for (int i = 0; i < p->sect_count; i++) {
        Section *s = &p->sections[i];
        SXFSection sd;
        memset(&sd, 0, sizeof(sd));
        memcpy(sd.name, s->name, 7); sd.name[7]=0;
        sd.file_offset = cur_offset;
        sd.file_size   = s->size;
        sd.virt_addr   = cur_vaddr;
        sd.mem_size    = s->size;
        sd.flags       = s->flags;
        fwrite(&sd, sizeof(sd), 1, f);
        cur_offset += s->size;
        cur_vaddr  += s->size;
        /* align vaddr to 4 bytes */
        while (cur_vaddr % 4) cur_vaddr++;
    }

    /* write section data */
    for (int i = 0; i < p->sect_count; i++)
        fwrite(p->sections[i].data, 1, p->sections[i].size, f);

    /* simple symbol table — name:value pairs (text format) */
    for (int i = 0; i < p->syms.sym_count; i++) {
        Symbol *sym = &p->syms.syms[i];
        fprintf(f, "%s 0x%llx %d %d\n",
                sym->name,
                (unsigned long long)sym->value,
                sym->section,
                sym->global);
    }

    fclose(f);
    printf("as64: wrote '%s'  (%d section(s))\n", path, p->sect_count);
    (void)src_path;
    return 0;
}

/* ─── write flat binary ───────────────────────────────────────── */
static int write_flat(Parser *p, const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return -1; }

    uint32_t total = 0;
    for (int i = 0; i < p->sect_count; i++) {
        /* only emit executable/data sections, skip bss */
        Section *s = &p->sections[i];
        if (strcmp(s->name, ".bss") == 0) continue;
        fwrite(s->data, 1, s->size, f);
        total += s->size;
    }

    fclose(f);
    printf("as64: wrote flat binary '%s'  (%u bytes)\n", path, total);
    return 0;
}

/* ─── main ────────────────────────────────────────────────────── */
int main(int argc, char **argv) {
    if (argc < 2) { usage(argv[0]); return 1; }

    const char *input   = NULL;
    const char *output  = "out.o64";
    int opt_flat        = 0;
    int opt_s32         = 0;
    int opt_be          = 0;
    int opt_list        = 0;
    int opt_syms        = 0;
    int opt_debug       = 0;

    for (int i = 1; i < argc; i++) {
        if      (!strcmp(argv[i], "-o") && i+1 < argc) output = argv[++i];
        else if (!strcmp(argv[i], "--flat"))  opt_flat  = 1;
        else if (!strcmp(argv[i], "--s32"))   opt_s32   = 1;
        else if (!strcmp(argv[i], "--be"))    opt_be    = 1;
        else if (!strcmp(argv[i], "--list"))  opt_list  = 1;
        else if (!strcmp(argv[i], "--syms"))  opt_syms  = 1;
        else if (!strcmp(argv[i], "-g"))      opt_debug = 1;
        else if (!strcmp(argv[i], "-h"))    { usage(argv[0]); return 0; }
        else if (argv[i][0] != '-')           input = argv[i];
        else { fprintf(stderr, "as64: unknown flag '%s'\n", argv[i]); return 1; }
    }

    if (!input) { fprintf(stderr, "as64: no input file\n"); usage(argv[0]); return 1; }

    if (opt_be) {
        fprintf(stderr, "as64: S64-BEXT big-endian not yet implemented\n");
        return 1;
    }

    /* read source */
    size_t src_len;
    char *src = read_file(input, &src_len);
    if (!src) return 1;

    /* assemble */
    Parser parser;
    parser_init(&parser, src, src_len, input);
    parser.debug = opt_debug;

    int errors = parser_run(&parser);

    if (opt_list)  emit_listing(&parser);
    if (opt_syms)  symtab_dump(&parser.syms);

    if (errors) {
        fprintf(stderr, "as64: %d error(s) — no output written\n", errors);
        parser_free(&parser);
        free(src);
        return 1;
    }

    /* write output */
    int rc;
    if (opt_flat)
        rc = write_flat(&parser, output);
    else
        rc = write_o64(&parser, output, opt_s32, input);

    parser_free(&parser);
    free(src);
    return rc ? 1 : 0;
}
