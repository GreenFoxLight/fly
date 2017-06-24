@echo off

mkdir build
pushd build

set CFLAGS=/nologo /I../compiler /Zi /DWIN32_BUILD
cl ..\compiler\main.c ..\compiler\lexer.c ..\compiler\parser.c ..\compiler\parse_expr.c /Feflyc.exe %CFLAGS%

popd
