## 2015-09-17

Only `%option yylineno` does not work.
Because this option is just compatible with the old lex, so it is better to add a rule and add up `yylineno` by hand.

