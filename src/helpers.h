#include <err.h>

#define __todo__0_ARG()                                                        \
  do {                                                                         \
    err(EXIT_FAILURE, "TODO: not yet implemented (%s:%d:%s)\n", __FILE__,      \
            __LINE__, __func__);                                               \
  } while (0)

#define __todo__1_ARG(msg)                                                     \
  do {                                                                         \
    err(EXIT_FAILURE, "TODO: %s (%s:%d:%s)\n", msg, __FILE__, __LINE__,        \
            __func__);                                                         \
  } while (0)

#define GET_2ND_ARG(_0, arg1, arg2, ...) arg2

#define __todo_MACRO_CHOOSER(...)                                              \
  GET_2ND_ARG(_0, ##__VA_ARGS__, __todo__1_ARG, __todo__0_ARG, )

#define todo(...) __todo_MACRO_CHOOSER(__VA_ARGS__)(__VA_ARGS__)

#ifdef DEBUG
#define dbg(x)                                                                 \
  ({                                                                           \
    typeof(x) y = (x);                                                         \
    fprintf(stderr, "%s:%d:%s: %s = %d\n", __FILE__, __LINE__, __func__, #x,   \
            y);                                                                \
    y;                                                                         \
  })
#else
#define dbg(x) (x)
#endif
