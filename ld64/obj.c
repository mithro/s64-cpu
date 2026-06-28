#include "obj.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int obj_load(ObjFile *obj, const char *path) {
    memset(obj, 0, sizeof(ObjFile));
    strncpy(obj->path, path, sizeof(obj->path) - 1);

    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); return -1; }

    /* read SXF header */
    SXFHeader hdr;
    if (fread(&hdr, sizeof(hdr), 1, f) != 1) {
        fprintf(stderr, "ld64: truncated header in '%s'\n", path);
        fclose(f); return -1;
    }

    if (memcmp(hdr.magic, SXF_MAGIC, SXF_MAGIC_SIZE) != 0) {
        fprintf(stderr, "ld64: '%s' is not an SXF file\n", path);
        fclose(f); return -1;
    }

    if (hdr.endian == SXF_ENDIAN_BE) {
        fprintf(stderr, "ld64: S64-BEXT big-endian not yet implemented\n");
        fclose(f); return -1;
    }

    obj->mode  = hdr.mode;
    obj->entry = hdr.entry;

    /* read section descriptors + data */
    if (hdr.section_count > MAX_OBJ_SECTIONS) {
        fprintf(stderr, "ld64: too many sections in '%s'\n", path);
        fclose(f); return -1;
    }

    for (int i = 0; i < hdr.section_count; i++) {
        SXFSection sd;
        if (fread(&sd, sizeof(sd), 1, f) != 1) {
            fprintf(stderr, "ld64: truncated section %d in '%s'\n", i, path);
            fclose(f); return -1;
        }

        ObjSection *s = &obj->sections[obj->sect_count++];
        memcpy(s->name, sd.name, 8);
        s->name[8]    = 0;
        s->size       = (uint32_t)sd.file_size;
        s->flags      = sd.flags;
        s->virt_addr  = 0; /* assigned by linker */

        if (sd.file_size > 0) {
            s->data = malloc(sd.file_size);
            if (!s->data) {
                fprintf(stderr, "ld64: OOM\n");
                fclose(f); return -1;
            }
            long pos = ftell(f);
            fseek(f, (long)sd.file_offset, SEEK_SET);
            if (fread(s->data, 1, sd.file_size, f) != sd.file_size) {
                fprintf(stderr, "ld64: truncated section data\n");
                fclose(f); return -1;
            }
            fseek(f, pos, SEEK_SET);
        }
    }

    /* read symbol table (text format: name value section global) */
    if (hdr.symtab_offset > 0) {
        fseek(f, (long)hdr.symtab_offset, SEEK_SET);
        char line[256];
        while (fgets(line, sizeof(line), f) && obj->sym_count < MAX_OBJ_SYMBOLS) {
            ObjSymbol *sym = &obj->symbols[obj->sym_count];
            int sect, global;
            unsigned long long val;
            if (sscanf(line, "%127s %llx %d %d",
                       sym->name, &val, &sect, &global) == 4) {
                sym->value   = (uint64_t)val;
                sym->section = sect;
                sym->global  = global;
                sym->defined = 1;
                obj->sym_count++;
            }
        }
    }

    fclose(f);
    return 0;
}

void obj_free(ObjFile *obj) {
    for (int i = 0; i < obj->sect_count; i++)
        free(obj->sections[i].data);
    memset(obj, 0, sizeof(ObjFile));
}
