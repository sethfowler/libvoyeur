#ifndef __APPLE__
#define _GNU_SOURCE
#endif

#include <pthread.h>
#include <spawn.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "dyld.h"
#include "env.h"
#include "net.h"
#include "util.h"


//////////////////////////////////////////////////
// Shared code for all exec*() functions.
//////////////////////////////////////////////////

static void write_exec_event(int sock, uint8_t options, const char* path,
                             char* const argv[], char* const envp[],
                             pid_t pid, pid_t ppid)
{
  if (!(options & OBSERVE_EXEC_NOACCESS)) {
    // Make sure this exec() call could succeed before reporting the event.
    if (access(path, X_OK) < 0) {
      return;
    }
  }

  voyeur_write_msg_type(sock, VOYEUR_MSG_EVENT);
  voyeur_write_event_type(sock, VOYEUR_EVENT_EXEC);
  voyeur_write_string(sock, path, 0);

  int argc = 0;
  while (argv[argc]) {
    ++argc;
  }
  voyeur_write_int(sock, argc);
  for (int i = 0 ; i < argc ; ++i) {
    voyeur_write_string(sock, argv[i], 0);
  }

  if (options & OBSERVE_EXEC_ENV) {
    int envc = 0;
    while (envp[envc]) {
      ++envc;
    }
    voyeur_write_int(sock, envc);
    for (int i = 0 ; i < envc ; ++i) {
      voyeur_write_string(sock, envp[i], 0);
    }
  }

  if (options & OBSERVE_EXEC_PATH) {
    voyeur_write_string(sock, getenv("PATH"), 0);
  }

  if (options & OBSERVE_EXEC_CWD) {
    voyeur_write_string(sock, getcwd(NULL, 0), 0);
  }

  voyeur_write_pid(sock, pid);
  voyeur_write_pid(sock, ppid);
}


//////////////////////////////////////////////////
// Replace vfork() with fork()
//////////////////////////////////////////////////

typedef pid_t (*vfork_fptr_t)();

pid_t VOYEUR_FUNC(vfork)(void)
{
    return fork();
}

VOYEUR_INTERPOSE(vfork)


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
  uint8_t options = voyeur_decode_options(opts, VOYEUR_EVENT_EXEC);
  const char* sockpath = getenv("LIBVOYEUR_SOCKET");

  // Write the event to the socket.
  int sock = voyeur_create_client_socket(sockpath);
  if (sock >= 0) {
    write_exec_event(sock, options, path, argv, envp, getpid(), getppid());

    // We might as well close the socket since there's no chance we'll
    // ever be called a second time by the same process. (Even if the
    // exec fails, generally the fork'd process will just bail.)
    voyeur_write_msg_type(sock, VOYEUR_MSG_DONE);
    voyeur_close_socket(sock);
  }

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
static uint8_t voyeur_posix_spawn_options = 0;
static char* voyeur_posix_spawn_sockpath = NULL;
static int voyeur_posix_spawn_sock = 0;
VOYEUR_STATIC_DECLARE_NEXT(posix_spawn_fptr_t, posix_spawn);
VOYEUR_STATIC_DECLARE_NEXT(posix_spawn_fptr_t, posix_spawnp);

__attribute__((destructor)) void voyeur_cleanup_posix_spawn()
{
  pthread_mutex_lock(&voyeur_posix_spawn_mutex);

  if (voyeur_posix_spawn_initialized) {
    if (voyeur_posix_spawn_sock >= 0) {
      voyeur_write_msg_type(voyeur_posix_spawn_sock, VOYEUR_MSG_DONE);
      voyeur_close_socket(voyeur_posix_spawn_sock);
      voyeur_posix_spawn_sock = -1;
    }

    voyeur_posix_spawn_initialized = 0;
  }

  pthread_mutex_unlock(&voyeur_posix_spawn_mutex);
}

static void voyeur_init_posix_spawn()
{
  if (!voyeur_posix_spawn_initialized) {
    voyeur_posix_spawn_libs = getenv("LIBVOYEUR_LIBS");
    voyeur_posix_spawn_opts = getenv("LIBVOYEUR_OPTS");
    voyeur_posix_spawn_options =
      voyeur_decode_options(voyeur_posix_spawn_opts, VOYEUR_EVENT_EXEC);
    voyeur_posix_spawn_sockpath = getenv("LIBVOYEUR_SOCKET");
    voyeur_posix_spawn_sock =
      voyeur_create_client_socket(voyeur_posix_spawn_sockpath);
    VOYEUR_LOOKUP_NEXT(posix_spawn_fptr_t, posix_spawn);
    VOYEUR_LOOKUP_NEXT(posix_spawn_fptr_t, posix_spawnp);
    voyeur_posix_spawn_initialized = 1;
  }
}

