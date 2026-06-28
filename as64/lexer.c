#include "lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

void lexer_init(Lexer *l, const char *src, size_t len, const char *filename) {
    l->src      = src;
    l->len      = len;
    l->pos      = 0;
    l->line     = 1;
    l->col      = 1;
    l->filename = filename;
}

static char peek_c(Lexer *l) {
    if (l->pos >= l->len) return 0;
    return l->src[l->pos];
}

static char next_c(Lexer *l) {
    if (l->pos >= l->len) return 0;
    char c = l->src[l->pos++];
    if (c == '\n') { l->line++; l->col = 1; }
    else           { l->col++; }
    return c;
}

static void skip_whitespace(Lexer *l) {
    while (l->pos < l->len) {
        char c = peek_c(l);
        if (c == ' ' || c == '\t' || c == '\r') { next_c(l); }
        else if (c == ';') {
            /* comment — skip to end of line */
            while (l->pos < l->len && peek_c(l) != '\n') next_c(l);
        } else break;
    }
}

/* register lookup — returns 0–31 or -1 */
static int parse_reg(const char *s) {
    /* R0–R31 */
    if ((s[0] == 'R' || s[0] == 'r') && isdigit((unsigned char)s[1])) {
        int n = atoi(s + 1);
        if (n >= 0 && n <= 31) return n;
    }
    /* W0–W31 (32-bit aliases — same physical reg) */
    if ((s[0] == 'W' || s[0] == 'w') && isdigit((unsigned char)s[1])) {
        int n = atoi(s + 1);
        if (n >= 0 && n <= 31) return n;
    }
    /* SABI aliases */
    if (!strcasecmp(s, "SP"))  return 28;
    if (!strcasecmp(s, "FP"))  return 29;
    if (!strcasecmp(s, "LR"))  return 30;
    if (!strcasecmp(s, "ZR"))  return 31;
    return -1;
}

Token lexer_next(Lexer *l) {
    skip_whitespace(l);

    Token tok;
    memset(&tok, 0, sizeof(tok));
    tok.line = l->line;
    tok.col  = l->col;

    if (l->pos >= l->len) { tok.type = TOK_EOF; return tok; }

    char c = peek_c(l);

    /* newline */
    if (c == '\n') {
        next_c(l);
        tok.type = TOK_NEWLINE;
        strcpy(tok.text, "\\n");
        return tok;
    }

    /* single-char tokens */
    switch (c) {
    case ',': next_c(l); tok.type = TOK_COMMA;    strcpy(tok.text, ","); return tok;
    case ':': next_c(l); tok.type = TOK_COLON;    strcpy(tok.text, ":"); return tok;
    case '[': next_c(l); tok.type = TOK_LBRACKET; strcpy(tok.text, "["); return tok;
    case ']': next_c(l); tok.type = TOK_RBRACKET; strcpy(tok.text, "]"); return tok;
    case '#': next_c(l); tok.type = TOK_HASH;     strcpy(tok.text, "#"); return tok;
    }

    /* string literal */
    if (c == '"') {
        next_c(l); /* skip opening quote */
        size_t i = 0;
        while (l->pos < l->len && peek_c(l) != '"') {
            char ch = next_c(l);
            if (ch == '\\') {
                char esc = next_c(l);
                switch (esc) {
                case 'n':  ch = '\n'; break;
                case 't':  ch = '\t'; break;
                case '\\': ch = '\\'; break;
                case '"':  ch = '"';  break;
                default:   ch = esc;  break;
                }
            }
            if (i < sizeof(tok.text) - 1) tok.text[i++] = ch;
        }
        tok.text[i] = 0;
        if (peek_c(l) == '"') next_c(l); /* skip closing quote */
        tok.type = TOK_STRING;
        return tok;
    }

    /* number: decimal, hex (0x), binary (0b), negative */
    if (isdigit((unsigned char)c) || (c == '-' && isdigit((unsigned char)l->src[l->pos+1]))) {
        size_t i = 0;
        if (c == '-') tok.text[i++] = next_c(l);
        while (l->pos < l->len &&
               (isalnum((unsigned char)peek_c(l)) || peek_c(l) == 'x' || peek_c(l) == 'b')) {
            if (i < sizeof(tok.text) - 1) tok.text[i++] = next_c(l);
            else next_c(l);
        }
        tok.text[i] = 0;
        tok.type  = TOK_INT;
        tok.ival  = (int64_t)strtoll(tok.text, NULL, 0);
        return tok;
    }

    /* directive: starts with '.' */
    if (c == '.') {
        size_t i = 0;
        tok.text[i++] = next_c(l); /* consume '.' */
        while (l->pos < l->len && (isalnum((unsigned char)peek_c(l)) || peek_c(l) == '_')) {
            if (i < sizeof(tok.text) - 1) tok.text[i++] = next_c(l);
            else next_c(l);
        }
        tok.text[i] = 0;
        tok.type = TOK_DIRECTIVE;
        return tok;
    }

    /* identifier or register */
    if (isalpha((unsigned char)c) || c == '_') {
        size_t i = 0;
        while (l->pos < l->len &&
               (isalnum((unsigned char)peek_c(l)) || peek_c(l) == '_')) {
            if (i < sizeof(tok.text) - 1) tok.text[i++] = next_c(l);
            else next_c(l);
        }
        tok.text[i] = 0;

        int reg = parse_reg(tok.text);
        if (reg >= 0) {
            tok.type = TOK_REG;
            tok.reg  = reg;
        } else {
            tok.type = TOK_IDENT;
        }
        return tok;
    }

    /* unknown */
    tok.type = TOK_UNKNOWN;
    tok.text[0] = next_c(l);
    tok.text[1] = 0;
    return tok;
}

/* peek without consuming */
Token lexer_peek(Lexer *l) {
    size_t saved_pos  = l->pos;
    int    saved_line = l->line;
    int    saved_col  = l->col;
    Token t = lexer_next(l);
    l->pos  = saved_pos;
    l->line = saved_line;
    l->col  = saved_col;
    return t;
}

const char *tok_type_name(TokenType t) {
    switch (t) {
    case TOK_EOF:       return "EOF";
    case TOK_NEWLINE:   return "NEWLINE";
    case TOK_COMMA:     return "COMMA";
    case TOK_COLON:     return "COLON";
    case TOK_LBRACKET:  return "LBRACKET";
    case TOK_RBRACKET:  return "RBRACKET";
    case TOK_HASH:      return "HASH";
    case TOK_IDENT:     return "IDENT";
    case TOK_REG:       return "REG";
    case TOK_INT:       return "INT";
    case TOK_STRING:    return "STRING";
    case TOK_DIRECTIVE: return "DIRECTIVE";
    default:            return "UNKNOWN";
    }
}
