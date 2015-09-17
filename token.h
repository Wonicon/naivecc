#ifndef __TOKEN_H__
#define __TOKEN_H__

#ifdef DEBUG
enum TOKEN_TYPE
{
    INT = 1,   // A sequence of digits without spaces

    FLOAT,     // A real number consisting of digits and one 
               // decimal point. The decimal point must be
               // surronded by at least one digit

    ID,        // A character string consisting of 52 upper-
               // or lower-case alphabetic, 10 numeric and one
               // underscore characters. Besides, an identifier
               // must not start with a digit

    SEMI,      // ;
    COMMA,     // ,
    ASSIGNOP,  // =
    RELOP,     // > | < | >= | <= | == | !=
    PLUS,      // +
    MINUS,     // -
    STAR,      // *
    DIV,       // /
    AND,       // &&
    OR,        // ||
    DOT,       // .
    NOT,       // !
    TYPE,      // int | float
    LP,        // (
    RP,        // )
    LB,        // [
    RB,        // ]
    LC,        // {
    RC,        // }
    STRUCT,    // struct
    RETURN,    // return
    IF,        // if
    ELSE,      // else
    WHILE,     // while
    OCT,       // oct number satrts with 0
    HEX,       // hex number starts with 0x | 0X
};
#endif

static inline void ErrorMsg(char *msg, int lineno)
{
#ifndef DEBUG
#define FD stderr
#else
#define FD stdout
#endif
    fprintf(FD, "Error type A at Line %d: Mysterious character \"%s\"",
            lineno, msg);
#undef FD
}

#endif