int VOYEUR_FUNC(posix_spawn)(pid_t* pid,
                             const char* restrict path,
                             const posix_spawn_file_actions_t* file_actions,
                             const posix_spawnattr_t* restrict attrp,
                             char* const argv[restrict],
                             char* const envp[restrict])
{
  pthread_mutex_lock(&voyeur_posix_spawn_mutex);

  voyeur_init_posix_spawn();

  // Add libvoyeur-specific environment variables.
  void* buf;
  char** voyeur_envp =
    voyeur_augment_environment(envp,
                               voyeur_posix_spawn_libs,
                               voyeur_posix_spawn_opts,
                               voyeur_posix_spawn_sockpath,
                               &buf);

  // Pass through the call to the real posix_spawn.
  pid_t child_pid;
  int retval = VOYEUR_CALL_NEXT(posix_spawn, &child_pid, path,
                                file_actions, attrp,
                                argv, voyeur_envp);

  // Write the event to the socket.
  if (voyeur_posix_spawn_sock >= 0) {
    write_exec_event(voyeur_posix_spawn_sock,
                     voyeur_posix_spawn_options,
                     path, argv, envp,
                     child_pid, getpid());
  }

  pthread_mutex_unlock(&voyeur_posix_spawn_mutex);

  // Free the resources we allocated.
  free(voyeur_envp);
  free(buf);

  // It's legal to pass NULL for the pid argument, so double-check we
  // have somewhere to write the pid to before doing it.
  if (pid) {
    *pid = child_pid;
  }

  return retval;
}

VOYEUR_INTERPOSE(posix_spawn)

#ifdef __linux__

// On Linux we need to interpose on every exec variant separately because they
// don't just forward the work to execve and posix_spawn like OS X's versions do.

//////////////////////////////////////////////////
// execl, execle, execv
//////////////////////////////////////////////////

#define VARARGS_TO_ARGV(_start, _path, _argv, _envp)     \
  do {                                                   \
    va_list args;                                        \
                                                         \
    /* Get total number of entries. */                   \
    unsigned length = 0;                                 \
    va_start(args, (_start));                            \
    while (va_arg(args, char*)) {                        \
      ++length;                                          \
    }                                                    \
    va_end(args);                                        \
                                                         \
    /* Increase size to fit argv[0] and final NULL. */   \
    length += 2;                                         \
                                                         \
    /* Create an appropriately sized _argv. */           \
    _argv = malloc(sizeof(const char*) * length);        \
    _argv[0] = (char *) _path;                           \
                                                         \
    /* Copy. */                                          \
    unsigned index = 1;                                  \
    va_start(args, (_start));                            \
    while (index < length) {                             \
      _argv[index] = va_arg(args, char*);                \
      ++index;                                           \
    }                                                    \
                                                         \
    /* Pull out envp from one past the end of argv. */   \
    _envp = va_arg(args, char**);                        \
                                                         \
    va_end(args);                                        \
  } while(0)


int VOYEUR_FUNC(execl)(const char* path, const char* start, ...)
{
  char** argv;
  char** dummy_envp;
  VARARGS_TO_ARGV(start, path, argv, dummy_envp);

  return execve(path, argv, environ);
}

VOYEUR_INTERPOSE(execl)


int VOYEUR_FUNC(execle)(const char* path, const char* start, ...)
{
  char** argv;
  char** envp;
  VARARGS_TO_ARGV(start, path, argv, envp);

  return execve(path, argv, envp);
}

VOYEUR_INTERPOSE(execle)


int VOYEUR_FUNC(execv)(const char* path, char* const argv[])
{
  return execve(path, argv, environ);
}

VOYEUR_INTERPOSE(execv)


//////////////////////////////////////////////////
// execlp, execvp, execvpe
//////////////////////////////////////////////////

typedef int (*execvpe_fptr_t)(const char*, char* const [], char* const []);

