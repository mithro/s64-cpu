#include "reloc.h"
#include "../include/s64.h"
#include <stdio.h>
#include <string.h>

int reloc_patch(ObjFile *objs, int obj_count, SymMap *sm) {
    int errors = 0;

    for (int oi = 0; oi < obj_count; oi++) {
        ObjFile *obj = &objs[oi];
        for (int ri = 0; ri < obj->reloc_count; ri++) {
            ObjReloc *rel = &obj->relocs[ri];

            LinkSym *sym = symmap_find(sm, rel->sym_name);
            if (!sym || !sym->defined) {
                fprintf(stderr, "ld64: reloc: undefined '%s'\n",
                        rel->sym_name);
                errors++;
                continue;
            }

            if (rel->section < 0 || rel->section >= obj->sect_count) {
                fprintf(stderr, "ld64: reloc: bad section index\n");
                errors++;
                continue;
            }

            ObjSection *sect = &obj->sections[rel->section];
            if (rel->offset + 4 > sect->size) {
                fprintf(stderr, "ld64: reloc: offset out of bounds\n");
                errors++;
                continue;
            }

            /* read the instruction word at the reloc offset */
            uint8_t *ptr = sect->data + rel->offset;
            uint32_t word = (uint32_t)ptr[0]
                          | (uint32_t)ptr[1] << 8
                          | (uint32_t)ptr[2] << 16
                          | (uint32_t)ptr[3] << 24;

            uint64_t target = sym->value;

            if (rel->type == SXF_RELOC_PCREL) {
                /* PC-relative: encode offset in instruction imm8 field */
                uint64_t pc = sect->virt_addr + rel->offset;
                int64_t  diff = (int64_t)(target - pc) / S64_INSTR_WIDTH;
                if (diff < -128 || diff > 127) {
                    fprintf(stderr,
                        "ld64: reloc: PC-relative offset %lld out of imm8 range"
                        " for '%s'\n", (long long)diff, rel->sym_name);
                    errors++;
                    continue;
                }
                /* patch imm8 field (bits 8:1) */
                word = (word & ~(0xFFU << 1)) | (((uint8_t)diff) << 1) | 1;
            } else {
                /* absolute — patch imm8 with low 8 bits of target */
                uint8_t imm = (uint8_t)(target & 0xFF);
                word = (word & ~(0xFFU << 1)) | ((uint32_t)imm << 1) | 1;
            }

            /* write back */
            ptr[0] = word & 0xFF;
            ptr[1] = (word >> 8)  & 0xFF;
            ptr[2] = (word >> 16) & 0xFF;
            ptr[3] = (word >> 24) & 0xFF;
        }
    }
    return errors;
}
