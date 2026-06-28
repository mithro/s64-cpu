#include "parser.h"
#include "../include/s64.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ─── section helpers ─────────────────────────────────────────── */
static Section *cur_section(Parser *p) {
    if (p->cur_sect < 0) return NULL;
    return &p->sections[p->cur_sect];
}

static int find_section(Parser *p, const char *name) {
    for (int i = 0; i < p->sect_count; i++)
        if (!strcmp(p->sections[i].name, name)) return i;
    return -1;
}

static int add_section(Parser *p, const char *name, uint8_t flags) {
    int idx = find_section(p, name);
    if (idx >= 0) return idx;
    if (p->sect_count >= MAX_SECTIONS) {
        fprintf(stderr, "as64: too many sections\n"); return -1;
    }
    Section *s = &p->sections[p->sect_count];
    strncpy(s->name, name, sizeof(s->name) - 1);
    s->flags    = flags;
    s->data     = malloc(SECT_BUF_SIZE);
    s->size     = 0;
    s->capacity = SECT_BUF_SIZE;
    s->base_addr = 0;
    if (!s->data) { fprintf(stderr, "as64: OOM\n"); return -1; }
    return p->sect_count++;
}

static void sect_emit8(Parser *p, uint8_t b) {
    Section *s = cur_section(p);
    if (!s) { fprintf(stderr, "as64: no section\n"); return; }
    if (s->size < s->capacity) s->data[s->size++] = b;
}

static void sect_emit32(Parser *p, uint32_t w) {
    sect_emit8(p, w & 0xFF);
    sect_emit8(p, (w >> 8)  & 0xFF);
    sect_emit8(p, (w >> 16) & 0xFF);
    sect_emit8(p, (w >> 24) & 0xFF);
}

static uint32_t sect_pc(Parser *p) {
    Section *s = cur_section(p);
    return s ? s->size : 0;
}

/* ─── parser init/free ────────────────────────────────────────── */
int parser_init(Parser *p, const char *src, size_t len, const char *filename) {
    memset(p, 0, sizeof(Parser));
    lexer_init(&p->lex, src, len, filename);
    symtab_init(&p->syms);
    p->filename = filename;
    p->cur_sect = -1;
    return 0;
}

void parser_free(Parser *p) {
    for (int i = 0; i < p->sect_count; i++)
        free(p->sections[i].data);
}

/* ─── error helper ────────────────────────────────────────────── */
#define PERR(p, tok, fmt, ...) do { \
    fprintf(stderr, "%s:%d: error: " fmt "\n", \
            (p)->filename, (tok).line, ##__VA_ARGS__); \
    (p)->errors++; \
} while(0)

/* ─── skip to next line ───────────────────────────────────────── */
static void skip_line(Parser *p) {
    Token t;
    do { t = lexer_next(&p->lex); }
    while (t.type != TOK_NEWLINE && t.type != TOK_EOF);
}

/* ─── expect helpers ──────────────────────────────────────────── */
static Token expect(Parser *p, TokenType type) {
    Token t = lexer_next(&p->lex);
    if (t.type != type) {
        PERR(p, t, "expected %s, got '%s'",
             tok_type_name(type), t.text);
    }
    return t;
}

static int maybe_comma(Parser *p) {
    Token t = lexer_peek(&p->lex);
    if (t.type == TOK_COMMA) { lexer_next(&p->lex); return 1; }
    return 0;
}

/* ─── parse one operand ───────────────────────────────────────── */
static int parse_operand(Parser *p, Operand *op) {
    Token t = lexer_peek(&p->lex);

    /* register */
    if (t.type == TOK_REG) {
        lexer_next(&p->lex);
        op->type = OPT_REG;
        op->reg  = t.reg;
        return 1;
    }

    /* immediate: #imm or just a number */
    if (t.type == TOK_HASH) {
        lexer_next(&p->lex);
        Token v = lexer_next(&p->lex);
        if (v.type != TOK_INT) {
            PERR(p, v, "expected integer after '#'"); return 0;
        }
        op->type = OPT_IMM;
        op->imm  = v.ival;
        return 1;
    }
    if (t.type == TOK_INT) {
        lexer_next(&p->lex);
        op->type = OPT_IMM;
        op->imm  = t.ival;
        return 1;
    }

    /* memory: [Rs + #off] or [Rs] */
    if (t.type == TOK_LBRACKET) {
        lexer_next(&p->lex);
        Token base = expect(p, TOK_REG);
        op->type    = OPT_MEM_REG_IMM;
        op->mem_reg = base.reg;
        op->mem_off = 0;

        Token next = lexer_peek(&p->lex);
        if (next.type == TOK_HASH || next.type == TOK_INT) {
            /* [Rs + #off] — treat '+' as optional, just read the imm */
            if (next.type == TOK_HASH) lexer_next(&p->lex);
            Token off = lexer_next(&p->lex);
            if (off.type == TOK_INT) op->mem_off = off.ival;
        }
        expect(p, TOK_RBRACKET);
        return 1;
    }

    /* label reference */
    if (t.type == TOK_IDENT) {
        lexer_next(&p->lex);
        op->type = OPT_LABEL;
        strncpy(op->label, t.text, sizeof(op->label) - 1);
        return 1;
    }

    return 0;
}

