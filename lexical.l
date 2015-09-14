letter [a-zA-Z_]
digit [0-9]
%%
letter+ { printf("Find ID: %s\n", yytext); }
digit+ { printf("Find DIGITS: %d\n", atoi(yytext)); }
%%
