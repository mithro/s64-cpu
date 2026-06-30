#include "loader.h"
#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int loader_load(const char *path, S64CPU *cpu) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "loader: cannot open '%s'\n", path);
        return -1;
    }

    /* read header */
    SXFHeader hdr;
    if (fread(&hdr, sizeof(hdr), 1, f) != 1) {
        fprintf(stderr, "loader: truncated header\n");
        fclose(f); return -1;
    }

    /* check magic */
    if (memcmp(hdr.magic, SXF_MAGIC, SXF_MAGIC_SIZE) != 0) {
        fprintf(stderr, "loader: bad magic — not an SXF file\n");
        fclose(f); return -1;
    }

    /* check version */
    if (hdr.version != SXF_VERSION) {
        fprintf(stderr, "loader: unsupported SXF version %u\n", hdr.version);
        fclose(f); return -1;
    }

    /* endian check — big-endian stub */
    if (hdr.endian == SXF_ENDIAN_BE) {
        fprintf(stderr, "loader: S64-BEXT big-endian not yet implemented\n");
        fclose(f); return -1;
    }

    /* set execution mode */
    cpu->mode = (hdr.mode == SXF_MODE_32) ? S64_MODE_32BIT : S64_MODE_64BIT;

    /* load sections */
    SXFSection sect;
    for (u16 i = 0; i < hdr.section_count; i++) {
        if (fread(&sect, sizeof(sect), 1, f) != 1) {
            fprintf(stderr, "loader: truncated section descriptor %u\n", i);
            fclose(f); return -1;
        }

        if (sect.file_size == 0) {
            /* .bss — zero fill in memory (already zeroed by mem_init) */
            continue;
        }

        /* save position, seek to section data, read, restore */
        long pos = ftell(f);
        fseek(f, (long)sect.file_offset, SEEK_SET);

        u8 *tmp = malloc(sect.file_size);
        if (!tmp) {
            fprintf(stderr, "loader: out of memory\n");
            fclose(f); return -1;
        }

        if (fread(tmp, 1, sect.file_size, f) != sect.file_size) {
            fprintf(stderr, "loader: truncated section data\n");
            free(tmp); fclose(f); return -1;
        }

        mem_write_buf(sect.virt_addr, tmp, sect.file_size);
        free(tmp);
        fseek(f, pos, SEEK_SET);
    }

    /* set entry point */
    cpu->pc = hdr.entry;

    fclose(f);
    printf("loader: loaded '%s' entry=0x%016llx mode=%s\n",
           path, (unsigned long long)hdr.entry,
           cpu->mode == S64_MODE_64BIT ? "64-bit" : "32-bit");
    return 0;
}

/* load flat raw binary at base address */
int loader_load_flat(const char *path, S64CPU *cpu, u64 base) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "loader: cannot open '%s'\n", path);
        return -1;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    u8 *buf = malloc(sz);
    if (!buf) { fclose(f); return -1; }

    if (fread(buf, 1, sz, f) != (size_t)sz) {
        fprintf(stderr, "loader: read error\n");
        free(buf); return -1;
    }
    fclose(f);

    mem_write_buf(base, buf, sz);
    free(buf);

    cpu->pc = base;
    printf("loader: flat binary '%s' loaded at 0x%016llx (%ld bytes)\n",
           path, (unsigned long long)base, sz);
    return 0;
}
