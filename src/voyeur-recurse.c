#ifndef __APPLE__
#define _GNU_SOURCE
#endif

#include <pthread.h>
#include <spawn.h>
#include <stdlib.h>
#include <unistd.h>

#include "dyld.h"
#include "env.h"
#include "net.h"


// This observer interposes on exec*(), just like libvoyeur-exec, but does not
// generate any events. It exists to allow libvoyeur to work recursively even in
// cases where the user does not want to observe exec*() events. event.c ensures
// that only libvoyeur-exec or libvoyeur-recurse are linked in, but not both.

//////////////////////////////////////////////////
// execve
//////////////////////////////////////////////////

typedef int (*execve_fptr_t)(const char*, char* const[], char* const []);

int VOYEUR_FUNC(execve)(const char* path, char* const argv[], char* const envp[])
{
  // In the case of exec we don't bother caching anything, since exec
  // will wipe out this whole process image anyway.
  const char* libs = getenv("LIBVOYEUR_LIBS");
  const char* opts = getenv("LIBVOYEUR_OPTS");
  const char* sockpath = getenv("LIBVOYEUR_SOCKET");

  // Add libvoyeur-specific environment variables. (We don't bother
  // freeing 'buf' since we need it until the execve call and we have
  // no way of freeing it after that.)
  void* buf;
  char** voyeur_envp =
    voyeur_augment_environment(envp, libs, opts, sockpath, &buf);

  // Pass through the call to the real execve.
  VOYEUR_DECLARE_NEXT(execve_fptr_t, execve);
  VOYEUR_LOOKUP_NEXT(execve_fptr_t, execve);
  return VOYEUR_CALL_NEXT(execve, path, argv, voyeur_envp);
}

VOYEUR_INTERPOSE(execve)


//////////////////////////////////////////////////
// posix_spawn
//////////////////////////////////////////////////

typedef int (*posix_spawn_fptr_t)(pid_t* restrict,
                                  const char* restrict,
                                  const posix_spawn_file_actions_t*,
                                  const posix_spawnattr_t* restrict,
                                  char* const[restrict],
                                  char* const[restrict]);

static pthread_mutex_t voyeur_posix_spawn_mutex = PTHREAD_MUTEX_INITIALIZER;
static char voyeur_posix_spawn_initialized = 0;
static char* voyeur_posix_spawn_libs = NULL;
static char* voyeur_posix_spawn_opts = NULL;
static char* voyeur_posix_spawn_sockpath = NULL;
VOYEUR_STATIC_DECLARE_NEXT(posix_spawn_fptr_t, posix_spawn);

__attribute__((destructor)) void voyeur_cleanup_posix_spawn()
{
  pthread_mutex_lock(&voyeur_posix_spawn_mutex);

  if (voyeur_posix_spawn_initialized) {
    voyeur_posix_spawn_initialized = 0;
  }

  pthread_mutex_unlock(&voyeur_posix_spawn_mutex);
}

int VOYEUR_FUNC(posix_spawn)(pid_t* pid,
                             const char* restrict path,
                             const posix_spawn_file_actions_t* file_actions,
                             const posix_spawnattr_t* restrict attrp,
                             char* const argv[restrict],
                             char* const envp[restrict])
{
  pthread_mutex_lock(&voyeur_posix_spawn_mutex);

  if (!voyeur_posix_spawn_initialized) {
    voyeur_posix_spawn_libs = getenv("LIBVOYEUR_LIBS");
    voyeur_posix_spawn_opts = getenv("LIBVOYEUR_OPTS");
    voyeur_posix_spawn_sockpath = getenv("LIBVOYEUR_SOCKET");
    VOYEUR_LOOKUP_NEXT(posix_spawn_fptr_t, posix_spawn);
    voyeur_posix_spawn_initialized = 1;
  }

  // Add libvoyeur-specific environment variables.
  void* buf;
  char** voyeur_envp =
    voyeur_augment_environment(envp,
                               voyeur_posix_spawn_libs,
                               voyeur_posix_spawn_opts,
                               voyeur_posix_spawn_sockpath,
                               &buf);

  // Pass through the call to the real posix_spawn.
  int retval = VOYEUR_CALL_NEXT(posix_spawn, pid, path,
                                file_actions, attrp,
                                argv, voyeur_envp);

  pthread_mutex_unlock(&voyeur_posix_spawn_mutex);

  // Free the resources we allocated.
  free(voyeur_envp);
  free(buf);

  return retval;
}

VOYEUR_INTERPOSE(posix_spawn)
