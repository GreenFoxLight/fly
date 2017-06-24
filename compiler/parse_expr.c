#include "parser.h"
#include "lexer.h"
#include "fly.h"

#include <stdlib.h>
#include <assert.h>

/* In parser.c */
extern void syntax_error(parser_t* parser, const char* expected);
extern void print_indent(parser_t* parser);
extern void next_token(parser_t* parser);

typedef enum {
    LEFT,
    RIGHT,
    NONE
} op_assoc_t;

internal op_assoc_t get_operator_associativity(token_tag_t op) {
    switch (op) {
        case TOKEN_T_NOT:
        case TOKEN_T_DEC:
        case TOKEN_T_INC:
            return NONE;
        case TOKEN_T_ASSIGN:
            return RIGHT;
        default:
            /* we assume that only operators get passed in here */
            return LEFT;
    }
}

internal int get_operator_precedence(token_tag_t op) {
    switch (op) {
        case TOKEN_T_ADD:
        case TOKEN_T_SUB:
            return 10;
        case TOKEN_T_MUL:
        case TOKEN_T_DIV:
        case TOKEN_T_MOD:
            return 11;
        case TOKEN_T_SHIFT_LEFT:
        case TOKEN_T_SHIFT_RIGHT:
            return 9;
        case TOKEN_T_BITWISE_AND:
            return 5;
        case TOKEN_T_BITWISE_OR:
            return 4;
        case TOKEN_T_BITWISE_XOR:
            return 3;
        case TOKEN_T_BITWISE_NOT:
            return 12;
        case TOKEN_T_AND:
            return 2;
        case TOKEN_T_OR:
            return 1;
        case TOKEN_T_NOT:
        case TOKEN_T_INC:
        case TOKEN_T_DEC:
            return 12;
        case TOKEN_T_ASSIGN:
        case TOKEN_T_ADD_ASSIGN:
        case TOKEN_T_SUB_ASSIGN:
        case TOKEN_T_MUL_ASSIGN:
        case TOKEN_T_DIV_ASSIGN:
        case TOKEN_T_MOD_ASSIGN:
        case TOKEN_T_BITWISE_AND_ASSIGN:
        case TOKEN_T_BITWISE_OR_ASSIGN:
        case TOKEN_T_BITWISE_XOR_ASSIGN:
        case TOKEN_T_SHIFT_LEFT_ASSIGN:
        case TOKEN_T_SHIFT_RIGHT_ASSIGN:
            return 0;
        case TOKEN_T_EQUAL:
            return 6;
        case TOKEN_T_NOT_EQUAL:
            return 7;
        case TOKEN_T_LESS_EQUAL:
        case TOKEN_T_GREATER_EQUAL:
        case TOKEN_T_LANGLE:
        case TOKEN_T_RANGLE:
            return 8;
        default:
            return -1;
    }
}

typedef struct {
    ast_id ast;
    op_assoc_t assoc;
    int prec;
} operator_t;

typedef enum {
    UNKNOWN = 0,
    INT,
    UINT,
    INTL,
    UINTL,
    CHAR,
    BOOL,
    STRING,
    FLOAT32,
    FLOAT64
} native_type_t;

typedef struct {
    ast_id ast;
    native_type_t type;
} operand_t;

/* ******* Operator stack ********* */
typedef struct {
    operator_t* stack;
    int capacity;
    int top;
} operator_stack_t;

operator_stack_t operator_stack_init() {
    operator_stack_t stack = {
            .stack = NULL,
            .capacity = 0,
            .top = -1
    };
    return stack;
}

bool operator_stack_push(operator_stack_t* stack, operator_t op) {
    assert(stack);
    stack->top++;
    if (stack->top == stack->capacity) {
        stack->capacity += 10;
        operator_t* tmp = realloc(stack->stack, stack->capacity * sizeof(operator_t));
        if (!tmp) {
            stack->top--;
            stack->capacity -= 10;
            return false;
        }
        stack->stack = tmp;
    }
    stack->stack[stack->top] = op;
    return true;
}

operator_t operator_stack_pop(operator_stack_t* stack) {
    assert(stack);
    assert(stack->top > -1);
    return stack->stack[stack->top--];
}