/* ─── handle directive ────────────────────────────────────────── */
static void handle_directive(Parser *p, Token dir_tok) {
    const char *d = dir_tok.text; /* e.g. ".section" */

    if (!strcmp(d, ".section")) {
        Token name = lexer_next(&p->lex);
        const char *sname = name.text;
        uint8_t flags = SXF_SECT_FLAG_R;
        if      (!strcmp(sname, ".text"))   flags = SXF_SECT_FLAG_R | SXF_SECT_FLAG_X;
        else if (!strcmp(sname, ".data"))   flags = SXF_SECT_FLAG_R | SXF_SECT_FLAG_W;
        else if (!strcmp(sname, ".rodata")) flags = SXF_SECT_FLAG_R;
        else if (!strcmp(sname, ".bss"))    flags = SXF_SECT_FLAG_R | SXF_SECT_FLAG_W;
        int idx = add_section(p, sname, flags);
        if (idx >= 0) p->cur_sect = idx;

    } else if (!strcmp(d, ".text")) {
        int idx = add_section(p, ".text", SXF_SECT_FLAG_R | SXF_SECT_FLAG_X);
        if (idx >= 0) p->cur_sect = idx;

    } else if (!strcmp(d, ".data")) {
        int idx = add_section(p, ".data", SXF_SECT_FLAG_R | SXF_SECT_FLAG_W);
        if (idx >= 0) p->cur_sect = idx;

    } else if (!strcmp(d, ".rodata")) {
        int idx = add_section(p, ".rodata", SXF_SECT_FLAG_R);
        if (idx >= 0) p->cur_sect = idx;

    } else if (!strcmp(d, ".bss")) {
        int idx = add_section(p, ".bss", SXF_SECT_FLAG_R | SXF_SECT_FLAG_W);
        if (idx >= 0) p->cur_sect = idx;

    } else if (!strcmp(d, ".global")) {
        Token name = lexer_next(&p->lex);
        symtab_add_global(&p->syms, name.text);

    } else if (!strcmp(d, ".extern")) {
        Token name = lexer_next(&p->lex);
        symtab_add_extern(&p->syms, name.text);

    } else if (!strcmp(d, ".equ")) {
        Token name = lexer_next(&p->lex);
        maybe_comma(p);
        Token val  = lexer_next(&p->lex);
        if (val.type == TOK_INT)
            symtab_define(&p->syms, name.text, SYM_EQU,
                          (uint64_t)val.ival, p->cur_sect);
        else PERR(p, val, ".equ: expected integer");

    } else if (!strcmp(d, ".db")) {
        Token v = lexer_next(&p->lex);
        while (v.type == TOK_INT) {
            sect_emit8(p, (uint8_t)v.ival);
            if (!maybe_comma(p)) break;
            v = lexer_next(&p->lex);
        }

    } else if (!strcmp(d, ".dw")) {
        Token v = lexer_next(&p->lex);
        if (v.type == TOK_INT) {
            sect_emit8(p, (uint8_t)(v.ival));
            sect_emit8(p, (uint8_t)(v.ival >> 8));
        }

    } else if (!strcmp(d, ".dd")) {
        Token v = lexer_next(&p->lex);
        if (v.type == TOK_INT) {
            uint32_t x = (uint32_t)v.ival;
            sect_emit8(p, x & 0xFF); sect_emit8(p, (x>>8)&0xFF);
            sect_emit8(p, (x>>16)&0xFF); sect_emit8(p, (x>>24)&0xFF);
        }

    } else if (!strcmp(d, ".dq")) {
        Token v = lexer_next(&p->lex);
        if (v.type == TOK_INT) {
            uint64_t x = (uint64_t)v.ival;
            for (int i = 0; i < 8; i++) sect_emit8(p, (x >> (i*8)) & 0xFF);
        }

    } else if (!strcmp(d, ".str")) {
        Token v = lexer_next(&p->lex);
        if (v.type == TOK_STRING) {
            for (size_t i = 0; v.text[i]; i++) sect_emit8(p, (uint8_t)v.text[i]);
            sect_emit8(p, 0); /* null terminator */
        } else PERR(p, v, ".str: expected string");

    } else if (!strcmp(d, ".strn")) {
        Token v = lexer_next(&p->lex);
        if (v.type == TOK_STRING)
            for (size_t i = 0; v.text[i]; i++) sect_emit8(p, (uint8_t)v.text[i]);
        else PERR(p, v, ".strn: expected string");

    } else if (!strcmp(d, ".align")) {
        Token v = lexer_next(&p->lex);
        if (v.type == TOK_INT) {
            uint32_t align = (uint32_t)v.ival;
            while (sect_pc(p) % align) sect_emit8(p, 0);
        }

    } else if (!strcmp(d, ".res")) {
        Token v = lexer_next(&p->lex);
        if (v.type == TOK_INT)
            for (int64_t i = 0; i < v.ival; i++) sect_emit8(p, 0);

    } else {
        PERR(p, dir_tok, "unknown directive '%s'", d);
    }
    skip_line(p);
}

