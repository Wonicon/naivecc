## 2015-09-17

### yylineno

Just set `%option yylineno` does not provide me with an auto-inc `yylineno`.
Because this option is just aimed to be compatible with the old lex, so it is better to add a rule and add up `yylineno` by hand.

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

`%locations` and `YY_USER_ACTION` should be written in `syntax.y`, namely, the bison source file, and then include `lex.yy.c`!

