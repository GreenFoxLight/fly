#include "parser.h"
#include "fly.h"
#include <stdio.h>
#include <assert.h>

/* NOTE(Kevin): Some functions do something like
 * parse_block() {
 *    if (parser->next != "[" ...) {
 *      syntax_error();
 *    }
 * }
 * This is for convenience, to remove the need
 * of doing syntax checking everytime we try
 * to parse these.
 *
 * Functions were there are only a few places at
 * which we parse them - especially when we checked
 * anyways to decice which rule applies - just do
 *
 * an assertion
 */

internal void print_line_marker(const char* file,
        int line, int start_column, int end_column) {
    FILE* f = fopen(file, "r");
    if (!f)
        return;
    for (int i = 1; i < line; i++) {
        while (fgetc(f) != '\n') {} // skip to next line
    }
    char c;
    while ((c = (char)fgetc(f)) != '\n') {
        printf("%c", c);
    }
    printf("\n");
    for (int i = 1; i < start_column; i++)
        printf(" ");
    for (int i = start_column; i < end_column; i++)
        printf("^");
    fclose(f);
    printf("\n");
}

void syntax_error(parser_t* parser, const char* expected) {
    printf("Syntax error. Expected %s at: %s %d:%d\n",
            expected,
            parser->next.loc.file,
            parser->next.loc.start_line,
            parser->next.loc.start_column
          );

    if ((parser->next.loc.start_line == parser->next.loc.end_line) &&
            (parser->next.loc.start_column <= parser->next.loc.end_column)) {
    }
}

void print_indent(parser_t* parser) {
    for (int i = 0; i < parser->debug_indent; i++)
        printf(" ");
}

void next_token(parser_t* parser) {
    parser->next = lexer_get_next(&parser->lexer);
}

int init_parser(parser_t* parser, char* file) {
    if (!lexer_init(&parser->lexer, file))
        return 0;
    if (!init_syntree(&parser->syntree))
        return 0;
    next_token(parser);
    parser->debug_indent = 0;
    return 1;
}

void release_parser(parser_t* parser) {
    lexer_release(&parser->lexer);
    release_syntree(&parser->syntree);
}

ast_id parse_program(parser_t* parser) {
    ast_id program = syntree_add_list(&parser->syntree, AST_PROGRAM, 0);
    do {
        parser->debug_indent = 4;
        ast_id elem = 0;
        if (parser->next.tag == TOKEN_T_KW_LET ||
                parser->next.tag == TOKEN_T_KW_TYPE ||
                parser->next.tag == TOKEN_T_KW_FUNC ||
                parser->next.tag == TOKEN_T_KW_EXTERN)
            elem = parse_declaration(parser);
        else if (parser->next.tag == '#')
            parse_meta_instruction(parser);
        else
            syntax_error(parser, "Declaration or meta instruction");

        if (elem)
            syntree_append_list(&parser->syntree, program, elem);
    } while (parser->next.tag != TOKEN_T_EOF);

    return program;
}


ast_id parse_id(parser_t* parser) {
    if (parser->next.tag != TOKEN_T_ID) {
        syntax_error(parser, "identifier");
        return AST_INVALID_ID;
    }
    print_indent(parser);
    printf("%s\n", parser->next.value.string);
    ast_id id = syntree_add_id(&parser->syntree, parser->next.value.string);
    next_token(parser);

    return id; 
}

ast_id parse_meta_instruction(parser_t* parser) {
    assert(parser->next.tag == '#');
    next_token(parser);

    print_indent(parser);
    printf("meta\n");

    parser->debug_indent += 4;
    parse_id(parser);

    parse_id(parser);
    parser->debug_indent -= 4;

    return 1; 
}

