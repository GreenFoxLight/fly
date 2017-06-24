//
// Created by Kevin Trogant on 13.05.17.
//

#ifndef MULTITHREADED_COMPILER_LEXER_H
#define MULTITHREADED_COMPILER_LEXER_H

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include "location.h"
#include "fly.h"

typedef struct {
    /* location data */
    char* path;
    int current_line;
    int current_column;
    int start_line;
    int start_column;

    /* state */
    bool inside_string;
    bool inside_line_comment;
    int block_comment_depth;

    FILE* file;
} lexer_t;

typedef enum {
    TOKEN_T_EOF = 0,
    TOKEN_T_ID = 300,
    // constants
    TOKEN_T_INT = 301,
    TOKEN_T_UINT = 302,
    TOKEN_T_INTL = 303,
    TOKEN_T_UINTL = 304,
    TOKEN_T_FLOAT32 = 305,
    TOKEN_T_FLOAT64 = 306,
    TOKEN_T_CHAR = 307,
    TOKEN_T_STRING = 308,
    TOKEN_T_BOOL = 309,
    // braces
    TOKEN_T_LBRACE = '(',
    TOKEN_T_RBRACE = ')',
    TOKEN_T_LCURL = '{',
    TOKEN_T_RCURL = '}',
    TOKEN_T_LSQUARE = '[',
    TOKEN_T_RSQUARE = ']',
    TOKEN_T_LANGLE = '<',
    TOKEN_T_RANGLE = '>',
    // punctation
    TOKEN_T_COMMA = ',',
    TOKEN_T_SEMICOLON = ';',
    TOKEN_T_DOT = '.',
    TOKEN_T_COLON = ':',
    TOKEN_T_RANGE = 400, // ..
    TOKEN_T_ELLIPSIS = 401, // ...
    // compiler directives
     TOKEN_T_META = '#',
    // declarations
    TOKEN_T_FUNC_DECL = 310, // ::
    TOKEN_T_DECL_ASSIGN = 311, // :=
    // operators
    TOKEN_T_ADD = '+',
    TOKEN_T_SUB = '-',
    TOKEN_T_MUL = '*',
    TOKEN_T_DIV = '/',
    TOKEN_T_MOD = '%',
    TOKEN_T_BITWISE_AND = '&',
    TOKEN_T_BITWISE_OR = '|',
    TOKEN_T_BITWISE_XOR = '^',
    TOKEN_T_BITWISE_NOT = '~',
    TOKEN_T_SHIFT_LEFT = 312, // <<
    TOKEN_T_SHIFT_RIGHT = 313, // >>
    TOKEN_T_AND = 314, // &&
    TOKEN_T_OR = 315, // ||
    TOKEN_T_NOT = '!',
    TOKEN_T_INC = 316, // ++
    TOKEN_T_DEC = 317, // --
    TOKEN_T_ASSIGN = '=',
    TOKEN_T_ADD_ASSIGN = 318, // +=
    TOKEN_T_SUB_ASSIGN = 319, // -=
    TOKEN_T_MUL_ASSIGN = 320, // *=
    TOKEN_T_DIV_ASSIGN = 321, // /=
    TOKEN_T_MOD_ASSIGN = 322, // %=
    TOKEN_T_BITWISE_AND_ASSIGN = 323, // &=
    TOKEN_T_BITWISE_OR_ASSIGN = 324, // |=
    TOKEN_T_BITWISE_XOR_ASSIGN = 325, // ^=
    TOKEN_T_SHIFT_LEFT_ASSIGN = 326, // <<=
    TOKEN_T_SHIFT_RIGHT_ASSIGN = 327, // >>=
    TOKEN_T_EQUAL = 328, // ==
    TOKEN_T_NOT_EQUAL = 329, // !=
    TOKEN_T_LESS_EQUAL = 330, // <=
    TOKEN_T_GREATER_EQUAL = 331, // >=
    // keywords
    TOKEN_T_KW_MOD = 332,
    TOKEN_T_KW_LET = 333,
    TOKEN_T_KW_TYPE = 334,
    TOKEN_T_KW_STATIC = 335,
    TOKEN_T_KW_CONST = 336,
    TOKEN_T_KW_AUTO = 337,
    TOKEN_T_KW_NEW = 338,
    TOKEN_T_KW_DELETE = 339,
    TOKEN_T_KW_STRUCT = 340,
    TOKEN_T_KW_UNION = 341,
    TOKEN_T_KW_ENUM = 342,
    TOKEN_T_KW_IF = 343,
    TOKEN_T_KW_ELSE = 344,
    TOKEN_T_KW_FOR = 345,
    TOKEN_T_KW_WHILE = 346,
    TOKEN_T_KW_DO = 347,
    TOKEN_T_KW_CAST = 348,
    TOKEN_T_KW_FUNC = 349,
    TOKEN_T_KW_EXTERN = 350,
    TOKEN_T_KW_DEFER = 351,
    TOKEN_T_KW_SWITCH = 352,
    TOKEN_T_KW_CASE = 353,
    TOKEN_T_KW_DEFAULT = 354,
    TOKEN_T_KW_RETURN = 355,
    // arrows
    TOKEN_T_ARROW = 356,
    TOKEN_T_BIG_ARROW = 357,

    TOKEN_T_COUNT,
} token_tag_t;

typedef struct {
    /* location data */
    location_t loc;

    union {
        char* string;
        char character;
        bool boolean;
        i32 signed_int;
        u32 unsigned_int;
        i64 signed_long;
        u64 unsigned_long;
        f32 float32;
        f64 float64;
    } value;

    token_tag_t tag;
} token_t;

/** Initialize a new lexer instance, responsible for the given file
 */
int lexer_init(lexer_t* lexer, char* file);
void lexer_release(lexer_t* lexer);

token_t lexer_get_next(lexer_t* lexer);

#endif //MULTITHREADED_COMPILER_LEXER_H