void operator_stack_release(operator_stack_t* stack) {
    assert(stack);
    free(stack->stack);
    stack->capacity = 0;
    stack->top = 0;
}

/* ********** Operand Stack *********** */
typedef struct {
    operand_t* stack;
    int capacity;
    int top;
} operand_stack_t;

operand_stack_t operand_stack_init() {
    operand_stack_t stack = {
            .stack = NULL,
            .capacity = 0,
            .top = -1
    };
    return stack;
}

bool operand_stack_push(operand_stack_t* stack, operand_t op) {
    assert(stack);
    stack->top++;
    if (stack->top == stack->capacity) {
        stack->capacity += 10;
        operand_t* tmp = realloc(stack->stack, stack->capacity * sizeof(operand_t));
        if (!tmp) {
            stack->top--;
            stack->capacity -= 10;
            return false;
        }
        stack->stack = tmp;
    }
    stack->stack[stack->top] = op;
    return true;
}

operand_t operand_stack_pop(operand_stack_t* stack) {
    assert(stack);
    assert(stack->top > -1);
    return stack->stack[stack->top--];
}

void operand_stack_release(operand_stack_t* stack) {
    assert(stack);
    free(stack->stack);
    stack->capacity = 0;
    stack->top = 0;
}

/* Expressions */

internal int is_next_infix_op(parser_t* parser) {
    switch (parser->next.tag) {
        case TOKEN_T_ADD:
        case TOKEN_T_SUB:
        case TOKEN_T_MUL:
        case TOKEN_T_DIV:
        case TOKEN_T_MOD:
        case TOKEN_T_BITWISE_AND:
        case TOKEN_T_BITWISE_OR:
        case TOKEN_T_BITWISE_XOR:
        case TOKEN_T_SHIFT_LEFT:
        case TOKEN_T_SHIFT_RIGHT:
        case TOKEN_T_AND:
        case TOKEN_T_OR:
        case TOKEN_T_EQUAL:
        case TOKEN_T_NOT_EQUAL:
        case TOKEN_T_LESS_EQUAL:
        case TOKEN_T_GREATER_EQUAL:
        case TOKEN_T_LANGLE:
        case TOKEN_T_RANGLE:
            return 1;
        default:
            return 0;
    }
}

internal ast_id parse_infix_operator(parser_t* parser) {
    switch (parser->next.tag) {
        case TOKEN_T_ADD:
        case TOKEN_T_SUB:
        case TOKEN_T_MUL:
        case TOKEN_T_DIV:
        case TOKEN_T_MOD:
        case TOKEN_T_BITWISE_AND:
        case TOKEN_T_BITWISE_OR:
        case TOKEN_T_BITWISE_XOR:
        case TOKEN_T_SHIFT_LEFT:
        case TOKEN_T_SHIFT_RIGHT:
        case TOKEN_T_AND:
        case TOKEN_T_OR:
        /* case TOKEN_T_ASSIGN:
        case TOKEN_T_ADD_ASSIGN:
        case TOKEN_T_SUB_ASSIGN:
        case TOKEN_T_MUL_ASSIGN:
        case TOKEN_T_DIV_ASSIGN:
        case TOKEN_T_MOD_ASSIGN:
        case TOKEN_T_BITWISE_AND_ASSIGN:
        case TOKEN_T_BITWISE_OR_ASSIGN:
        case TOKEN_T_BITWISE_XOR_ASSIGN:
        case TOKEN_T_SHIFT_LEFT_ASSIGN:
        case TOKEN_T_SHIFT_RIGHT_ASSIGN: */
        case TOKEN_T_EQUAL:
        case TOKEN_T_NOT_EQUAL:
        case TOKEN_T_LESS_EQUAL:
        case TOKEN_T_GREATER_EQUAL:
        case TOKEN_T_LANGLE:
        case TOKEN_T_RANGLE:
            next_token(parser);
            return 1; // we need to differentiate between success and failure.
        default:
            syntax_error(parser, "infix operator");
            break;
    }
    return AST_INVALID_ID;
}


internal ast_id parse_const_simple_operand(parser_t* parser);

