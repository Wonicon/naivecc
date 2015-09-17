%{
#define DEBUG
#include "token.h"
#include "debug.h"
%}

digit [0-9]
letter [a-zA-Z]
number {digit}+
id ({letter}|_)({letter}|{digit}|_)*
int 0+|[1-9][0-9]*
oct 0[1-7][0-7]*
hex 0(x|X)[0-9a-fA-F]+
%%
{hex} { printf("HEX"); }
{int} { printf("INT"); }
{oct} { printf("OCT"); }
{id} { printf("ID"); }
. { printf("Unknow"); }
%%
