#ifndef LIB_H
#define LIB_H

char *cmm_strdup(const char *src);

#define DEBUG
#define concat(x, y) x ## y

#ifdef DEBUG
#define LOG(s, ...) fprintf(stderr, "\033[31;1m" "[%s] " s "\033[0m\n", __func__, ## __VA_ARGS__)
#else
#define LOG(s, ...)
#endif

/* Our assigned task */
#define STRUCTURE

/* Some useful macro */
#define NEW(type) (type *)malloc(sizeof(type))
#define NEW_ARRAY(type, n) (type *)malloc(sizeof(x) * n)
#define CONCAT(x, y) x ## y
#endif /* LIB_H */
