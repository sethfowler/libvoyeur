#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "env.h"
#include "event.h"
#include "net.h"
#include "util.h"
#include <voyeur.h>

// Note that in event handlers, we always have to read the data,
// even if the callback isn't present, so we can move on to the
// next event in the stream. In practice the callback should always be
// present, so it's not worth worrying about.

static void handle_exec(voyeur_context* context, int fd)
{
  // Read the path.
  char* path;
  TRY(voyeur_read_string, fd, &path, 0);

  // Read the arguments.
  int argc;
  TRY(voyeur_read_int, fd, &argc);

  char** argv = malloc(sizeof(char*) * (argc + 1));
  for (int i = 0 ; i < argc ; ++i) {
    char* arg;
    TRY(voyeur_read_string, fd, &arg, 0);
    argv[i] = arg;
  }
  argv[argc] = NULL;

  // Read the environment.
  int envc;
  char** envp = NULL;
  if (context->exec_opts & OBSERVE_EXEC_ENV) {
    TRY(voyeur_read_int, fd, &envc);

    envp = malloc(sizeof(char*) * (envc + 1));
    for (int i = 0 ; i < envc ; ++i) {
      char* envvar;
      TRY(voyeur_read_string, fd, &envvar, 0);
      envp[i] = envvar;
    }
    envp[envc] = NULL;
  }

  // Read the current working directory.
  char* cwd = NULL;
  if (context->exec_opts & OBSERVE_EXEC_CWD) {
    TRY(voyeur_read_string, fd, &cwd, 0);
  }

  // Read the pid and ppid.
  pid_t pid, ppid;
  TRY(voyeur_read_pid, fd, &pid);
  TRY(voyeur_read_pid, fd, &ppid);

  if (context->exec_cb) {
    ((voyeur_exec_callback)context->exec_cb)(path, argv,
                                             envp, cwd,
                                             pid, ppid,
                                             context->exec_userdata);
  }

  // Free everything.
  free(path);

  for (int i = 0 ; i < argc ; ++i) {
    free(argv[i]);
  }
  free(argv);

  if (context->exec_opts & OBSERVE_EXEC_ENV) {
    for (int i = 0 ; i < envc ; ++i) {
      free(envp[i]);
    }
    free(envp);
  }

  if (context->exec_opts & OBSERVE_EXEC_CWD) {
    free(cwd);
  }
}

static void handle_open(voyeur_context* context, int fd)
{
  char* path;
  int oflag, mode, retval;
  char* cwd = NULL;
  pid_t pid;

  TRY(voyeur_read_string, fd, &path, 0);
  TRY(voyeur_read_int, fd, &oflag);
  TRY(voyeur_read_int, fd, &mode);
  TRY(voyeur_read_int, fd, &retval);

  if (context->open_opts & OBSERVE_OPEN_CWD) {
    TRY(voyeur_read_string, fd, &cwd, 0);
  }

  TRY(voyeur_read_pid, fd, &pid);

  if (context->open_cb) {
    ((voyeur_open_callback)context->open_cb)(path, oflag,
                                             (mode_t) mode,
                                             cwd, retval, pid,
                                             context->open_userdata);
  }

  free(path);
}

static void handle_close(voyeur_context* context, int fd)
{
  int fildes, retval;
  pid_t pid;

  TRY(voyeur_read_int, fd, &fildes);
  TRY(voyeur_read_int, fd, &retval);
  TRY(voyeur_read_pid, fd, &pid);

  if (context->close_cb) {
    ((voyeur_close_callback)context->close_cb)(fildes, retval, pid,
                                               context->close_userdata);
  }
}

#define ON_EVENT(E, e)                          \
  case VOYEUR_EVENT_##E:                        \
    handle_##e(context, fd);                    \
    break;

void voyeur_handle_event(voyeur_context* context, voyeur_event_type type, int fd)
{
  switch (type) {
    MAP_EVENTS
    default:
      fprintf(stderr, "libvoyeur: got unknown event type %u\n",
              (unsigned) type);
      exit(EXIT_FAILURE);
  }
}

#undef ON_EVENT

#ifdef __APPLE__
#define LIB_SUFFIX ".dylib"
#else
#define LIB_SUFFIX ".so"
#endif

#define ON_EVENT(_, e)           \
  "libvoyeur-" #e LIB_SUFFIX ":" \

static size_t compute_libs_size(size_t libdir_size)
{
  static char all_libs[] =
    MAP_EVENTS
    ;

  return sizeof(all_libs) + VOYEUR_EVENT_MAX * libdir_size;
}

#undef ON_EVENT

#define ON_EVENT(_, e)                                    \
  if (context->e##_cb) {                                  \
    if (prev) strlcat(libs, ":", libs_size);              \
    strlcat(libs, libdir, libs_size);                     \
    strlcat(libs, "libvoyeur-" #e LIB_SUFFIX, libs_size); \
    prev = 1;                                             \
  }                                                       \

char* voyeur_requested_libs(voyeur_context* context)
{
  bool did_allocate = false;
  char* libdir = "./";
  Dl_info dlinfo;
  if (dladdr(voyeur_requested_libs, &dlinfo) && dlinfo.dli_fname) {
    // Strip the filename. It'd be great if 'dirname' could be used
    // for this, but it's not thread safe. Note that if no slash is
    // found, we just stick with "./".
    char* last_slash = strrchr(dlinfo.dli_fname, '/');
    if (last_slash) {
      did_allocate = true;
      int len_with_slash_and_null = (last_slash - dlinfo.dli_fname) + 2;
      libdir = calloc(1, len_with_slash_and_null);
      strlcpy(libdir, dlinfo.dli_fname, len_with_slash_and_null);
    }
  }

  size_t libs_size = compute_libs_size(strnlen(libdir, 4096));
  char* libs = calloc(1, libs_size);
  char prev = 0;
  
  MAP_EVENTS

  if (did_allocate) {
    free(libdir);
  }

  return libs;
}

#undef ON_EVENT

#define ON_EVENT(E, e) opts[VOYEUR_EVENT_##E] = voyeur_encode_options(context->e##_opts);

char* voyeur_requested_opts(voyeur_context* context)
{
  // VOYEUR_EVENT_MAX is one greater than the highest event number and
  // thus leaves room for a terminating null.
  char* opts = calloc(1, VOYEUR_EVENT_MAX);

  MAP_EVENTS

  return opts;
}

#undef ON_EVENT

#define ON_EVENT(_, e)                                  \
void voyeur_observe_##e(voyeur_context_t ctx,           \
                        uint8_t opts,                   \
                        voyeur_##e##_callback callback, \
                        void* userdata)                 \
{                                                       \
  voyeur_context* context = (voyeur_context*) ctx;      \
  context->e##_opts = opts;                             \
  context->e##_cb = (void*) callback;                   \
  context->e##_userdata = userdata;                     \
}

MAP_EVENTS

#undef ON_EVENT