/* ─── parse one instruction line ──────────────────────────────── */
static void handle_instruction(Parser *p, Token mnem_tok) {
    Instruction ins;
    memset(&ins, 0, sizeof(ins));
    strncpy(ins.mnemonic, mnem_tok.text, sizeof(ins.mnemonic) - 1);
    ins.line = mnem_tok.line;

    /* parse up to 4 operands */
    Token peek = lexer_peek(&p->lex);
    while (peek.type != TOK_NEWLINE && peek.type != TOK_EOF
           && ins.op_count < 4) {
        if (ins.op_count > 0) {
            if (peek.type != TOK_COMMA) break;
            lexer_next(&p->lex); /* consume comma */
        }
        if (!parse_operand(p, &ins.ops[ins.op_count])) break;
        ins.op_count++;
        peek = lexer_peek(&p->lex);
    }
    skip_line(p);

    /* resolve label operands to immediates if possible */
    for (int i = 0; i < ins.op_count; i++) {
        if (ins.ops[i].type == OPT_LABEL) {
            Symbol *s = symtab_find(&p->syms, ins.ops[i].label);
            if (s && s->defined) {
                ins.ops[i].type = OPT_IMM;
                ins.ops[i].imm  = (int64_t)s->value;
            } else {
                /* needs relocation — emit 0 placeholder */
                symtab_add_reloc(&p->syms, ins.ops[i].label,
                                 sect_pc(p), p->cur_sect,
                                 RELOC_ABS, i);
                ins.ops[i].type = OPT_IMM;
                ins.ops[i].imm  = 0;
            }
        }
    }

    /* encode */
    uint32_t word = encoder_encode(&ins, p->filename);
    if (word == 0xFFFFFFFF) { p->errors++; return; }
    sect_emit32(p, word);
}

/* ─── main parse loop ─────────────────────────────────────────── */
int parser_run(Parser *p) {
    /* Pass 1 + 2 combined (single pass with forward ref patching) */
    /* default to .text section */
    p->cur_sect = add_section(p, ".text",
                               SXF_SECT_FLAG_R | SXF_SECT_FLAG_X);

    for (;;) {
        Token t = lexer_next(&p->lex);

        if (t.type == TOK_EOF) break;
        if (t.type == TOK_NEWLINE) continue;

        if (t.type == TOK_DIRECTIVE) {
            handle_directive(p, t);
            continue;
        }

        if (t.type == TOK_IDENT) {
            /* could be label: or instruction */
            Token next = lexer_peek(&p->lex);
            if (next.type == TOK_COLON) {
                /* label definition */
                lexer_next(&p->lex); /* consume ':' */
                uint64_t addr = (uint64_t)sect_pc(p);
                symtab_define(&p->syms, t.text, SYM_LABEL,
                              addr, p->cur_sect);
            } else {
                /* instruction mnemonic */
                handle_instruction(p, t);
            }
            continue;
        }

        PERR(p, t, "unexpected token '%s'", t.text);
        skip_line(p);
    }

    /* resolve forward references */
    int unres = symtab_resolve(&p->syms);
    if (unres > 0) p->errors += unres;

    return p->errors;
}