int VOYEUR_FUNC(execlp)(const char* path, const char* start, ...)
{
  const char* libs = getenv("LIBVOYEUR_LIBS");
  const char* opts = getenv("LIBVOYEUR_OPTS");
  uint8_t options = voyeur_decode_options(opts, VOYEUR_EVENT_EXEC);
  const char* sockpath = getenv("LIBVOYEUR_SOCKET");

  char** argv;
  char** dummy_envp;
  VARARGS_TO_ARGV(start, path, argv, dummy_envp);
  char** envp = environ;

  int sock = voyeur_create_client_socket(sockpath);
  if (sock >= 0) {
    write_exec_event(sock, options, path, argv, envp, getpid(), getppid());
    voyeur_write_msg_type(sock, VOYEUR_MSG_DONE);
    voyeur_close_socket(sock);
  }

  void* buf;
  char** voyeur_envp =
    voyeur_augment_environment(envp, libs, opts, sockpath, &buf);

  // Need to pass through to execvpe since we need to provide an environment.
  VOYEUR_DECLARE_NEXT(execvpe_fptr_t, execvpe);
  VOYEUR_LOOKUP_NEXT(execvpe_fptr_t, execvpe);
  return VOYEUR_CALL_NEXT(execvpe, path, argv, voyeur_envp);
}

VOYEUR_INTERPOSE(execlp)


int VOYEUR_FUNC(execvp)(const char* path, char* const argv[])
{
  const char* libs = getenv("LIBVOYEUR_LIBS");
  const char* opts = getenv("LIBVOYEUR_OPTS");
  uint8_t options = voyeur_decode_options(opts, VOYEUR_EVENT_EXEC);
  const char* sockpath = getenv("LIBVOYEUR_SOCKET");

  char** envp = environ;

  int sock = voyeur_create_client_socket(sockpath);
  if (sock >= 0) {
    write_exec_event(sock, options, path, argv, envp, getpid(), getppid());
    voyeur_write_msg_type(sock, VOYEUR_MSG_DONE);
    voyeur_close_socket(sock);
  }

  void* buf;
  char** voyeur_envp =
    voyeur_augment_environment(envp, libs, opts, sockpath, &buf);

  // Need to pass through to execvpe since we need to provide an environment.
  VOYEUR_DECLARE_NEXT(execvpe_fptr_t, execvpe);
  VOYEUR_LOOKUP_NEXT(execvpe_fptr_t, execvpe);
  return VOYEUR_CALL_NEXT(execvpe, path, argv, voyeur_envp);
}

VOYEUR_INTERPOSE(execvp)


int VOYEUR_FUNC(execvpe)(const char* path, char* const argv[], char* const envp[])
{
  const char* libs = getenv("LIBVOYEUR_LIBS");
  const char* opts = getenv("LIBVOYEUR_OPTS");
  uint8_t options = voyeur_decode_options(opts, VOYEUR_EVENT_EXEC);
  const char* sockpath = getenv("LIBVOYEUR_SOCKET");

  int sock = voyeur_create_client_socket(sockpath);
  if (sock >= 0) {
    write_exec_event(sock, options, path, argv, envp, getpid(), getppid());
    voyeur_write_msg_type(sock, VOYEUR_MSG_DONE);
    voyeur_close_socket(sock);
  }

  void* buf;
  char** voyeur_envp =
    voyeur_augment_environment(envp, libs, opts, sockpath, &buf);

  // Pass through the call to the real execvpe.
  VOYEUR_DECLARE_NEXT(execvpe_fptr_t, execvpe);
  VOYEUR_LOOKUP_NEXT(execvpe_fptr_t, execvpe);
  return VOYEUR_CALL_NEXT(execvpe, path, argv, voyeur_envp);
}

VOYEUR_INTERPOSE(execvpe)


//////////////////////////////////////////////////
// posix_spawnp
//////////////////////////////////////////////////

int VOYEUR_FUNC(posix_spawnp)(pid_t* pid,
                              const char* restrict path,
                              const posix_spawn_file_actions_t* file_actions,
                              const posix_spawnattr_t* restrict attrp,
                              char* const argv[restrict],
                              char* const envp[restrict])
{
  pthread_mutex_lock(&voyeur_posix_spawn_mutex);

  voyeur_init_posix_spawn();

  void* buf;
  char** voyeur_envp =
    voyeur_augment_environment(envp,
                               voyeur_posix_spawn_libs,
                               voyeur_posix_spawn_opts,
                               voyeur_posix_spawn_sockpath,
                               &buf);

  // Pass through the call to the real posix_spawnp.
  pid_t child_pid;
  int retval = VOYEUR_CALL_NEXT(posix_spawnp, &child_pid, path,
                                file_actions, attrp,
                                argv, voyeur_envp);

  if (voyeur_posix_spawn_sock >= 0) {
    write_exec_event(voyeur_posix_spawn_sock,
                     voyeur_posix_spawn_options,
                     path, argv, envp,
                     child_pid, getpid());
  }

  pthread_mutex_unlock(&voyeur_posix_spawn_mutex);

  free(voyeur_envp);
  free(buf);

  if (pid) {
    *pid = child_pid;
  }

  return retval;
}

VOYEUR_INTERPOSE(posix_spawnp)

#endif
