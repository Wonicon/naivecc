#ifndef LIB_H
#define LIB_H

char *cmm_strdup(const char *src);

typedef int bool;
#define true (1)
#define false (0)

#define DEBUG
#define concat(x, y) x ## y

#ifdef DEBUG
#define LOG(s, ...) fprintf(stderr, "\033[31;1m" "[%s] " s "\033[0m\n", __func__, ## __VA_ARGS__)
#define TEST(expr, s, ...) do {\
    if (!(expr)) {\
        LOG("Error" s, __VA_ARGS__);\
        assert((expr));\
    }\
} while (0);
#else
#define LOG(s, ...)
#define TEST(expr, s, ...)
#endif

/* Our assigned task */
#define STRUCTURE

/* Some useful macro */
#define NEW(type) (type *)malloc(sizeof(type))
#define NEW_ARRAY(type, n) (type *)malloc(sizeof(x) * n)
#define CONCAT(x, y) x ## y
#endif /* LIB_H */
