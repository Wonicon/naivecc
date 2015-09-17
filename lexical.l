%{
#ifndef DEBUG
// This header file will generate with default `make'
// The sheer lexical analyzer does not need it.
// The DEBUG macro should be defined in compile command
#include "syntax.tab.h"
#else
#include "token.h"
#include "debug.h"
#endif
%}

%option yylineno

digit [0-9]
letter [a-zA-Z]
number {digit}+

ID ({letter}|_)({letter}|{digit}|_)*
INT 0+|[1-9][0-9]*
OCT 0[1-7][0-7]*
HEX 0(x|X)[0-9a-fA-F]+

SEMI ;
COMMA ,
ASSIGNOP =
RELOP >|<|>=|<=|==|!=
PLUS \+
MINUS -
STAR \*
DIV \/
AND &&
OR \|\|
DOT \.
NOT !
TYPE int|float
LP \(
RP \)
LB \[
RB \]
LC \{
RC \}
STRUCT struct
RETURN return
IF if
ELSE else
WHILE while
WS [[:blank:]]
%%
{SEMI} { LOG(SEMI); return SEMI; }
{COMMA} { LOG(COMMA); return COMMA; }
{ASSIGNOP} { LOG(ASSIGNOP); return ASSIGNOP; }
{RELOP} { LOG(RELOP); return RELOP; }
{PLUS} { LOG(PLUS); return PLUS; }
{MINUS} { LOG(MINUS); return MINUS; }
{STAR} { LOG(STAR); return STAR; }
{DIV} { LOG(DIV); return DIV; }
{AND} { LOG(AND); return AND; }
{OR} { LOG(OR); return OR; }
{DOT} { LOG(DOT); return DOT; }
{NOT} { LOG(NOT); return NOT; }
{TYPE} { LOG(TYPE); return TYPE; }
{LP} { LOG(LP); return LP; }
{RP} { LOG(RP); return RP; }
{LB} { LOG(LB); return LB; }
{RB} { LOG(RB); return RB; }
{LC} { LOG(LC); return LC; }
{RC} { LOG(RC); return RC; }
{STRUCT} { LOG(STRUCT); return STRUCT; }
{RETURN} { LOG(RETURN); return RETURN; }
{IF} { LOG(IF); return IF; }
{ELSE} { LOG(ELSE); return ELSE; }
{WHILE} { LOG(WHILE); return WHILE; }
{HEX} { LOG(HEX); return HEX; }
{OCT} { LOG(OCT); return OCT; }
{INT} { LOG(INT); return INT; }
{ID} { LOG(ID); return ID; }
{WS} {}
\n { LOG(\n); yylineno++; }
. { ErrorMsg(yytext, yylineno); exit(-1); }
%%
