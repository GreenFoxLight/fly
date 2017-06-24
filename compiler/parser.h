#pragma once

#include "fly.h"
#include "lexer.h"
#include "location.h"

#define AST_INVALID_ID 0
typedef u64 ast_id;

typedef enum {
    /* constants */
    AST_CONST_INT,
    AST_CONST_UINT,
    AST_CONST_INTL,
    AST_CONST_UINTL,
    AST_CONST_FLOAT32,
    AST_CONST_FLOAT64,
    AST_CONST_BOOL,
    AST_CONST_CHAR,
    AST_CONST_STRING,
    /* identifier */
    AST_ID,
    /* field access */
    AST_FIELD_ACCESS,
    /* types */
    AST_STRUCT,
    AST_UNION,
    AST_ENUM,
    AST_ARRAY,
    AST_POINTER,
    AST_AUTO,
    /* statements */
    AST_IF,
    AST_ELSE_IF,
    AST_ELSE,
    AST_FOR,
    AST_WHILE,
    AST_DO_WHILE,
    AST_SWITCH,
    AST_CASE,
    AST_DEFAULT,
    AST_RETURN,
    AST_DEFER,
    /* expressions */
    AST_INFIX_EXPR,
    AST_PREFIX_EXPR,
    AST_POSTFIX_EXPR,
    AST_OPERATOR,
    /* special operators */
    AST_CALL,
    AST_ARRAY_ACCESS,
    /* meta instructions */
    AST_META_LOAD,
    AST_META_RUN,
    AST_ANNOTATION,
    /* declarations */
    AST_VAR_DECL,
    AST_FUNC_DECL,
    AST_TYPE_DECL,
    /* blocks */
    AST_BLOCK,
    AST_CAPTURE,
    /* functions */
    AST_FUNCTION,
    AST_FUNC_PARAMS,
    
    AST_PROGRAM,
} synentry_tag_t;

typedef struct {
    union {
        /* "constants" */
        char* string;
        i32 integer;
        i64 long_int;
        u32 unsigned_int;
        u64 unsigned_long;
        f32 float32;
        f64 float64;
        bool boolean;
        char character;
        token_tag_t operator;

        /* tree nodes */
        ast_id tag;
        struct {
            ast_id first; 
            ast_id second;
        } pair;

        struct {
            ast_id* list;
            size_t length;
        } list;
    } value;
    synentry_tag_t tag;
    u8 type; /* internal use */
} synentry_t;

typedef struct {
    synentry_t* entries;
    u64 num_entries;
} syntree_t;

int init_syntree(syntree_t* tree);
void release_syntree(syntree_t* tree);

/* Return 0: Don't visit children, 1: Visit children */
typedef int (*syntree_traverse_fnc)(synentry_t*);
void syntree_traverse(syntree_t* tree, ast_id root, syntree_traverse_fnc fnc);

synentry_t* syntree_get_entry(syntree_t* tree, ast_id id);

ast_id syntree_add_int(syntree_t* tree, i32 i);
ast_id syntree_add_uint(syntree_t* tree, u32 u);
ast_id syntree_add_long(syntree_t* tree, i64 l);
ast_id syntree_add_ulong(syntree_t* tree, u64 u);
ast_id syntree_add_f32(syntree_t* tree, f32 f);
ast_id syntree_add_f64(syntree_t* tree, f64 f);
ast_id syntree_add_bool(syntree_t* tree, bool b);
ast_id syntree_add_char(syntree_t* tree, char c);
ast_id syntree_add_string(syntree_t* tree, const char* s);
ast_id syntree_add_id(syntree_t* tree, const char* id);
ast_id syntree_add_operator(syntree_t* tree, token_tag_t op);

ast_id syntree_add_tag(syntree_t* tree, synentry_tag_t tag, ast_id contained);
ast_id syntree_add_pair(syntree_t* tree, synentry_tag_t tag,
        ast_id first, ast_id second);
ast_id syntree_add_list(syntree_t* tree, synentry_tag_t tag,
        size_t length, ...);

typedef struct {
    location_t location;
} ast_node_t;

typedef struct {
    int debug_indent;
    token_t next;
    lexer_t lexer;
    syntree_t syntree;
} parser_t;

int init_parser(parser_t* parser, char* file);
void release_parser(parser_t* parser);

ast_id parse_program(parser_t* parser);

ast_id parse_id(parser_t* parser);

ast_id parse_meta_instruction(parser_t* parser);

ast_id parse_declaration(parser_t* parser);
ast_id parse_extern_func_declaration(parser_t* parser);
ast_id parse_func_declaration(parser_t* parser);
ast_id parse_variable_declaration(parser_t* parser);
ast_id parse_type_declaration(parser_t* parser);

ast_id parse_function(parser_t* parser);
ast_id parse_func_params(parser_t* parser);

ast_id parse_annotation(parser_t* parser);

ast_id parse_block(parser_t* parser);
ast_id parse_capture(parser_t* parser);

ast_id parse_statement(parser_t* parser);
ast_id parse_if_stmt(parser_t* parser);
ast_id parse_for_stmt(parser_t* parser);
ast_id parse_while_stmt(parser_t* parser);
ast_id parse_do_while_stmt(parser_t* parser);
ast_id parse_switch_stmt(parser_t* parser);

ast_id parse_expr(parser_t* parser);
ast_id parse_cast(parser_t* parser);
ast_id parse_call(parser_t* parser);
ast_id parse_assign(parser_t* parser);
ast_id parse_array_access(parser_t* parser);

ast_id parse_const_expr(parser_t* parser);
ast_id parse_const_cast(parser_t* parser);

ast_id parse_type(parser_t* parser);
ast_id parse_native_type(parser_t* parser);
ast_id parse_struct_type(parser_t* parser);
ast_id parse_union_type(parser_t* parser);
ast_id parse_enum_type(parser_t* parser);
ast_id parse_func_type(parser_t* parser);
ast_id parse_array_type(parser_t* parser);
ast_id parse_pointer_type(parser_t* parser);
