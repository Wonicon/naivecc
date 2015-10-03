## 2015-09-17

### yylineno

Just set `%option yylineno` does not provide me with an auto-inc `yylineno`.
Because this option is just aimed to be compatible with the old lex, so it is better to add a rule and add up `yylineno` by hand.
But if combined with bison, it will have an auto-increase yylineno.

### fileno

While using `-std=c99` to compile `lex.yy.c`, I run into such a warning message:

```
./lex.yy.c: In function ‘yy_init_buffer’:
./lex.yy.c:1595:9: warning: implicit declaration of function ‘fileno’ [-Wimplicit-function-declaration]
         b->yy_is_interactive = file ? (isatty( fileno(file) ) > 0) : 0;
         ^
```

The `fileno` function seems a feature in POSIX system. Therefore, the pure C environment will consider it implicit. Using `-std=gnu99` will solve the problem.
But it is suggested not modifying the original commands in makefile, and I notice that the original compile command is using defualt `-std=c89`
with no for loop initialier and other benefits. If I using the default compile options, the `lex.yy.c` will finally come up with errors
when compiled to generate parser......

### parser

`%locations` and `YY_USER_ACTION` should be written in `lexical.l`, then the generated `lex.yy.c` can have this macro!

## 2015-09-18

### always syntax error

~~I forgot to add `HEX` and `OCT` in the production, and the newline seems having some problems.~~

The `HEX` and `OCT` is the pattern handled in lexicier which should return `INT` as its token type!

### syntax error at newline

Because the project folder was first opened under windows, we get '^M' at the end of line.

### YYDEBUG redifined

syntax.tab.c will define YYDEBUG in ifndef block, we must provide a YYDEBUG definition before it.
