#ifndef VOYEUR_ENV_H
#define VOYEUR_ENV_H

#include <voyeur.h>

// Augments the provided list of environment variables with the
// variables required for libvoyeur to observe a process. It is
// assumed that the caller is calling this function between calling
// fork() and calling exec(), so the ownership of the allocated
// memory is irrelevant.
char** voyeur_augment_environment(char* const* envp,
                                  const char* voyeur_libs,
                                  const char* voyeur_opts,
                                  const char* sockpath);

// Encoding and decoding options.
char voyeur_encode_options(uint8_t opts);
uint8_t voyeur_decode_options(const char* opts, uint8_t offset);

#endif
