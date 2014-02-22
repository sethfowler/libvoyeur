#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "env.h"
#include "event.h"
#include "net.h"
#include <voyeur.h>

#define TRY(_f, ...)                            \
  do {                                          \
    if (_f(__VA_ARGS__) < 0) {                  \
      perror(#_f);                              \
      exit(EXIT_FAILURE);                       \
    }                                           \
  } while (0)

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

  if (context->exec_cb) {
    ((voyeur_exec_callback)context->exec_cb)(path, argv, envp, cwd,
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

  TRY(voyeur_read_string, fd, &path, 0);
  TRY(voyeur_read_int, fd, &oflag);
  TRY(voyeur_read_int, fd, &mode);
  TRY(voyeur_read_int, fd, &retval);

  // Read the current working directory.
  char* cwd = NULL;
  if (context->open_opts & OBSERVE_OPEN_CWD) {
    TRY(voyeur_read_string, fd, &cwd, 0);
  }

  if (context->open_cb) {
    ((voyeur_open_callback)context->open_cb)(path, oflag, (mode_t) mode, cwd,
                                             retval, context->open_userdata);
  }

  free(path);
}

static void handle_close(voyeur_context* context, int fd)
{
  int fildes, retval;

  TRY(voyeur_read_int, fd, &fildes);
  TRY(voyeur_read_int, fd, &retval);

  if (context->close_cb) {
    ((voyeur_close_callback)context->close_cb)(fildes, retval,
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

#define LIBS_SIZE 256

#ifdef __APPLE__
#define LIB_SUFFIX ".dylib"
#else
#define LIB_SUFFIX ".so"
#endif

#define ON_EVENT(_, e) \
  if (context->e##_cb) { \
    if (prev) strlcat(libs, ":", LIBS_SIZE); \
    strlcat(libs, cwd, LIBS_SIZE); \
    strlcat(libs, "/", LIBS_SIZE); \
    strlcat(libs, "libvoyeur-" #e LIB_SUFFIX, LIBS_SIZE); \
    prev = 1; \
  } \

char* voyeur_requested_libs(voyeur_context* context)
{
  char* libs = calloc(1, LIBS_SIZE);
  char prev = 0;
  
  // TODO: This should be relative to the library location, not the current
  // directory. This is just a quick hack.
  char* cwd = getcwd(NULL, 0);

  MAP_EVENTS

  free(cwd);

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