internal ast_id parse_simple_operand(parser_t* parser) {
    /* simple operands are either constants or identifier (which are ids or .) */
    if (parser->next.tag == TOKEN_T_ID) {
        parse_id(parser);
        while (parser->next.tag == '.') {
            next_token(parser);
            print_indent(parser);
            printf(".\n");
            parse_id(parser);
        }
        return 1;
    } else
        return parse_const_simple_operand(parser);

}

internal ast_id parse_prefix_expr(parser_t* parser) {
    if (parser->next.tag != TOKEN_T_NOT &&
            parser->next.tag != TOKEN_T_INC &&
            parser->next.tag != TOKEN_T_DEC &&
            parser->next.tag != '~') {
        return AST_INVALID_ID;
    }
    next_token(parser);
    print_indent(parser);
    printf("prefix_exp\n");
    parser->debug_indent += 4;
    parse_expr(parser);
    parser->debug_indent -= 4;
    return 1;
}

#define UNUSED(x)((void)x)

internal ast_id parse_call_operator(parser_t* parser) {
    assert(parser->next.tag == '(');
    next_token(parser);
    
    print_indent(parser);
    printf("call\n");
    parser->debug_indent += 4;

    while (parser->next.tag != ')') {
        parse_expr(parser);
        if (parser->next.tag != ',' && parser->next.tag != ')') {
            syntax_error(parser, "',' or ')'");
            return AST_INVALID_ID;
        }
        if (parser->next.tag == ',')
            next_token(parser);
    }
    next_token(parser);
    parser->debug_indent -= 4;
    
    return 1;
}