ast_id parse_declaration(parser_t* parser) {
    ast_id decl;
    switch (parser->next.tag) {
        case TOKEN_T_KW_LET:
            decl = parse_variable_declaration(parser);
            break;
        case TOKEN_T_KW_TYPE:
            decl = parse_type_declaration(parser);
            break;
        case TOKEN_T_KW_FUNC:
            decl = parse_func_declaration(parser);
            break;
        case TOKEN_T_KW_EXTERN:
            decl = parse_extern_func_declaration(parser);
            break;
        default:
            syntax_error(parser, "'let', 'type', 'fn' or 'extern'");
            return AST_INVALID_ID;
    }
    if (parser->next.tag != ';') {
        syntax_error(parser, ";");
        return AST_INVALID_ID;
    }
    next_token(parser);
    return decl;
}

ast_id parse_func_declaration(parser_t* parser) {
    assert(parser->next.tag == TOKEN_T_KW_FUNC);
    next_token(parser);

    print_indent(parser);
    printf("fn\n");

    parser->debug_indent += 4;
    ast_id id = parse_id(parser);

    if (parser->next.tag != TOKEN_T_FUNC_DECL) {
        syntax_error(parser, "'::'");
        return AST_INVALID_ID;
    }
    next_token(parser);

    ast_id fnc = parse_function(parser);

    parser->debug_indent -= 4;


    return syntree_add_pair(&parser->syntree, AST_FUNC_DECL, id, fnc);
}

ast_id parse_extern_func_declaration(parser_t* parser) {
    assert(parser->next.tag == TOKEN_T_KW_EXTERN);
    next_token(parser);

    if (parser->next.tag != TOKEN_T_KW_FUNC) {
        syntax_error(parser, "fn");
        return AST_INVALID_ID;
    }
    next_token(parser);

    print_indent(parser);
    printf("extern fn\n");

    parser->debug_indent += 4;
    ast_id id = parse_id(parser);

    next_token(parser);

    ast_id fn = parse_func_type(parser);

    parser->debug_indent -= 4;

    return syntree_add_pair(&parser->syntree, AST_EXT_FUNC_DECL, id, fn);
}

ast_id parse_variable_declaration(parser_t* parser) {
    assert(parser->next.tag == TOKEN_T_KW_LET);
    next_token(parser);

    print_indent(parser);
    printf("let\n");

    parser->debug_indent += 4;
    ast_id id = parse_id(parser);
    ast_id type, value;

    switch (parser->next.tag) {
        case ':':
            next_token(parser);
            if (parser->next.tag == TOKEN_T_KW_AUTO) {
                print_indent(parser);
                printf("auto\n");
                type = AST_INVALID_ID;
            }
            else {
                type = parse_type(parser);
            }
            if (parser->next.tag == '=') {
                next_token(parser);
                value = parse_expr(parser);
            }
            break;
        case TOKEN_T_DECL_ASSIGN:
            next_token(parser);
            type = AST_INVALID_ID;
            value = parse_expr(parser);
            break;
        default:
            syntax_error(parser, "':' or ':='");
            return AST_INVALID_ID;
    }

    parser->debug_indent -= 4;
    return syntree_add_list(&parser->syntree, AST_VAR_DECL,
                            3, id, type, value );
}

ast_id parse_type_declaration(parser_t* parser) {
    assert(parser->next.tag == TOKEN_T_KW_TYPE);
    next_token(parser);

    print_indent(parser);
    printf("type\n");

    parser->debug_indent += 4;
    ast_id id = parse_id(parser);

    if (parser->next.tag != '=') {
        syntax_error(parser, "=");
        return AST_INVALID_ID;
    }
    next_token(parser);

    if (parser->next.tag == '#') {
        parse_annotation(parser);
    }
    ast_id type = parse_type(parser);

    parser->debug_indent -= 4;
    return syntree_add_pair(&parser->syntree, AST_TYPE_DECL, id, type);
}

