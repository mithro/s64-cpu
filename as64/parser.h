#ifndef AS64_PARSER_H
#define AS64_PARSER_H

#include "lexer.h"
#include "symbols.h"
#include "encoder.h"
#include <stdint.h>

#define MAX_SECTIONS    8
#define SECT_BUF_SIZE   (4 * 1024 * 1024)   /* 4MB per section */

typedef struct {
    char     name[32];
    uint8_t *data;
    uint32_t size;
    uint32_t capacity;
    uint8_t  flags;         /* SXF_SECT_FLAG_R/W/X */
    uint64_t base_addr;
} Section;

typedef struct {
    Lexer    lex;
    SymTable syms;
    Section  sections[MAX_SECTIONS];
    int      sect_count;
    int      cur_sect;
    int      errors;
    int      debug;         /* emit debug info */
    const char *filename;
} Parser;

int  parser_init(Parser *p, const char *src, size_t len, const char *filename);
void parser_free(Parser *p);
int  parser_run(Parser *p);   /* pass 1+2 — returns error count */

#endif
