#ifndef VOYEUR_ENV_H
#define VOYEUR_ENV_H

#include <voyeur.h>

// Augments the provided list of environment variables with the
// variables required for libvoyeur to observe a process. If the
// caller doesn't fork, then after calling exec() they should free
// both the returned environment and the buffer returned in buf_out.
char** voyeur_augment_environment(char* const* envp,
                                  const char* voyeur_libs,
                                  const char* voyeur_opts,
                                  const char* sockpath,
                                  void** buf_out);

// Encoding and decoding options.
char voyeur_encode_options(uint8_t opts);
uint8_t voyeur_decode_options(const char* opts, uint8_t offset);

#endif