ast_id parse_function(parser_t* parser) {
    if (parser->next.tag != '(') {
        syntax_error(parser, "'('");
        return AST_INVALID_ID;
    }
    next_token(parser);

    print_indent(parser);
    printf("function\n");

    parser->debug_indent += 4;

    ast_id params = AST_INVALID_ID;
    if (parser->next.tag == TOKEN_T_ID ||
            parser->next.tag == TOKEN_T_ELLIPSIS) {
        params = parse_func_params(parser);
    }

    ast_id ret_type, body;
    switch (parser->next.tag) {
        case TOKEN_T_ARROW:
            next_token(parser);
            ret_type = syntree_add_list(&parser->syntree, AST_RET_TYPE,
                    1, parse_type(parser));
            while (parser->next.tag == ',') {
                next_token(parser);
                ast_id tmp = parse_type(parser);
                if (tmp)
                    syntree_append_list(&parser->syntree, ret_type, tmp);
            }
            body = parse_block(parser);
            break;
        case TOKEN_T_BIG_ARROW:
            ret_type = AST_INVALID_ID;
            next_token(parser);
            body = parse_expr(parser);
            break;
        default:
            syntax_error(parser, "'->' or '=>'");
            return AST_INVALID_ID;
    }
    return syntree_add_list(&parser->syntree, AST_FUNCTION,
            3, params, ret_type, body);
}

ast_id parse_func_params(parser_t* parser) {
    /* ID ':' TYPE ( ',' ID ':' TYPE )* ['...']
     * | '...'
     */
    assert(parser->next.tag == TOKEN_T_ID ||
            parser->next.tag == TOKEN_T_ELLIPSIS);
    print_indent(parser);
    printf("func params\n");
    parser->debug_indent += 4;
    ast_id params = syntree_add_list(&parser->syntree, AST_FUNC_PARAMS, 0);
                                    
    while (parser->next.tag == TOKEN_T_ID) {
        ast_id id = parse_id(parser);
        if (parser->next.tag != ':') {
            syntax_error(parser, ":");
            return AST_INVALID_ID;
        }
        next_token(parser);
        ast_id type = parse_type(parser);
        if (parser->next.tag != ',' &&
                parser->next.tag != ')') {
            syntax_error(parser, "',', ')' or '...'");
            return AST_INVALID_ID;
        }
        ast_id param = syntree_add_pair(&parser->syntree, AST_FUNC_PARAM,
                                        id, type);
        syntree_append_list(&parser->syntree, params, param);
        next_token(parser);
    }
    if (parser->next.tag == TOKEN_T_ELLIPSIS) {
        print_indent(parser);
        printf("...\n");
        ast_id elp = syntree_add_ellipsis(&parser->syntree);
        syntree_append_list(&parser->syntree, params, elp); 
    }
    parser->debug_indent -= 4;

    return params;
}

ast_id parse_annotation(parser_t* parser) {
    assert(parser->next.tag == '#');
    next_token(parser);
    print_indent(parser);
    printf("annotation\n");
    parser->debug_indent += 4;
    ast_id tag = parse_id(parser);
    parser->debug_indent -= 4;
    return syntree_add_tag(&parser->syntree, AST_ANNOTATION, tag);
}

ast_id parse_block(parser_t* parser) {
    if (parser->next.tag != '[' &&
            parser->next.tag != '{') {
        syntax_error(parser, "'[' or '{'");
        return AST_INVALID_ID;
    }

    print_indent(parser);
    printf("block\n");

    parser->debug_indent += 4;

    if (parser->next.tag == '[')
        parse_capture(parser);

    if (parser->next.tag != '{') {
        syntax_error(parser, "{");
        return AST_INVALID_ID;
    }
    next_token(parser);

    while (parser->next.tag != '}') {
        if (parser->next.tag == TOKEN_T_KW_LET ||
                parser->next.tag == TOKEN_T_KW_FUNC ||
                parser->next.tag == TOKEN_T_KW_EXTERN ||
                parser->next.tag == TOKEN_T_KW_TYPE) {
            parse_declaration(parser);
        } 
        else {
            parse_statement(parser);
        }
    }

    parser->debug_indent -= 4;
    next_token(parser);

    return 1;
}

