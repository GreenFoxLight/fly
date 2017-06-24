#pragma once

#include "fly.h"
#include "lexer.h"
#include "location.h"

#define AST_INVALID_ID 0
typedef u64 ast_id;

typedef struct {
    location_t location;
} ast_node_t;

typedef struct {
    int debug_indent;
    token_t next;
    lexer_t lexer;
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
