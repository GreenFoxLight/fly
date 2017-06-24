#include "parser.h"
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
 * anyways ot decice which rule applies - just do
 * an assertion
 */

void syntax_error(parser_t* parser, const char* expected) {
    printf("Syntax error. Expected %s at: %s %d:%d\n",
            expected,
            parser->next.loc.file,
            parser->next.loc.start_line,
            parser->next.loc.start_column
          );

    if ((parser->next.loc.start_line == parser->next.loc.end_line) &&
            (parser->next.loc.start_column <= parser->next.loc.end_column)) {
        FILE* f = fopen(parser->next.loc.file, "r");
        if (!f)
            return;
        int line = parser->next.loc.start_line;
        for (int i = 1; i < line; i++) {
            while (fgetc(f) != '\n') {} // skip to next line
        }
        char c;
        while ((c = (char)fgetc(f)) != '\n') {
            printf("%c", c);
        }
        printf("\n");
        for (int i = 1; i < parser->next.loc.start_column; i++)
            printf(" ");
        for (int i = parser->next.loc.start_column;
                i < parser->next.loc.end_column;
                i++)
            printf("^");
        fclose(f);
        printf("\n");
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
    next_token(parser);
    parser->debug_indent = 0;
    return 1;
}

void release_parser(parser_t* parser) {
    lexer_release(&parser->lexer);
}

ast_id parse_program(parser_t* parser) {
    do {
        parser->debug_indent = 4;
        if (parser->next.tag == TOKEN_T_KW_LET ||
                parser->next.tag == TOKEN_T_KW_TYPE ||
                parser->next.tag == TOKEN_T_KW_FUNC ||
                parser->next.tag == TOKEN_T_KW_EXTERN)
            parse_declaration(parser);

        parser->debug_indent = 4;
        if (parser->next.tag == '#')
            parse_meta_instruction(parser);
    } while (parser->next.tag != TOKEN_T_EOF);

    return AST_INVALID_ID;
}


ast_id parse_id(parser_t* parser) {
    if (parser->next.tag != TOKEN_T_ID) {
        syntax_error(parser, "identifier");
        return AST_INVALID_ID;
    }
    print_indent(parser);
    printf("%s\n", parser->next.value.string);
    next_token(parser);

    return AST_INVALID_ID;
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

    return AST_INVALID_ID;
}

ast_id parse_declaration(parser_t* parser) {
    switch (parser->next.tag) {
        case TOKEN_T_KW_LET:
            parse_variable_declaration(parser);
            break;
        case TOKEN_T_KW_TYPE:
            parse_type_declaration(parser);
            break;
        case TOKEN_T_KW_FUNC:
            parse_func_declaration(parser);
            break;
        case TOKEN_T_KW_EXTERN:
            parse_extern_func_declaration(parser);
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
    return AST_INVALID_ID;
}

ast_id parse_func_declaration(parser_t* parser) {
    assert(parser->next.tag == TOKEN_T_KW_FUNC);
    next_token(parser);

    print_indent(parser);
    printf("fn\n");

    parser->debug_indent += 4;
    parse_id(parser);

    if (parser->next.tag != TOKEN_T_FUNC_DECL) {
        syntax_error(parser, "'::'");
        return AST_INVALID_ID;
    }
    next_token(parser);

    parse_function(parser);

    parser->debug_indent -= 4;

    return AST_INVALID_ID;
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
    parse_id(parser);

    next_token(parser);

    parse_func_type(parser);

    parser->debug_indent -= 4;

    return AST_INVALID_ID;
}

ast_id parse_variable_declaration(parser_t* parser) {
    assert(parser->next.tag == TOKEN_T_KW_LET);
    next_token(parser);

    print_indent(parser);
    printf("let\n");

    parser->debug_indent += 4;
    parse_id(parser);

    switch (parser->next.tag) {
        case ':':
            next_token(parser);
            if (parser->next.tag == TOKEN_T_KW_AUTO) {
                print_indent(parser);
                printf("auto\n");
            } else {
                parse_type(parser);
            }
            if (parser->next.tag == '=') {
                next_token(parser);
                parse_expr(parser);
            }
            break;
        case TOKEN_T_DECL_ASSIGN:
            next_token(parser);
            parse_expr(parser);
        default:
            syntax_error(parser, "':' or ':='");
            return AST_INVALID_ID;
    }

    parser->debug_indent -= 4;
    return AST_INVALID_ID;
}

ast_id parse_type_declaration(parser_t* parser) {
    assert(parser->next.tag == TOKEN_T_KW_TYPE);
    next_token(parser);

    print_indent(parser);
    printf("type\n");

    parser->debug_indent += 4;
    parse_id(parser);

    if (parser->next.tag != '=') {
        syntax_error(parser, "=");
        return AST_INVALID_ID;
    }
    next_token(parser);

    if (parser->next.tag == '#') {
        parse_annotation(parser);
    }
    parse_type(parser);

    parser->debug_indent -= 4;
    return AST_INVALID_ID;
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

    if (parser->next.tag == TOKEN_T_ID ||
            parser->next.tag == TOKEN_T_ELLIPSIS) {
        parse_func_params(parser);
    }

    switch (parser->next.tag) {
        case TOKEN_T_ARROW:
            next_token(parser);
            parse_type(parser);
            while (parser->next.tag == ',') {
                next_token(parser);
                parse_type(parser);
            }
            parse_block(parser);
            break;
        case TOKEN_T_BIG_ARROW:
            next_token(parser);
            parse_expr(parser);
            break;
        default:
            syntax_error(parser, "'->' or '=>'");
            return AST_INVALID_ID;
    }

    return AST_INVALID_ID;
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
    while (parser->next.tag == TOKEN_T_ID) {
        parse_id(parser);
        if (parser->next.tag != ':') {
            syntax_error(parser, ":");
            return AST_INVALID_ID;
        }
        next_token(parser);
        parse_type(parser);
        if (parser->next.tag != ',' &&
                parser->next.tag != ')') {
            syntax_error(parser, "',', ')' or '...'");
            return AST_INVALID_ID;
        }
        next_token(parser);
    }
    if (parser->next.tag == TOKEN_T_ELLIPSIS) {
        print_indent(parser);
        printf("...\n");
    }
    parser->debug_indent -= 4;

    return AST_INVALID_ID;
}

ast_id parse_annotation(parser_t* parser) {
    assert(parser->next.tag == '#');
    next_token(parser);
    print_indent(parser);
    printf("annotation\n");
    parser->debug_indent += 4;
    parse_id(parser);
    parser->debug_indent -= 4;
    return AST_INVALID_ID;
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
        } else {
            parse_statement(parser);
        }
    }

    parser->debug_indent -= 4;
    next_token(parser);

    return AST_INVALID_ID;
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

    return AST_INVALID_ID;
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
        case TOKEN_T_KW_RETURN:
            next_token(parser);
            printf("return\n");
            parse_expr(parser);
            if (parser->next.tag != ';') {
                syntax_error(parser, ";");
                return AST_INVALID_ID;
            }
            next_token(parser);
            break;
        default:
            parse_expr(parser);
            if (parser->next.tag != ';') {
                syntax_error(parser, ";");
                return AST_INVALID_ID;
            }
            next_token(parser);
            break;
    }
    parser->debug_indent -= 4;
    return AST_INVALID_ID;
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
            parse_block(parser);
        } else if (parser->next.tag == '{' ||
                parser->next.tag == '[') {
            parse_block(parser);
            break; /* we are done. any else that follows is
                    either a syntax error, or an else beloging
                    to an outer if statement */
        }
    }

    parser->debug_indent -= 4;
    return AST_INVALID_ID;
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

    return AST_INVALID_ID;
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

    return AST_INVALID_ID;
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

    parse_expr(parser);

    if (parser->next.tag != ';') {
        syntax_error(parser, ";");
        return AST_INVALID_ID;
    }
    next_token(parser);

    parser->debug_indent -= 4;

    return AST_INVALID_ID;
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
        parse_block(parser);

        parser->debug_indent -= 4;
    }

    if (parser->next.tag == TOKEN_T_KW_DEFAULT) {
        next_token(parser);
        print_indent(parser);
        printf("default\n");
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
    return AST_INVALID_ID;
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
            break;
        default:
            syntax_error(parser, "type");
            return AST_INVALID_ID;
    }
    return AST_INVALID_ID;
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
    return AST_INVALID_ID;
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
    return AST_INVALID_ID;
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
    return AST_INVALID_ID;
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
        } else {
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
    return AST_INVALID_ID;
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
    return AST_INVALID_ID;
}

ast_id parse_pointer_type(parser_t* parser) {
    assert(parser->next.tag == '*');
    next_token(parser);

    print_indent(parser);
    printf("pointer\n");

    parser->debug_indent += 4;

    parse_type(parser);

    parser->debug_indent -= 4;
    return AST_INVALID_ID;
}
