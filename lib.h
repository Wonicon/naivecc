#ifndef LIB_H
#define LIB_H

char *cmm_strdup(const char *src);

#define concat(x, y) x ## y

#ifdef DEBUG
#define LOG(s, ...) fprintf(stderr, "[%s] " s "\n", __func__, ## __VA_ARGS__)
#else
#define LOG(s, ...)
#endif

/* Our assigned task */
#define STRUCTURE

/* Some useful macro */
#define NEW(type) (type *)malloc(sizeof(type))
#define NEW_ARRAY(type, n) (type *)malloc(sizeof(x) * n)
#endif /* LIB_H */
