#include <stdlib.h>
#include <string.h>

#include "env.h"

char** augment_environment(char* const* envp, const char* sockpath)
{
  // Determine the size of the original environment.
  // TODO: Check if DYLD_INSERT_LIBRARIES already exists.
  unsigned envlen = 0;
  for ( ; envp[envlen] != NULL ; ++envlen);

  // Allocate the new environment variables and store in the context
  // to make it possible to free them later.
  char* dyld_insert_libraries_env = malloc(sizeof(char) * 1024);
  strlcpy(dyld_insert_libraries_env, "DYLD_INSERT_LIBRARIES=", 1024);
  strlcat(dyld_insert_libraries_env, "libvoyeur-exec.dylib", 1024);

  char* libvoyeur_socket_env = malloc(sizeof(char) * 1024);
  strlcpy(libvoyeur_socket_env, "LIBVOYEUR_SOCKET=", 1024);
  strlcat(libvoyeur_socket_env, sockpath, 1024);

  // Allocate a new environment, including additional space for the 2
  // extra environment variables we'll add and a terminating NULL.
  char** newenvp = malloc(sizeof(char*) * (envlen + 3));
  memcpy(newenvp, envp, sizeof(char*) * envlen);
  newenvp[envlen]     = dyld_insert_libraries_env;
  newenvp[envlen + 1] = libvoyeur_socket_env;
  newenvp[envlen + 2] = NULL;

  return newenvp;
}
