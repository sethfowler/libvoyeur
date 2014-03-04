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

typedef char env_buf[2048];

char** voyeur_augment_environment(char* const* envp,
                                  const char* voyeur_libs,
                                  const char* voyeur_opts,
                                  const char* sockpath,
                                  void** buf_out)
{
  // Determine the size of the original environment.
  unsigned envlen = 0;
  unsigned existing_insert_idx = 0;
  char* existing_insert = NULL;
  for ( ; envp[envlen] != NULL ; ++envlen) {
    if (strncmp(envp[envlen], INSERT_LIBS, sizeof(INSERT_LIBS) - 1) == 0) {
      existing_insert = envp[envlen] +
                             sizeof(INSERT_LIBS) - 1;
      existing_insert_idx = envlen;
    }
  }

  char must_add_voyeur_libs = 1;
  if (existing_insert && strnstr(existing_insert, voyeur_libs, sizeof(env_buf) * 2)) {
    must_add_voyeur_libs = 0;
  }
  if (must_add_voyeur_libs &&
      (strlen(voyeur_libs) + (existing_insert ? strlen(existing_insert) : 0)) > sizeof(env_buf)) {
    fprintf(stderr, "Not enough space to insert libvoyeur into " INSERT_LIBS);
    must_add_voyeur_libs = 0;
  }

  // Allocate the new environment variables.
  env_buf* buf = malloc(sizeof(env_buf) * 4);
  *buf_out = (void*) buf;

  strlcpy(buf[0], "LIBVOYEUR_LIBS=", sizeof(env_buf));
  strlcat(buf[0], voyeur_libs, sizeof(env_buf));

  strlcpy(buf[1], "LIBVOYEUR_OPTS=", sizeof(env_buf));
  strlcat(buf[1], voyeur_opts, sizeof(env_buf));

  strlcpy(buf[2], "LIBVOYEUR_SOCKET=", sizeof(env_buf));
  strlcat(buf[2], sockpath, sizeof(env_buf));

  strlcpy(buf[3], INSERT_LIBS, sizeof(env_buf));
  if (must_add_voyeur_libs) {
    strlcat(buf[3], voyeur_libs, sizeof(env_buf));
    if (existing_insert) {
      strlcat(buf[3], ":", sizeof(env_buf));
      strlcat(buf[3], existing_insert, sizeof(env_buf));
    }
  } else if (existing_insert) {
    strlcat(buf[3], existing_insert, sizeof(env_buf));
  }

  // Allocate a new environment, including additional space for the 4
  // extra environment variables we'll add and a terminating NULL.
  char** newenvp = malloc(sizeof(char*) * (envlen + 5));
  memcpy(newenvp, envp, sizeof(char*) * envlen);
  newenvp[envlen + 0] = buf[0];
  newenvp[envlen + 1] = buf[1];
  newenvp[envlen + 2] = buf[2];

  if (existing_insert) {
    // Make sure we don't have two INSERT_LIBS environment variables.
    newenvp[existing_insert_idx] = buf[3];
    newenvp[envlen + 3] = NULL;
  } else {
    newenvp[envlen + 3] = buf[3];
    newenvp[envlen + 4] = NULL;
  }

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
