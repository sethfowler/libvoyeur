#ifndef LIBVOYEUR_UTIL_H
#define LIBVOYEUR_UTIL_H

#include <stdio.h>

#define TRY(_f, ...)                            \
  do {                                          \
    if (_f(__VA_ARGS__) < 0) {                  \
      perror(#_f);                              \
      exit(EXIT_FAILURE);                       \
    }                                           \
  } while (0)

#define CHECK(_var, _err)                       \
  do {                                          \
    if (_var < 0) {                             \
      perror(_err);                             \
      exit(EXIT_FAILURE);                       \
    }                                           \
  } while (0)


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
