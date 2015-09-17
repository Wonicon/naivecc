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

    int yycolumn = 1;
    #define YY_USER_ACTION\
    yylloc.first_line = yylloc.last_line = yylineno;\
    yylloc.first_column = yycolumn;\
    yylloc.last_column = yycolumn + yyleng - 1;\
    yycolumn += yyleng;
%}


%option yylineno
%option locations

digit [0-9]
letter [a-zA-Z]

ID ({letter}|_)({letter}|{digit}|_)*
INT 0|[1-9][0-9]*
OCT 0[0-9]+
HEX 0(x|X)({digit}|{letter})+

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

{HEX} {
    int i;
    // Jump over '0x'
    for (i = 2; i < yyleng; i++) {
        char ch = yytext[i];
        if ((ch > 'f' && ch <= 'z') || (ch > 'F' && ch <= 'Z')) {
            ErrorMsg("Illegal hexadecimal number", yytext, yylineno);
            return 0;
        }
    }
    LOG(HEX); return HEX;
}

{OCT} {
    int i;
    // all zero here is considered oct
    for (i = 0; i < yyleng; i++) {
        char ch = yytext[i];
        if (ch == '8' || ch == '9') {
            ErrorMsg("Illegal octal number", yytext, yylineno);
            return 0;
        }
    }
    LOG(OCT);
    return OCT;
}

{INT} { LOG(INT); return INT; }
{ID} { LOG(ID); return ID; }
{WS} {}

\n {
#ifdef DEBUG
    putchar('\n');
#endif
    yylineno++;
    yycolumn = 1;
}

. { ErrorMsg("Mysterious character", yytext, yylineno); return 0; }
%%