internal ast_id parse_infix_expr(parser_t* parser) {
    operand_stack_t operand_stack = operand_stack_init();
    operator_stack_t operator_stack = operator_stack_init();

    { /* parse first operand */
        ast_id first_operand;
        if (parser->next.tag == '!' ||
                parser->next.tag == TOKEN_T_INC ||
                parser->next.tag == TOKEN_T_DEC ||
                parser->next.tag == '~') {
            first_operand = parse_prefix_expr(parser);
        } else {
            first_operand = parse_simple_operand(parser);
        }
        if (parser->next.tag == '(') {
            /* call operator */
            parse_call_operator(parser);
        }
        if (parser->next.tag == '[') {
            /* array access operator */
            next_token(parser);
            parser->debug_indent += 4;
            parse_expr(parser);
            parser->debug_indent -= 4;
            if (parser->next.tag != ']') {
                syntax_error(parser, "]");
                return AST_INVALID_ID;
            }
            next_token(parser);
        }
        if (parser->next.tag == TOKEN_T_INC ||
                parser->next.tag == TOKEN_T_DEC) {
            /* we have a postfix operator */
            next_token(parser);
        }

        operand_t op = { .ast = first_operand };
        operand_stack_push(&operand_stack, op);
    }
    if (!is_next_infix_op(parser)) {
        /* done. */
        return 1;
    }

    while (true) {
        // Looking from the last operand, we expect to see an infix operator
        // and an operand, which is either an expression in braces,
        // a normal operand or a prefix expression.
        // This is optionally followed by a postfix operator.
        if (!is_next_infix_op(parser))
            break; // done

        token_tag_t infix_tag = parser->next.tag;
        ast_id infix_op = parse_infix_operator(parser);

        ast_id operand = AST_INVALID_ID;
        if (parser->next.tag == '(') {
            next_token(parser);
            parser->debug_indent += 4;
            operand = parse_expr(parser);
            parser->debug_indent -= 4;
        } else if (parser->next.tag == '!' ||
                parser->next.tag == '~' ||
                parser->next.tag == TOKEN_T_INC ||
                parser->next.tag == TOKEN_T_DEC) {
            operand = parse_prefix_expr(parser);
        } else {
            operand = parse_simple_operand(parser);
        }

        if (!operand) {
            operand_stack_release(&operand_stack);
            operator_stack_release(&operator_stack);
            printf("!");
            syntax_error(parser, "operand");
            return AST_INVALID_ID;
        }

		if (parser->next.tag == '(') {
			/* call operator */
			parse_call_operator(parser);
		}
		if (parser->next.tag == '[') {
			/* array access operator */
			next_token(parser);
			parser->debug_indent += 4;
			parse_expr(parser);
			parser->debug_indent -= 4;
			if (parser->next.tag != ']') {
				syntax_error(parser, "]");
				return AST_INVALID_ID;
			}
			next_token(parser);
		}
        if (parser->next.tag == TOKEN_T_INC ||
                parser->next.tag == TOKEN_T_DEC) {
            /* postfix op */
            next_token(parser);
        }

        int top_prec, new_prec;
        top_prec = (operator_stack.top > -1) ?
            operator_stack.stack[operator_stack.top].prec : -1;
        new_prec = get_operator_precedence(infix_tag);
        op_assoc_t assoc = get_operator_associativity(infix_tag);
        while (new_prec <= top_prec) {
            if ((new_prec == top_prec) && (assoc == RIGHT)) {
                // evaluate from right to left, which is done below or in
                // a later iteration
                break;
            }
            operator_t top_op = operator_stack_pop(&operator_stack);
            assert(operand_stack.top >= 1);
            operand_t right = operand_stack_pop(&operand_stack);
            operand_t left = operand_stack_pop(&operand_stack);
            UNUSED(top_op);
            UNUSED(right);
            UNUSED(left);

            /* create a new syntree entry */
            operand_t result = { .ast = 42, .type = 0 };
            operand_stack_push(&operand_stack, result);

            top_prec = (operator_stack.top > -1) ?
                operator_stack.stack[operator_stack.top].prec : -1;
        }
        operator_t new_op = {
            .ast = infix_op,
            .assoc = assoc,
            .prec = new_prec
        };
        operand_t new_operand = { operand, 0 };
        operator_stack_push(&operator_stack, new_op);
        operand_stack_push(&operand_stack, new_operand);
    }

    while (operator_stack.top > -1) {
        operator_t top_operator = operator_stack_pop(&operator_stack);
        assert(operand_stack.top >= 1);

        // NOTE(Kevin): Verify that these are in the right order
        operand_t left = operand_stack_pop(&operand_stack);
        operand_t right = operand_stack_pop(&operand_stack);

        UNUSED(top_operator);
        UNUSED(left);
        UNUSED(right);

        /* TODO: create new operand */
        operand_t new_operand = { 42, 0 };
        operand_stack_push(&operand_stack, new_operand);
    }
    assert(operand_stack.top == 0);
    ast_id expr = operand_stack_pop(&operand_stack).ast;

    print_indent(parser);
    printf("expr\n");

    if (parser->next.tag == '(') {
        /* call operator */
        next_token(parser);
    }
    if (parser->next.tag == '[') {
        /* array access operator */
        next_token(parser);
        parser->debug_indent += 4;
        parse_expr(parser);
        parser->debug_indent -= 4;
        if (parser->next.tag != ']') {
            syntax_error(parser, "]");
            return AST_INVALID_ID;
        }
        next_token(parser);
    }

    if (parser->next.tag == TOKEN_T_INC ||
            parser->next.tag == TOKEN_T_DEC) {
        /* we have a postfix operator */
        next_token(parser);
    }



    operator_stack_release(&operator_stack);
    operand_stack_release(&operand_stack);
    return expr;
}

ast_id parse_expr(parser_t* parser) {
    /* This could be a cast or an arithmetic expression. */
    if (parser->next.tag == TOKEN_T_KW_CAST)
        return parse_cast(parser);
    else
        return parse_infix_expr(parser);
}

ast_id parse_cast(parser_t* parser) {
    assert(parser->next.tag == TOKEN_T_KW_CAST);
    next_token(parser);

    print_indent(parser);
    printf("cast\n");

    parser->debug_indent += 4;
    if (parser->next.tag != '<') {
        syntax_error(parser, "<");
        return AST_INVALID_ID;
    }
    next_token(parser);

    parse_type(parser);
    if (parser->next.tag != '>') {
        syntax_error(parser, ">");
        return AST_INVALID_ID;
    }
    next_token(parser);

    if (parser->next.tag != '(') {
        syntax_error(parser, "(");
        return AST_INVALID_ID;
    }
    next_token(parser);

    parse_expr(parser);

    if (parser->next.tag != ')') {
        syntax_error(parser, ")");
        return AST_INVALID_ID;
    }
    next_token(parser);

    parser->debug_indent -= 4;
    return 1;
}

