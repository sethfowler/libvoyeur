#ifndef LIBVOYEUR_UTIL_H
#define LIBVOYEUR_UTIL_H

#include <stdio.h>

#define RETURN_ON_FAIL(_f, ...)                 \
  do {                                          \
    if (_f(__VA_ARGS__) < 0) {                  \
      perror(#_f);                              \
      return;                                   \
    }                                           \
  } while (0)

#define RETURN_ERROR_ON_FAIL(_f, ...)           \
  do {                                          \
    if (_f(__VA_ARGS__) < 0) {                  \
      perror(#_f);                              \
      return -1;                                \
    }                                           \
  } while (0)

#define ABORT_ON_FAIL(_f, ...)                  \
  do {                                          \
    if (_f(__VA_ARGS__) < 0) {                  \
      perror(#_f);                              \
      exit(EXIT_FAILURE);                       \
    }                                           \
  } while (0)

#ifdef DEBUG

#  define WARN_ON_FAIL(_f, ...)                 \
  do {                                          \
    if (_f(__VA_ARGS__) < 0) {                  \
      perror(#_f);                              \
    }                                           \
  } while (0)

#  define WARN_ON_FAIL_VALUE(_var, _err)        \
  do {                                          \
    if (_var < 0) {                             \
      perror(_err);                             \
      exit(EXIT_FAILURE);                       \
    }                                           \
  } while (0)

#  define ASSERT(_check, ...)                   \
  do {                                          \
    if (!(_check)) {                             \
      fprintf(stderr, __VA_ARGS__);             \
      exit(EXIT_FAILURE);                       \
    }                                           \
  } while (0)

#else

#  define WARN_ON_FAIL(...)
#  define WARN_ON_FAIL_VALUE(...)
#  define ASSERT(...)

#endif

#define SHOULD_NOT_REACH(...) ASSERT(-1, __VA_ARGS__)

inline void voyeur_log(const char* str)
{
#ifdef DEBUG
  fprintf(stderr, "%s", str);
  fflush(stderr);
#else
  // Do nothing.
#endif
}

// Enters an infinite loop and requests the user to attach a
// debugger. For debugging only.
void voyeur_request_debug(const char* reason);


#endif
