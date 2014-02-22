#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef __linux__
// For strlcpy/strlcat.
#include <bsd/string.h>
#endif

#include "env.h"

#ifdef __APPLE__
#define INSERT_LIBS "DYLD_INSERT_LIBRARIES="
#else
#define INSERT_LIBS "LD_PRELOAD="
#endif

#define ENV_VAR_SIZE 2048

char** voyeur_augment_environment(char* const* envp,
                                  const char* voyeur_libs,
                                  const char* voyeur_opts,
                                  const char* sockpath)
{
  // Determine the size of the original environment.
  unsigned envlen = 0;
  char* existing_dyld_insert = NULL;
  for ( ; envp[envlen] != NULL ; ++envlen) {
    printf("Comparing [%s] and [%s]\n", envp[envlen], INSERT_LIBS);
    if (strncmp(envp[envlen], INSERT_LIBS, sizeof(INSERT_LIBS) - 1) == 0) {
      existing_dyld_insert = envp[envlen] +
                             sizeof(INSERT_LIBS) - 1;
    }
  }

  // Allocate the new environment variables and store in the context
  // to make it possible to free them later.
  // TODO: Change the comment once I decide what to do here.
  char* dyld_insert_libraries_env = malloc(ENV_VAR_SIZE);
  strlcpy(dyld_insert_libraries_env, INSERT_LIBS, ENV_VAR_SIZE);
  strlcat(dyld_insert_libraries_env, voyeur_libs, ENV_VAR_SIZE);
  if (existing_dyld_insert) {
    strlcat(dyld_insert_libraries_env, ":", ENV_VAR_SIZE);
    strlcat(dyld_insert_libraries_env, existing_dyld_insert, ENV_VAR_SIZE);
  }
  printf("Final env var is %s\n", dyld_insert_libraries_env);

  char* libvoyeur_libs_env = malloc(ENV_VAR_SIZE);
  strlcpy(libvoyeur_libs_env, "LIBVOYEUR_LIBS=", ENV_VAR_SIZE);
  strlcat(libvoyeur_libs_env, voyeur_libs, ENV_VAR_SIZE);

  char* libvoyeur_opts_env = malloc(ENV_VAR_SIZE);
  strlcpy(libvoyeur_opts_env, "LIBVOYEUR_OPTS=", ENV_VAR_SIZE);
  strlcat(libvoyeur_opts_env, voyeur_opts, ENV_VAR_SIZE);

  char* libvoyeur_socket_env = malloc(ENV_VAR_SIZE);
  strlcpy(libvoyeur_socket_env, "LIBVOYEUR_SOCKET=", ENV_VAR_SIZE);
  strlcat(libvoyeur_socket_env, sockpath, ENV_VAR_SIZE);

  // Allocate a new environment, including additional space for the 4
  // extra environment variables we'll add and a terminating NULL.
  char** newenvp = malloc(sizeof(char*) * (envlen + 5));
  memcpy(newenvp, envp, sizeof(char*) * envlen);
  newenvp[envlen]     = dyld_insert_libraries_env;
  newenvp[envlen + 1] = libvoyeur_libs_env;
  newenvp[envlen + 2] = libvoyeur_opts_env;
  newenvp[envlen + 3] = libvoyeur_socket_env;
  newenvp[envlen + 4] = NULL;

  return newenvp;
}

char voyeur_encode_options(uint8_t opts)
{
  // Stripping all but the last 5 bits and bitwise-or'ing with '@' will always
  // result in a printable character. The downside is that we can only
  // support 5 flags this way.
  return '@' | ((char) opts & 0x1F);
}

uint8_t voyeur_decode_options(const char* opts, uint8_t offset)
{
  if (!opts || offset > strnlen(opts, 8)) {
    return 0;
  }

  return (uint8_t) (opts[offset] & 0x1F);
}