ast_id parse_assign(parser_t* parser) {
    ast_id expr = parse_expr(parser);
    while (parser->next.tag == ',' ) {
        parse_expr(parser);
    }
    if (parser->next.tag != TOKEN_T_ASSIGN &&
            parser->next.tag != TOKEN_T_ADD_ASSIGN &&
            parser->next.tag != TOKEN_T_SUB_ASSIGN &&
            parser->next.tag != TOKEN_T_MUL_ASSIGN &&
            parser->next.tag != TOKEN_T_DIV_ASSIGN &&
            parser->next.tag != TOKEN_T_MOD_ASSIGN &&
            parser->next.tag != TOKEN_T_BITWISE_AND_ASSIGN &&
            parser->next.tag != TOKEN_T_BITWISE_OR_ASSIGN &&
            parser->next.tag != TOKEN_T_BITWISE_XOR_ASSIGN &&
            parser->next.tag != TOKEN_T_SHIFT_LEFT_ASSIGN &&
            parser->next.tag != TOKEN_T_SHIFT_RIGHT_ASSIGN) {
        return expr;
    }
    print_indent(parser);
    printf("assign\n");
    next_token(parser);
    parse_assign(parser);
    return 1;
}

/* Const expressions */

internal ast_id parse_const_simple_operand(parser_t* parser) {
    if (parser->next.tag == TOKEN_T_INT ||
            parser->next.tag == TOKEN_T_UINT ||
            parser->next.tag == TOKEN_T_INTL ||
            parser->next.tag == TOKEN_T_UINTL ||
            parser->next.tag == TOKEN_T_FLOAT32 ||
            parser->next.tag == TOKEN_T_FLOAT64 ||
            parser->next.tag == TOKEN_T_CHAR ||
            parser->next.tag == TOKEN_T_BOOL ||
            parser->next.tag == TOKEN_T_STRING) {
        next_token(parser);
        return 1;
    } else {
        syntax_error(parser, "constant");
        return AST_INVALID_ID;
    }
}

internal ast_id parse_const_prefix_expr(parser_t* parser) {
    if (parser->next.tag != TOKEN_T_NOT &&
            parser->next.tag != TOKEN_T_INC &&
            parser->next.tag != TOKEN_T_DEC &&
            parser->next.tag != '~') {
        return AST_INVALID_ID;
    }
    next_token(parser);
    print_indent(parser);
    printf("const prefix_exp\n");
    parser->debug_indent += 4;
    print_indent(parser);
    parse_const_expr(parser);
    parser->debug_indent -= 4;
    return 1;
}

