#ifndef AS64_LEXER_H
#define AS64_LEXER_H

#include <stdint.h>
#include <stddef.h>

typedef enum {
    TOK_EOF = 0,
    TOK_NEWLINE,
    TOK_COMMA,
    TOK_COLON,
    TOK_LBRACKET,
    TOK_RBRACKET,
    TOK_HASH,

    TOK_IDENT,      /* instruction mnemonics, labels, directive names */
    TOK_REG,        /* R0–R31 / W0–W31 / SP / FP / LR / ZR */
    TOK_INT,        /* 0x1A, 42, -5 */
    TOK_STRING,     /* "hello" */
    TOK_DIRECTIVE,  /* .section .global .text .data etc */

    TOK_UNKNOWN,
} TokenType;

typedef struct {
    TokenType   type;
    char        text[256];   /* raw text */
    int64_t     ival;        /* for TOK_INT */
    int         reg;         /* for TOK_REG: 0–31 */
    int         line;
    int         col;
} Token;

typedef struct {
    const char *src;
    size_t      len;
    size_t      pos;
    int         line;
    int         col;
    const char *filename;
} Lexer;

void  lexer_init(Lexer *l, const char *src, size_t len, const char *filename);
Token lexer_next(Lexer *l);
Token lexer_peek(Lexer *l);
const char *tok_type_name(TokenType t);

#endif
