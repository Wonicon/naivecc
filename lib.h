#ifndef LIB_H
#define LIB_H

enum ProductionTag {
    UNSIMPLIFIED,
    PROG_is_EXTDEF,
    EXTDEF_is_SPEC_EXTDEC,
    EXTDEF_is_SPEC,
    EXTDEF_is_SPEC_FUNC_COMPST,
    EXTDEC_is_VARDEC,
    SPEC_is_TYPE,
    SPEC_is_STRUCT,
    STRUCT_is_ID_DEF,
    STRUCT_is_DEF,
    STRUCT_is_ID,
    VARDEC_is_ID,
    VARDEC_is_VARDEC_SIZE,
    FUNC_is_ID_VAR,
    VAR_is_SPEC_VARDEC,
    COMPST_is_DEF_STMT,
    STMT_is_EXP,
    STMT_is_COMPST,
    STMT_is_RETURN,
    STMT_is_IF,
    STMT_is_IF_ELSE,
    STMT_is_WHILE,
    STMT_is_FOR,
    DEF_is_SPEC_DEC,
    DEC_is_VARDEC,
    DEC_is_VARDEC_INITIALIZATION,
    EXP_is_ASSIGN,
    EXP_is_AND,
    EXP_is_OR,
    EXP_is_NOT,
    EXP_is_RELOP,
    EXP_is_BINARY,
    EXP_is_UNARY,
    EXP_is_EXP,
    EXP_is_ID,
    EXP_is_ID_ARG,
    EXP_is_EXP_IDX,
    EXP_is_EXP_FIELD,
    EXP_is_INT,
    EXP_is_FLOAT,
    TERM_INT,
    TERM_FLOAT,
    TERM_ID
};

typedef enum _CmmType {
    CMM_INT,
    CMM_FLOAT,
    CMM_ARRAY,
    CMM_STRUCT,
    CMM_FUNC,
    CMM_FIELD,
    CMM_PARAM,
    CMM_TYPE,
    NR_CMM_TYPE
} CmmType;

typedef enum IR_Type {
    // 无效指令
    IR_NOP,
    IR_LABEL,
    IR_FUNC,

    // 运算类指令
    IR_ASSIGN,
    IR_ADD,
    IR_SUB,
    IR_MUL,
    IR_DIV,
    IR_ADDR,     // 寻址, 相当于 &x
    IR_DEREF_R,  // 获取 rs 指向的地址的值
    IR_DEREF_L,  // 写入 rd 指向的地址

    // 跳转类指令, 含义参考 MIPS
    IR_JMP,
    IR_BEQ,
    IR_BLT,
    IR_BLE,
    IR_BGT,
    IR_BGE,
    IR_BNE,
    IR_RET,

    // 内存类
    IR_DEC,

    // 函数类指令
    IR_ARG,      // 参数压栈
    IR_CALL,
    IR_PARAM,    // 参数声明

    // I/O类指令
    IR_READ,
    IR_WRITE,
    NR_IR_TYPE
} IR_Type;

typedef enum {
    OPE_NOT_USED,
    OPE_VAR,      // 变量
    OPE_REF,      // 引用类型, 包括数组和结构类型, 引用类型的首元素支持不解引用直接访问
    OPE_BOOL,     // 由于布尔值跨基本块却不能被优化掉, 所以开此特例
    OPE_TEMP,     // 编译器自行分配的临时变量
    OPE_ADDR,     // 地址值
    OPE_DEREF,    // 内联解引用
    OPE_REFADDR,  // REF的首元素分身
    OPE_INTEGER,
    OPE_FLOAT,
    OPE_LABEL,
    OPE_FUNC,
    NR_OPE_TYPE
} Ope_Type;

#define NR_OPE 3
#define RD_IDX 0
#define DISALIVE 0
#define ALIVE 1
#define NO_USE -1

#define OPE_TAB_SZ 4096

#define MAX_LINE 4096
#define NAME_LEN 120

char *cmm_strdup(const char *src);


typedef int bool;
#define true (1)
#define false (0)

#define concat(x, y) x ## y
#define str(x) # x

#define END         "\e[0m"
#define LOG_COLOR   "\e[38;5;046m"
#define PANIC_COLOR "\e[38;5;160m"
#define WARN_COLOR  "\e[38;5;166m"

#ifdef DEBUG
  #define LOG(s, ...) fprintf(stderr, LOG_COLOR "[%s] " s END "\n", __func__, ## __VA_ARGS__)
  #define PANIC(s, ...) do { fprintf(stderr, PANIC_COLOR "[%s] " s END "\n", __func__, ## __VA_ARGS__); assert(0); } while (0)
  #define WARN(s, ...) do { fprintf(stderr, WARN_COLOR "[%s] " s END "\n", __func__, ## __VA_ARGS__); } while (0)
  #define TEST(expr, s, ...) do { if (!(expr)) PANIC(s, ## __VA_ARGS__); } while (0)
#else   // ifndef DEBUG
  #define LOG(s, ...)
  #define PANIC(s, ...)
  #define WARN(s, ...)
  #define TEST(expr, s, ...)
#endif  // ifdef DEBUG

// Our assigned task
#define STRUCTURE

// Misc.
#define NEW(type) (type *)malloc(sizeof(type))

#define SWAP(x, y) ({ \
    typeof(x) tmp = x; \
    x = y; \
    y = tmp; \
})

#endif // LIB_H

