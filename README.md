# Naive C-like Compiler

The code is derived from my compiler lab code in school.
Since the lab only covers some basic topics of a compiler, and my implementation is dirty and naive,
I want to keep polishing my code and present my work here.

## Supported Syntax

Some significant limitations are shown below:

1. No scope, all names should be distinguished;
1. No declaration, all functions should be defined before called;
1. No global variables;
1. No struct;
1. No comments;
1. No preprocessor;
1. Basic type only consists of int;
1. All local variables should be defined at the beginning of a function.

The top level of a program should look like this:

```c
int global_variable;

int echo(int e)
{
    return e;
}

int main()
{
    /* Local definitions, like follows */

    int foo;
    int bar = 3;
    int vec[2];

    /* Statements, like follows */

    if (/* Expression */) {
        /* Statements */
    }
    else if (/* Expression */) {
        /* Statements */
    }
    else {
        /* Statements */
    }

    while (/* Expression */) {
        /* Statements */
    }

    foo = bar;
    foo + 1;    // +, -, *, /
    foo < bar;  // <, <=, >, >=, ==, !=
    !foo;       // -, !
    (foo);
    echo(vec[0]);

    return 0;
}

