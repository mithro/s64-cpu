#ifndef LD64_OBJ_H
#define LD64_OBJ_H

#include "../include/s64.h"
#include <stdint.h>

#define MAX_OBJ_SECTIONS  8
#define MAX_OBJ_SYMBOLS   4096
#define MAX_OBJ_RELOCS    8192

typedef struct {
    char     name[32];
    uint8_t *data;
    uint32_t size;
    uint64_t virt_addr;   /* assigned by linker */
    uint8_t  flags;
} ObjSection;

typedef struct {
    char     name[128];
    uint64_t value;
    int      section;
    int      global;
    int      defined;
} ObjSymbol;

typedef struct {
    char     sym_name[128];
    uint64_t offset;       /* offset in section */
    int      section;
    uint8_t  type;         /* SXF_RELOC_ABS or SXF_RELOC_PCREL */
    int      field;
} ObjReloc;

typedef struct {
    char        path[256];
    ObjSection  sections[MAX_OBJ_SECTIONS];
    int         sect_count;
    ObjSymbol   symbols[MAX_OBJ_SYMBOLS];
    int         sym_count;
    ObjReloc    relocs[MAX_OBJ_RELOCS];
    int         reloc_count;
    uint8_t     mode;      /* SXF_MODE_64 or SXF_MODE_32 */
    uint64_t    entry;
} ObjFile;

int  obj_load(ObjFile *obj, const char *path);
void obj_free(ObjFile *obj);

#endif
