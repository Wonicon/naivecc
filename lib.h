#ifndef LIB_H
#define LIB_H

char *cmm_strdup(const char *src);

typedef int bool;
#define true (1)
#define false (0)

#define DEBUG
#define INLINE_REPLACE
#define concat(x, y) x ## y

#define END         "\e[0m"
#define LOG_COLOR   "\e[38;5;046m"
#define PANIC_COLOR "\e[38;5;160m"
#define WARN_COLOR  "\e[38;5;166m"

#ifdef DEBUG
  #define LOG(s, ...) fprintf(stderr, LOG_COLOR "[%s] " s END "\n", __func__, ## __VA_ARGS__)
  #define PANIC(s, ...) do { fprintf(stderr, PANIC_COLOR "[%s] " s END "\n", __func__, ## __VA_ARGS__); assert(0); } while (0)
  #define WARN(s, ...) do { fprintf(stderr, WARN_COLOR "[%s] " s END "\n", __func__, ## __VA_ARGS__); } while (0)
  #define TEST(expr, s, ...) do { if (!(expr)) PANIC(s, __VA_ARGS__); } while (0)
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
#endif // LIB_H
