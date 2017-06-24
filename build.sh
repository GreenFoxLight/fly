#!/bin/sh

# compiler srcs
SRCS="compiler/main.c compiler/lexer.c compiler/parser.c compiler/parse_expr.c compiler/syntree.c"
gcc -o flyc -std=c11 -O2 -g -Wall -Wextra $SRCS
