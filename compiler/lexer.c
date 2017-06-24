//
// Created by Kevin Trogant on 13.05.17.
//

#include "lexer.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

int lexer_init(lexer_t* lexer, char* file) {
    /* attempt to open the file */
    FILE* f = fopen(file, "r");
    if (!f)
        return 0;
    /* copy file path */
    lexer->path = malloc(strlen(file) + 1);
    if (!lexer->path) {
        fclose(f);
        return 0;
    }
    strcpy(lexer->path, file);

    lexer->start_line = 1;
    lexer->start_column = 1;
    lexer->current_line = 1;
    lexer->current_column = 1;
    lexer->file = f;
    lexer->block_comment_depth = 0;
    lexer->inside_line_comment = false;
    lexer->inside_string = false;

    return 1;
}

void lexer_release(lexer_t* lexer) {
    if (!lexer)
        return;
    if (lexer->file) fclose(lexer->file);
    if (lexer->path) free(lexer->path);
}

static bool char_in_string(char c, const char* str) {
    for (size_t i = 0; i < strlen(str); i++) {
        if (c == str[i])
            return true;
    }
    return false;
}

static int get_next_char(lexer_t* lexer) {
    return fgetc(lexer->file);
}

token_t lexer_get_next(lexer_t* lexer) {
    token_t token;
    size_t curBufSize = 40;
    char* buffer = (char*)malloc(curBufSize);
    if (!buffer) {
        fprintf(stderr, "Out of memory!\n");
        exit(1);
    }

    // skip leading whitespaces
    bool skippingWhitespaces = true;
    while (skippingWhitespaces) {
        int nextChar = get_next_char(lexer);
        if (nextChar == -1) {
            token_t eof_token = { .tag = TOKEN_T_EOF };
            return eof_token;
        } else if (nextChar == ' ' || nextChar == '\t') {
            lexer->current_column++;
        } else if (nextChar == '\n') {
            lexer->current_column = 1;
            lexer->current_line++;
        } else {
            skippingWhitespaces = false;
            ungetc(nextChar, lexer->file); // re-read that character
        }
    }

    lexer->start_line = lexer->current_line;
    lexer->start_column = lexer->current_column;

    int firstChar = get_next_char(lexer);
    if (firstChar == -1) { // TODO: I don't think this can happen...
        token.tag = TOKEN_T_EOF;
        goto out;
    }
    lexer->current_column++;

    if ((char)firstChar == '/') {
        // comment
        int leadingChar = get_next_char(lexer);
        if (leadingChar == -1) {
            fprintf(stderr, "[Lexer] Unexpected end of file. %s in line %d\n", lexer->path, lexer->current_line);
            exit(1);
        }
        lexer->current_column++;
        if ((char)leadingChar == '/') {
            // line comment
            // read until end of line
            int c = 0;
            do {
                c = get_next_char(lexer);
                if (c == -1) {
                    fprintf(stderr, "[Lexer] Unexpected end of file. %s in line %d\n", lexer->path, lexer->current_line);
                    exit(1);
                }
                lexer->current_column++;
            } while ((char)c != '\n');
            lexer->current_line++;
            lexer->current_column = 1;
            token = lexer_get_next(lexer);
        } else if ((char)leadingChar == '*') {
            // block comment
            int blockCommentDepth = 1;
            int prevChar, c = leadingChar;
            while (blockCommentDepth > 0) {
                prevChar = c;
                c = get_next_char(lexer);
                if (c == -1) {
                    fprintf(stderr, "[Lexer] Unexpected end of file. %s in line %d\n", lexer->path, lexer->current_line);
                    exit(1);
                }
                lexer->current_column++;
                if (c == '\n') {
                    lexer->current_line++;
                    lexer->current_column = 1;
                } else if ((c == '/') && (prevChar == '*')) {
                    blockCommentDepth--;
                } else if ((c == '*') && (prevChar == '/')) {
                    blockCommentDepth++;
                }
            }
            token = lexer_get_next(lexer);
        } else {
            fprintf(stderr, "[Lexer] Unexpected character %c at %s %d:%d\n",
                    leadingChar, lexer->path, lexer->current_line, lexer->current_column);
            exit(1);
        }
    } else if (char_in_string((char)firstChar, "0123456789")) {
        // numbers
        int base = 10;
        int i = 0;
        bool couldBeBinary = true;
        bool isLong = false, isUnsigned = false;
        bool isFloat32 = false, isFloat64 = false;
        if ((char)firstChar == '0') {
            // this may be an hex- or octal number
            int peek = get_next_char(lexer);
            if (peek == -1) {
                fprintf(stderr, "[Lexer] Unexpected end of file. %s in line %d\n", lexer->path, lexer->current_line);
                exit(1);
            }
            if ((char)peek == 'x') {
                base = 16;
                lexer->current_column++;
                couldBeBinary = false;
                isUnsigned = true;
            } else {
                base = 8;
                ungetc(peek, lexer->file);
                isUnsigned = true;
            }
        } else {
            buffer[0] = firstChar;
            i = 1;
        }
        bool readingNumber = true;
        while (readingNumber) {
            int nextChar = get_next_char(lexer);
            lexer->current_column++;
            if (nextChar == -1) {
                fprintf(stderr, "[Lexer] Unexpected end of file. %s in line %d\n", lexer->path, lexer->current_line);
                exit(1);
            } else if (char_in_string((char)nextChar, "01")) {
                buffer[i] = (char)nextChar;
                i++;
            } else if (char_in_string((char)nextChar, "234567")) {
                couldBeBinary = false;
                buffer[i] = (char)nextChar;
                i++;
            } else if (char_in_string((char)nextChar, "89")) {
                if (base == 8) {
                    fprintf(stderr, "[Lexer] Digits 8 and 9 are not allowed in octal numbers. %s %d:%d-%d:%d\n",
                            lexer->path, lexer->start_line, lexer->start_column, lexer->current_line, lexer->current_column);
                    exit(1);
                }
                buffer[i] = (char)nextChar;
                i++;
            } else if ((base == 16) && char_in_string((char)nextChar, "abcdefABCDEF")) {
                if (base != 16) {
                    fprintf(stderr, "[Lexer] Digits a-f are only allowed in hexadecimal numbers. %s %d:%d-%d:%d\n",
                            lexer->path, lexer->start_line, lexer->start_column, lexer->current_line, lexer->current_column);
                    exit(1);
                }
                if (char_in_string((char)nextChar, "ABCDEF")) {
                    nextChar += 'a' - 'A'; // convert to lower case
                }
                buffer[i] = (char)nextChar;
                i++;
            } else if ((char)nextChar == 'b') {
                if (couldBeBinary) {
                    base = 2;
                    isLong = i >= 32;
                    isUnsigned = true;
                } else {
                    fprintf(stderr, "[Lexer] Suffix 'b' is only allowed after binary numbers. %s %d:%d-%d:%d\n",
                            lexer->path, lexer->start_line, lexer->start_column, lexer->current_line, lexer->current_column);
                    exit(1);
                }
                readingNumber = false;
            } else if ((char)nextChar == 'L') {
                isLong = true;
                readingNumber = false;
            } else if ((char)nextChar == 'u') {
                isUnsigned = true;
                readingNumber = false;
            } else if ((char)nextChar == '.') {
                isFloat64 = true;
                buffer[i] = (char)nextChar;
                i++;
            } else if ((char)nextChar == 'f') {
                isFloat32 = true;
                isFloat64 = false;
                isLong = false;
                isUnsigned = false;
                readingNumber = false;
            } else {
                readingNumber = false;
                ungetc(nextChar, lexer->file);
                lexer->current_column--;
            }
            if ((size_t)i == curBufSize) {
                curBufSize *= 2;
                buffer = (char*)realloc(buffer, curBufSize);
                if (!buffer) {
                    fprintf(stderr, "Out of memory.\n");
                    exit(1);
                }
            }
        }
        buffer[i] = '\0';
        if (isFloat32) {
            token.tag = TOKEN_T_FLOAT32;
            sscanf(buffer, "%f", &token.value.float32);
        } else if (isFloat64) {
            token.tag = TOKEN_T_FLOAT64;
            sscanf(buffer, "%lf", &token.value.float64);
        } else if (isUnsigned) {
            if (isLong) {
                token.tag = TOKEN_T_UINTL;
                if (base == 10)
                    sscanf(buffer, "%llu", &token.value.unsigned_long);
                else if (base == 8)
                    sscanf(buffer, "%llo", &token.value.unsigned_long);
                else if (base == 16)
                    sscanf(buffer, "%llx", &token.value.unsigned_long);
                else if (base == 2) {
                    i -= 1; // skip '\0'
                    uint64_t d = 1;
                    token.value.unsigned_long = 0;
                    do {
                        if (buffer[i] == '1') {
                            token.value.unsigned_long += d;
                        }
                        d *= 2;
                        i--;
                    } while (i >= 0);
                }
            } else {
                token.tag = TOKEN_T_UINT;
                if (base == 10)
                    sscanf(buffer, "%u", &token.value.unsigned_int);
                else if (base == 8)
                    sscanf(buffer, "%o", &token.value.unsigned_int);
                else if (base == 16)
                    sscanf(buffer, "%x", &token.value.unsigned_int);
                else if (base == 2) {
                    i -= 1; // skip '\0'
                    uint32_t d = 1;
                    token.value.unsigned_int = 0;
                    do {
                        if (buffer[i] == '1') {
                            token.value.unsigned_long += d;
                        }
                        d *= 2;
                        i--;
                    } while (i >= 0);
                }
            }
        } else {
            if (isLong) {
                token.tag = TOKEN_T_INTL;
                assert(base == 10);
                sscanf(buffer, "%lld", &token.value.signed_long);
            } else {
                token.tag = TOKEN_T_INT;
                assert(base == 10);
                sscanf(buffer, "%d", &token.value.signed_int);
            }
        }
    } else if ((char)firstChar == '"') {
        // string literal
        int i = 0;
        int nextChar = 0;
        while ((char)nextChar != '"') {
            nextChar = get_next_char(lexer);
            if (nextChar == -1) {
                fprintf(stderr, "[Lexer] Unexpected end of file in string literal. %s in line %d\n",
                        lexer->path, lexer->current_line);
                exit(1);
            }
            lexer->current_column++;
            if ((char)nextChar == '\n') {
                lexer->current_column = 1;
                lexer->current_line++;
            }
            buffer[i] = ((char)nextChar == '"') ? '\0' : (char)nextChar;
            i++;
            if ((size_t)i == curBufSize) {
                curBufSize *= 2;
                buffer = (char*)realloc(buffer, curBufSize);
                if (!buffer) {
                    fprintf(stderr, "Out of memory.\n");
                    exit(1);
                }
            }
        }
        token.tag = TOKEN_T_STRING;
        token.value.string = malloc(strlen(buffer) + 1);
        strcpy(token.value.string, buffer);
    } else if ((char)firstChar == '\'') {
        // character literal
        token.tag = TOKEN_T_CHAR;
        int c = get_next_char(lexer);
        lexer->current_column++;
        if (c == -1) {
            fprintf(stderr, "[Lexer] Unexpected end of file in character literal. %s in line %d\n",
                    lexer->path, lexer->current_line);
            exit(1);
        } else if ((char)c == '\n') {
            fprintf(stderr, "[Lexer] Unexpected newline in character literal at %s %d:%d\n",
                    lexer->path, lexer->current_line, lexer->current_column);
            exit(1);
        } else if ((char)c == '\'') {
            fprintf(stderr, "[Lexer] Error: Empty character literal at %s %d:%d\n",
                    lexer->path, lexer->current_line, lexer->current_column);
            exit(1);
        } else if ((char)c == '\\') {
            // escape character
            c = get_next_char(lexer);
            lexer->current_column++;
            switch (c) {
                case 'n':
                    token.value.character = '\n';
                    break;
                case 'r':
                    token.value.character = '\r';
                    break;
                case 't':
                    token.value.character = '\t';
                    break;
                case '\\':
                    token.value.character = '\\';
                    break;
                default:
                    fprintf(stderr, "[Lexer] Unrecognized escape character %c at %s %d:%d\n",
                            c, lexer->path, lexer->current_line, lexer->current_column);
                    exit(1);
                    break;
            }
        } else {
            token.value.character = c;
        }
        c = get_next_char(lexer); // must be closed by another '
        if ((char)c != '\'') {
            fprintf(stderr, "[Lexer] Missing ' in character literal at %s %d:%d\n", lexer->path,
                    lexer->current_line, lexer->current_column);
            exit(1);
        }
        lexer->current_column++;
    } else if ((char)firstChar == '(') { /* Braces */
        token.tag = TOKEN_T_LBRACE;
    } else if ((char)firstChar == ')') {
        token.tag = TOKEN_T_RBRACE;
    } else if ((char)firstChar == '{') {
        token.tag = TOKEN_T_LCURL;
    } else if ((char)firstChar == '}') {
        token.tag = TOKEN_T_RCURL;
    } else if ((char)firstChar == '[') {
        token.tag = TOKEN_T_LSQUARE;
    } else if ((char)firstChar == ']') {
        token.tag = TOKEN_T_RSQUARE;
    } else if ((char)firstChar == '<') {
        int peek = get_next_char(lexer);
        if (peek == '=') {
            token.tag = TOKEN_T_LESS_EQUAL;
            lexer->current_column++;
        } else if (peek == '<') {
            lexer->current_column++;
            int peek2 = get_next_char(lexer);
            if (peek2 == '=') {
                lexer->current_column++;
                token.tag = TOKEN_T_SHIFT_LEFT_ASSIGN;
            } else {
                ungetc(peek2, lexer->file);
                token.tag = TOKEN_T_SHIFT_LEFT;
            }
        } else {
            ungetc(peek, lexer->file);
            token.tag = TOKEN_T_LANGLE;
        }
    } else if ((char)firstChar == '>') {
        int peek = get_next_char(lexer);
        if (peek == '=') {
            token.tag = TOKEN_T_GREATER_EQUAL;
            lexer->current_column++;
        } else if (peek == '>') {
            lexer->current_column++;
            int peek2 = get_next_char(lexer);
            if (peek2 == '=') {
                lexer->current_column++;
                token.tag = TOKEN_T_SHIFT_RIGHT_ASSIGN;
            } else {
                ungetc(peek2, lexer->file);
                token.tag = TOKEN_T_SHIFT_RIGHT;
            }
        } else {
            ungetc(peek, lexer->file);
            token.tag = TOKEN_T_RANGLE;
        }
    } else if ((char)firstChar == '#') {
        token.tag = TOKEN_T_META;
    } else if ((char)firstChar == ';') { /* Semicolon, dot, comma */
        token.tag = TOKEN_T_SEMICOLON;
    } else if ((char)firstChar == '.') {
        int peek = get_next_char(lexer);
        if (peek == '.') {
            int peek2 = get_next_char(lexer);
            lexer->current_column++;
            if (peek2 == '.') {
                lexer->current_column++;
                token.tag = TOKEN_T_ELLIPSIS;
            } else {
                ungetc(peek2, lexer->file);
                token.tag = TOKEN_T_RANGE;
            }
        } else {
            ungetc(peek, lexer->file);
            token.tag = TOKEN_T_DOT;
        }
    } else if ((char)firstChar == ',') {
        token.tag = TOKEN_T_COMMA;
    } else if ((char)firstChar == ':') {
        int peek = get_next_char(lexer);
        if (peek == ':') {
            token.tag = TOKEN_T_FUNC_DECL;
            lexer->current_column++;
        } else if (peek == '=') {
            token.tag = TOKEN_T_DECL_ASSIGN;
            lexer->current_column++;
        } else {
            ungetc(peek, lexer->file);
            token.tag = TOKEN_T_COLON;
        }
    } else if ((char)firstChar == '+') { /* Operators */
        int peek = get_next_char(lexer);
        if (char_in_string((char)peek, "0123456789")) {
            ungetc(peek, lexer->file);
            token_t number = lexer_get_next(lexer);
            switch (number.tag) { // do nothing, but print an error if we are not followed by '+'
                case TOKEN_T_INT:
                case TOKEN_T_INTL:
                case TOKEN_T_UINT:
                case TOKEN_T_UINTL:
                case TOKEN_T_FLOAT32:
                case TOKEN_T_FLOAT64:
                    break;
                default:
                    fprintf(stderr, "[Lexer] Expected number after '+' in file %s line %d.\n",
                            number.loc.file, number.loc.start_line);
                    exit(1);
            }
            number.loc.start_column -= 1;
            token = number;

        } else if (peek == '+') {
            token.tag = TOKEN_T_INC;
            lexer->current_column++;
        } else if (peek == '=') {
            token.tag = TOKEN_T_ADD_ASSIGN;
            lexer->current_column++;
        } else {
            ungetc(peek, lexer->file);
            token.tag = TOKEN_T_ADD;
        }
    } else if ((char)firstChar == '-') {
        int peek = get_next_char(lexer);
        if (char_in_string((char)peek, "0123456789")) {
            ungetc(peek, lexer->file);
            token_t number = lexer_get_next(lexer);
            switch (number.tag) {
                case TOKEN_T_INT:
                    number.value.signed_int *= -1;
                    break;
                case TOKEN_T_INTL:
                    number.value.signed_long *= -1;
                    break;
                case TOKEN_T_UINT:
                case TOKEN_T_UINTL:
                    fprintf(stderr, "Warning, '-' before unsigned integer in file %s line %d.\n",
                            number.loc.file, number.loc.start_line);
                    break;
                case TOKEN_T_FLOAT32:
                    number.value.float32 *= -1;
                    break;
                case TOKEN_T_FLOAT64:
                    number.value.float64 *= -1;
                    break;
                default:
                    fprintf(stderr, "[Lexer] Expected number after '-' in file %s line %d.\n",
                            number.loc.file, number.loc.start_line);
                    exit(1);
            }
            number.loc.start_column -= 1;
            token = number;
        } else if (peek == '-') {
            token.tag = TOKEN_T_DEC;
            lexer->current_column++;
        } else if (peek == '=') {
            token.tag = TOKEN_T_SUB_ASSIGN;
            lexer->current_column++;
        } else if (peek == '>') {
            token.tag = TOKEN_T_ARROW;
            lexer->current_column++;
        } else {
            ungetc(peek, lexer->file);
            token.tag = TOKEN_T_SUB;
        }
    } else if ((char)firstChar == '*') {
        int peek = get_next_char(lexer);
        if (peek == '=') {
            token.tag = TOKEN_T_MUL_ASSIGN;
            lexer->current_column++;
        } else {
            ungetc(peek, lexer->file);
            token.tag = TOKEN_T_MUL;
        }
    } else if ((char)firstChar == '/') {
        int peek = get_next_char(lexer);
        if (peek == '=') {
            token.tag = TOKEN_T_DIV_ASSIGN;
            lexer->current_column++;
        } else {
            ungetc(peek, lexer->file);
            token.tag = TOKEN_T_DIV;
        }
    } else if ((char)firstChar == '%') {
        int peek = get_next_char(lexer);
        if (peek == '=') {
            token.tag = TOKEN_T_MOD_ASSIGN;
            lexer->current_column++;
        } else {
            ungetc(peek, lexer->file);
            token.tag = TOKEN_T_MOD;
        }
    } else if ((char)firstChar == '&') {
        int peek = get_next_char(lexer);
        if (peek == '=') {
            token.tag = TOKEN_T_BITWISE_AND_ASSIGN;
            lexer->current_column++;
        } else if (peek == '&') {
            token.tag = TOKEN_T_AND;
            lexer->current_column++;
        } else {
            ungetc(peek, lexer->file);
            token.tag = TOKEN_T_BITWISE_AND;
        }
    } else if ((char)firstChar == '|') {
        int peek = get_next_char(lexer);
        if (peek == '=') {
            token.tag = TOKEN_T_BITWISE_OR_ASSIGN;
            lexer->current_column++;
        } else if (peek == '|') {
            token.tag = TOKEN_T_OR;
            lexer->current_column++;
        } else {
            ungetc(peek, lexer->file);
            token.tag = TOKEN_T_BITWISE_OR;
        }
    } else if ((char)firstChar == '^') {
        int peek = get_next_char(lexer);
        if (peek == '=') {
            token.tag = TOKEN_T_BITWISE_XOR_ASSIGN;
            lexer->current_column++;
        } else {
            ungetc(peek, lexer->file);
            token.tag = TOKEN_T_BITWISE_XOR;
        }
    } else if ((char)firstChar == '~') {
        token.tag = TOKEN_T_BITWISE_NOT;
    } else if ((char)firstChar == '!') {
        int peek = get_next_char(lexer);
        if (peek == '=') {
            token.tag = TOKEN_T_NOT_EQUAL;
            lexer->current_column++;
        } else {
            ungetc(peek, lexer->file);
            token.tag = TOKEN_T_NOT;
        }
    } else if ((char)firstChar == '=') {
        int peek = get_next_char(lexer);
        if (peek == '=') {
            token.tag = TOKEN_T_EQUAL;
            lexer->current_column++;
        } else if (peek == '>') {
            token.tag = TOKEN_T_BIG_ARROW;
            lexer->current_column++;
        } else {
            ungetc(peek, lexer->file);
            token.tag = TOKEN_T_ASSIGN;
        }
    } else if (char_in_string((char)firstChar, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_")) {
        // identifier
        int i = 1;
        buffer[0] = (char)firstChar;
        bool done = false;
        while (!done) {
            if ((size_t)i == curBufSize) {
                curBufSize *= 2;
                buffer = (char*)realloc(buffer, curBufSize);
                if (!buffer) {
                    fprintf(stderr, "Out of memory!\n");
                    exit(1);
                }
            }

            int nextChar = get_next_char(lexer);
            if (nextChar == -1) {
                fprintf(stderr, "[Lexer] Unexpected end of file. %s in line %d\n", lexer->path, lexer->current_line);
                exit(1);
            }
            if (!char_in_string((char)nextChar, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_")) {
                done = true;
                buffer[i] = '\0';
                ungetc(nextChar, lexer->file);
            } else {
                buffer[i] = (char)nextChar;
                lexer->current_column++;
            }

            i++;
        }
        // check if we got an keyword or boolean constant
        if (strcmp(buffer, "true") == 0) {
            token.tag = TOKEN_T_BOOL;
            token.value.boolean = true;
        } else if (strcmp(buffer, "false") == 0) {
            token.tag = TOKEN_T_BOOL;
            token.value.boolean = false;
        } else if (strcmp(buffer, "mod") == 0) {
            token.tag = TOKEN_T_KW_MOD;
        } else if (strcmp(buffer, "let") == 0) {
            token.tag = TOKEN_T_KW_LET;
        } else if (strcmp(buffer, "type") == 0) {
            token.tag = TOKEN_T_KW_TYPE;
        } else if (strcmp(buffer, "static") == 0) {
            token.tag = TOKEN_T_KW_STATIC;
        } else if (strcmp(buffer, "const") == 0) {
            token.tag = TOKEN_T_KW_CONST;
        } else if (strcmp(buffer, "new") == 0) {
            token.tag = TOKEN_T_KW_NEW;
        } else if (strcmp(buffer, "delete") == 0) {
            token.tag = TOKEN_T_KW_DELETE;
        } else if (strcmp(buffer, "auto") == 0) {
            token.tag = TOKEN_T_KW_AUTO;
        } else if (strcmp(buffer, "struct") == 0) {
            token.tag = TOKEN_T_KW_STRUCT;
        } else if (strcmp(buffer, "union") == 0) {
            token.tag = TOKEN_T_KW_UNION;
        } else if (strcmp(buffer, "enum") == 0) {
            token.tag = TOKEN_T_KW_ENUM;
        } else if (strcmp(buffer, "if") == 0) {
            token.tag = TOKEN_T_KW_IF;
        } else if (strcmp(buffer, "else") == 0) {
            token.tag = TOKEN_T_KW_ELSE;
        } else if (strcmp(buffer, "cast") == 0) {
            token.tag = TOKEN_T_KW_CAST;
        } else if (strcmp(buffer, "fn") == 0) {
            token.tag = TOKEN_T_KW_FUNC;
        } else if (strcmp(buffer, "extern") == 0) {
            token.tag = TOKEN_T_KW_EXTERN;
        } else if (strcmp(buffer, "for") == 0) {
            token.tag = TOKEN_T_KW_FOR;
        } else if (strcmp(buffer, "while") == 0) {
            token.tag = TOKEN_T_KW_WHILE;
        } else if (strcmp(buffer, "do") == 0) {
            token.tag = TOKEN_T_KW_DO;
        } else if (strcmp(buffer, "defer") == 0) {
            token.tag = TOKEN_T_KW_DEFER;
        } else if (strcmp(buffer, "switch") == 0) {
            token.tag = TOKEN_T_KW_SWITCH;
        } else if (strcmp(buffer, "case") == 0) {
            token.tag = TOKEN_T_KW_CASE;
        } else if (strcmp(buffer, "default") == 0) {
            token.tag = TOKEN_T_KW_DEFAULT;
        } else if (strcmp(buffer, "return") == 0) {
            token.tag = TOKEN_T_KW_RETURN;
        } else {
            token.tag = TOKEN_T_ID;
            token.value.string = malloc(strlen(buffer) + 1);
            strcpy(token.value.string, buffer);
        }
    } else {
        fprintf(stderr, "[Lexer] Unexpected character %c at %s %d:%d\n", firstChar, lexer->path, lexer->current_line, lexer->current_column);
        exit(1);
    }

out:
    if (lexer->path) {
        token.loc.file = malloc(strlen(lexer->path) + 1);
        strcpy(token.loc.file, lexer->path);
    } else {
        token.loc.file = NULL;
    }
    token.loc.start_line = lexer->start_line;
    token.loc.start_column = lexer->start_column;
    token.loc.end_line = lexer->current_line;
    token.loc.end_column = lexer->current_column;
    free(buffer);
    return token;

}