internal ast_id parse_const_infix_expr(parser_t* parser) {
    operand_stack_t operand_stack = operand_stack_init();
    operator_stack_t operator_stack = operator_stack_init();

    { /* parse first operand */
        ast_id first_operand;
        if (parser->next.tag == '!' ||
                parser->next.tag == TOKEN_T_INC ||
                parser->next.tag == TOKEN_T_DEC ||
                parser->next.tag == '~') {
            first_operand = parse_const_prefix_expr(parser);
        } else {
            first_operand = parse_const_simple_operand(parser);
        }

        if (parser->next.tag == TOKEN_T_INC ||
                parser->next.tag == TOKEN_T_DEC) {
            /* we have a postfix operator */
            next_token(parser);
        }

        operand_t op = { .ast = first_operand };
        operand_stack_push(&operand_stack, op);
    }
    if (!is_next_infix_op(parser)) {
        /* done */
        return 1;
    }

    while (true) {
        // Looking from the last operand, we expect to see an infix operator
        // and an operand, which is either an expression in braces,
        // a normal operand or a prefix expression.
        // This is optionally followed by a postfix operator.
        if (!is_next_infix_op(parser))
            break; // done

        token_tag_t infix_tag = parser->next.tag;
        ast_id infix_op = parse_infix_operator(parser);

        ast_id operand = AST_INVALID_ID;
        if (parser->next.tag == '(') {
            next_token(parser);
            parser->debug_indent += 4;
            operand = parse_const_expr(parser);
            parser->debug_indent -= 4;
        } else if (parser->next.tag == '!' ||
                parser->next.tag == '~' ||
                parser->next.tag == TOKEN_T_INC ||
                parser->next.tag == TOKEN_T_DEC) {
            operand = parse_const_prefix_expr(parser);
        } else {
            operand = parse_const_simple_operand(parser);
        }

        if (!operand) {
            operand_stack_release(&operand_stack);
            operator_stack_release(&operator_stack);
            syntax_error(parser, "operand");
            return AST_INVALID_ID;
        }

        if (parser->next.tag == TOKEN_T_INC ||
                parser->next.tag == TOKEN_T_DEC) {
            /* postfix op */
            next_token(parser);
        }

        int top_prec, new_prec;
        top_prec = (operator_stack.top > -1) ?
            operator_stack.stack[operator_stack.top].prec : -1;
        new_prec = get_operator_precedence(infix_tag);
        op_assoc_t assoc = get_operator_associativity(infix_tag);
        while (new_prec <= top_prec) {
            if ((new_prec == top_prec) && (assoc == RIGHT)) {
                // evaluate from right to left, which is done below or in
                // a later iteration
                break;
            }
            operator_t top_op = operator_stack_pop(&operator_stack);
            assert(operand_stack.top >= 1);
            operand_t right = operand_stack_pop(&operand_stack);
            operand_t left = operand_stack_pop(&operand_stack);
            UNUSED(top_op);
            UNUSED(right);
            UNUSED(left);

            /* We would evaluate the expression... */
            operand_t result = { .ast = 42, .type = 0 };
            operand_stack_push(&operand_stack, result);

            top_prec = (operator_stack.top > -1) ?
                operator_stack.stack[operator_stack.top].prec : -1;
        }
        operator_t new_op = {
            .ast = infix_op,
            .assoc = assoc,
            .prec = new_prec
        };
        operand_t new_operand = { operand, 0 };
        operator_stack_push(&operator_stack, new_op);
        operand_stack_push(&operand_stack, new_operand);
    }

    while (operator_stack.top > -1) {
        operator_t top_operator = operator_stack_pop(&operator_stack);
        assert(operand_stack.top >= 1);

        // NOTE(Kevin): Verify that these are in the right order
        operand_t left = operand_stack_pop(&operand_stack);
        operand_t right = operand_stack_pop(&operand_stack);

        UNUSED(top_operator);
        UNUSED(left);
        UNUSED(right);

        /* TODO: create new operand */
        operand_t new_operand = { 42, 0 };
        operand_stack_push(&operand_stack, new_operand);
    }
    assert(operand_stack.top == 0);
    ast_id expr = operand_stack_pop(&operand_stack).ast;

    print_indent(parser);
    printf("const expr\n");

    operator_stack_release(&operator_stack);
    operand_stack_release(&operand_stack);
    return expr;

}

ast_id parse_const_expr(parser_t* parser) {
    if (parser->next.tag == TOKEN_T_KW_CAST) {
        return parse_const_cast(parser);
    }
    return parse_const_infix_expr(parser);
}

ast_id parse_const_cast(parser_t* parser) {
    assert(parser->next.tag == TOKEN_T_KW_CAST);
    next_token(parser);

    print_indent(parser);
    printf("cast\n");

    parser->debug_indent += 4;
    if (parser->next.tag != '<') {
        syntax_error(parser, "<");
        return AST_INVALID_ID;
    }
    next_token(parser);

    parse_type(parser);
    if (parser->next.tag != '>') {
        syntax_error(parser, ">");
        return AST_INVALID_ID;
    }
    next_token(parser);

    if (parser->next.tag != '(') {
        syntax_error(parser, "(");
        return AST_INVALID_ID;
    }
    next_token(parser);

    parse_const_expr(parser);

    if (parser->next.tag != ')') {
        syntax_error(parser, ")");
        return AST_INVALID_ID;
    }
    next_token(parser);

    parser->debug_indent -= 4;
    return AST_INVALID_ID;

}
