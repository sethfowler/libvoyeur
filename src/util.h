#ifndef LIBVOYEUR_UTIL_H
#define LIBVOYEUR_UTIL_H

#define TRY(_f, ...)                            \
  do {                                          \
    if (_f(__VA_ARGS__) < 0) {                  \
      perror(#_f);                              \
      exit(EXIT_FAILURE);                       \
    }                                           \
  } while (0)

#endif