ast_id parse_capture(parser_t* parser) {
    assert(parser->next.tag == '[');
    next_token(parser);

    print_indent(parser);
    printf("capture");

    parser->debug_indent += 4;

    while (parser->next.tag != ']') {
        parse_id(parser);
        if (parser->next.tag != ',' && parser->next.tag != ']') {
            syntax_error(parser, "',' or ']'");
            return AST_INVALID_ID;
        }
    }

    parser->debug_indent -= 4;

    return 1;
}

ast_id parse_statement(parser_t* parser) {
    parser->debug_indent += 4;
    switch (parser->next.tag) {
        case TOKEN_T_KW_IF:
            parse_if_stmt(parser);
            break;
        case TOKEN_T_KW_FOR:
            parse_for_stmt(parser);
            break;
        case TOKEN_T_KW_WHILE:
            parse_while_stmt(parser);
            break;
        case TOKEN_T_KW_DO:
            parse_do_while_stmt(parser);
            break;
        case TOKEN_T_KW_DEFER:
            next_token(parser);
            printf("defer\n");
            parse_statement(parser);
            break;
        case TOKEN_T_KW_SWITCH:
            parse_switch_stmt(parser);
            break;
        case TOKEN_T_KW_RETURN:
            next_token(parser);
            print_indent(parser);
            printf("return\n");
            parser->debug_indent += 4;
            parse_expr(parser);
            parser->debug_indent -= 4;
            if (parser->next.tag != ';') {
                syntax_error(parser, ";");
                return AST_INVALID_ID;
            }
            next_token(parser);
            break;
        case ';':
            printf("Warning: Stray ';' at %s %d:%d\n",
                    parser->next.loc.file,
                    parser->next.loc.start_line,
                    parser->next.loc.start_column);
            if (parser->next.loc.start_line == parser->next.loc.end_line) {
                print_line_marker(parser->next.loc.file,
                        parser->next.loc.start_line,
                        parser->next.loc.start_column,
                        parser->next.loc.end_column);
            }
            next_token(parser);
            break;
        default:
            parse_assign(parser);
            if (parser->next.tag != ';') {
                syntax_error(parser, ";");
                return AST_INVALID_ID;
            }
            next_token(parser);
            break;
    }
    parser->debug_indent -= 4;
    return 1;
}

ast_id parse_if_stmt(parser_t* parser) {
    assert(parser->next.tag == TOKEN_T_KW_IF);
    next_token(parser);
    print_indent(parser);
    printf("if\n");
    parser->debug_indent += 4;

    parse_expr(parser);
    parse_block(parser);

    while (parser->next.tag == TOKEN_T_KW_ELSE) {
        next_token(parser);
        if (parser->next.tag == TOKEN_T_KW_IF) {
            next_token(parser);
            parse_expr(parser);
            parse_block(parser);
        }
        else if (parser->next.tag == '{' ||
                parser->next.tag == '[') {
            parse_block(parser);
            break; /* we are done. any else that follows is
                      either a syntax error, or an else beloging
                      to an outer if statement */
        }
    }

    parser->debug_indent -= 4;
    return 1;
}

ast_id parse_for_stmt(parser_t* parser) {
    /* (1) for ASSIGN ';' EXPR ';' EXPR BLOCK
     * (2) for VAR_DECL ';' EXPR ';' EXPR BLOCK
     * (3) for ASSIGN BLOCK
     * (4) for VAR_DECL BLOCK
     *
     * (3) and (4) are the iterator versions
     */
    assert(parser->next.tag == TOKEN_T_KW_FOR);
    next_token(parser);

    print_indent(parser);
    printf("for\n");

    parser->debug_indent += 4;

    ast_id init = (parser->next.tag == TOKEN_T_KW_LET) ?
        parse_variable_declaration(parser)
        : parse_assign(parser);
    if (!init)
        return AST_INVALID_ID;

    if (parser->next.tag == ';') {
        next_token(parser);
        ast_id cond = parse_expr(parser);
        if (!cond)
            return AST_INVALID_ID;
        if (parser->next.tag != ';') {
            syntax_error(parser, ";");
            return AST_INVALID_ID;
        }
        next_token(parser);
        parse_expr(parser);
    }

    parse_block(parser);

    parser->debug_indent -= 4;

    return 1;
}

ast_id parse_while_stmt(parser_t* parser) {
    assert(parser->next.tag == TOKEN_T_KW_WHILE);
    next_token(parser);

    print_indent(parser);
    printf("while\n");

    parser->debug_indent += 4;

    parse_expr(parser);
    parse_block(parser);

    parser->debug_indent -= 4;

    return 1;
}

ast_id parse_do_while_stmt(parser_t* parser) {
    assert(parser->next.tag == TOKEN_T_KW_DO);
    next_token(parser);

    print_indent(parser);
    printf("do\n");

    parser->debug_indent += 4;

    parse_block(parser);

    if (parser->next.tag != TOKEN_T_KW_WHILE) {
        syntax_error(parser, "while");
        return AST_INVALID_ID;
    }
    next_token(parser);

    parse_expr(parser);

    if (parser->next.tag != ';') {
        syntax_error(parser, ";");
        return AST_INVALID_ID;
    }
    next_token(parser);

    parser->debug_indent -= 4;

    return 1;
}

ast_id parse_switch_stmt(parser_t* parser) {
    assert(parser->next.tag == TOKEN_T_KW_SWITCH);
    next_token(parser);

    print_indent(parser);
    printf("switch\n");

    parser->debug_indent += 4;

    parse_expr(parser);
    if (parser->next.tag != '{') {
        syntax_error(parser, "{");
        return AST_INVALID_ID;
    }
    next_token(parser);

    while (parser->next.tag == TOKEN_T_KW_CASE) {
        next_token(parser);
        print_indent(parser);
        printf("case\n");
        parser->debug_indent += 4;

        parse_const_expr(parser);
        if (parser->next.tag != ':') {
            syntax_error(parser, ":");
            return AST_INVALID_ID;
        }
        next_token(parser);
        parse_block(parser);

        parser->debug_indent -= 4;
    }

    if (parser->next.tag == TOKEN_T_KW_DEFAULT) {
        next_token(parser);
        print_indent(parser);
        printf("default\n");
        if (parser->next.tag != ':') {
            syntax_error(parser, ":");
            return AST_INVALID_ID;
        }
        next_token(parser);
        parser->debug_indent += 4;
        parse_block(parser);
        parser->debug_indent -= 4;
    }

    if (parser->next.tag != '}') {
        syntax_error(parser, "}");
        return AST_INVALID_ID;
    }
    next_token(parser);

    parser->debug_indent -= 4;
    return 1;
}

ast_id parse_type(parser_t* parser) {
    switch (parser->next.tag) {
        case TOKEN_T_KW_STRUCT:
            return parse_struct_type(parser);
        case TOKEN_T_KW_UNION:
            return parse_union_type(parser);
        case TOKEN_T_KW_ENUM:
            return parse_enum_type(parser);
        case '[':
            return parse_array_type(parser);
        case '*':
            return parse_pointer_type(parser);
        case '(':
            return parse_func_type(parser);
        case TOKEN_T_ID:
            print_indent(parser);
            printf("some type\n");
            next_token(parser);
            return 1;
        default:
            syntax_error(parser, "type");
            return AST_INVALID_ID;
    }
    assert(!"Unreachable code reached");
}

ast_id parse_native_type(parser_t* parser);

ast_id parse_struct_type(parser_t* parser) {
    assert(parser->next.tag == TOKEN_T_KW_STRUCT);
    next_token(parser);

    print_indent(parser);
    printf("struct\n");

    if (parser->next.tag != '{') {
        syntax_error(parser, "{");
        return AST_INVALID_ID;
    }

    parser->debug_indent += 4;

    while (parser->next.tag != '}') {
        /* Struct fields */
        /* ID ':' type ',' */
        parse_id(parser);
        if (parser->next.tag != ':') {
            syntax_error(parser, ":");
            return AST_INVALID_ID;
        }
        parse_type(parser);

        if (parser->next.tag != ',') {
            syntax_error(parser, ",");
            return AST_INVALID_ID;
        }
        next_token(parser);
    }

    parser->debug_indent -= 4;
    return 1;
}

ast_id parse_union_type(parser_t* parser) {
    assert(parser->next.tag == TOKEN_T_KW_UNION);
    next_token(parser);

    print_indent(parser);
    printf("union\n");

    if (parser->next.tag != '{') {
        syntax_error(parser, "{");
        return AST_INVALID_ID;
    }

    parser->debug_indent += 4;

    while (parser->next.tag != '}') {
        /* Union fields */
        /* ID ':' type ',' */
        parse_id(parser);
        if (parser->next.tag != ':') {
            syntax_error(parser, ":");
            return AST_INVALID_ID;
        }
        parse_type(parser);

        if (parser->next.tag != ',') {
            syntax_error(parser, ",");
            return AST_INVALID_ID;
        }
        next_token(parser);
    }

    parser->debug_indent -= 4;
    return 1;
}

ast_id parse_enum_type(parser_t* parser) {
    assert(parser->next.tag == TOKEN_T_KW_ENUM);
    next_token(parser);

    print_indent(parser);
    printf("enum\n");

    if (parser->next.tag != '{') {
        syntax_error(parser, "{");
    }

    parser->debug_indent += 4;

    while (parser->next.tag != '}') {
        /* enum fields */
        /* ID ',' */
        parse_id(parser);
        if (parser->next.tag != ',') {
            syntax_error(parser, ",");
            return AST_INVALID_ID;
        }
        next_token(parser);
    }

    parser->debug_indent -= 4;
    return 1;
}

ast_id parse_func_type(parser_t* parser) {
    assert(parser->next.tag == '(');
    next_token(parser);

    print_indent(parser);
    printf("func type\n");

    parser->debug_indent += 4;

    /* param types
     * [ TYPE ( ',' TYPE )* ] [ ... ] */
    while (parser->next.tag != ')') {
        if (parser->next.tag == TOKEN_T_ELLIPSIS) {
            print_indent(parser);
            printf("ellipsis\n");
            next_token(parser);
            if (parser->next.tag != ')') {
                syntax_error(parser, ")");
                return AST_INVALID_ID;
            }
        } 
        else {
            parse_type(parser);
            if (parser->next.tag != ',' &&
                    parser->next.tag != ')') {
                syntax_error(parser, "',' or ')'");
                return AST_INVALID_ID;
            }
            if (parser->next.tag == ',')
                next_token(parser);
        }
    }
    next_token(parser);

    if (parser->next.tag != TOKEN_T_ARROW) {
        syntax_error(parser, "->");
        return AST_INVALID_ID;
    }

    /* Return types */
    parse_type(parser);
    while (parser->next.tag == ',')
        parse_type(parser);

    parser->debug_indent -= 4;
    return 1;
}

ast_id parse_array_type(parser_t* parser) {
    assert(parser->next.tag == '[');
    next_token(parser);

    if (parser->next.tag != ']') {
        syntax_error(parser, "]");
        return AST_INVALID_ID;
    }

    next_token(parser);

    print_indent(parser);
    printf("array\n");

    parser->debug_indent += 4;

    parse_type(parser);

    parser->debug_indent -= 4;
    return 1;
}

ast_id parse_pointer_type(parser_t* parser) {
    assert(parser->next.tag == '*');
    next_token(parser);

    print_indent(parser);
    printf("pointer\n");

    parser->debug_indent += 4;

    parse_type(parser);

    parser->debug_indent -= 4;
    return 1;
}
