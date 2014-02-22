#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/un.h>
#include <unistd.h>

#ifdef __linux__
// For strlcpy/strlcat.
#include <bsd/string.h>
#endif

#include <voyeur.h>
#include "env.h"
#include "net.h"

#ifdef __APPLE__
#define LIB_SUFFIX ".dylib"
#else
#define LIB_SUFFIX ".so"
#endif

#define TRY(_f, ...)                            \
  do {                                          \
    if (_f(__VA_ARGS__) < 0) {                  \
      perror(#_f);                              \
      exit(EXIT_FAILURE);                       \
    }                                           \
  } while (0)

typedef struct {
  uint8_t exec_opts;
  voyeur_exec_callback exec_cb;
  void* exec_userdata;

  uint8_t open_opts;
  voyeur_open_callback open_cb;
  void* open_userdata;

  uint8_t close_opts;
  voyeur_close_callback close_cb;
  void* close_userdata;
} voyeur_context;

voyeur_context_t voyeur_context_create()
{
  voyeur_context* ctx = calloc(1, sizeof(voyeur_context));
  return (voyeur_context_t) ctx;
}

void voyeur_context_destroy(voyeur_context_t ctx)
{
  free((voyeur_context*) ctx);
}

void voyeur_observe_exec(voyeur_context_t ctx,
                         uint8_t opts,
                         voyeur_exec_callback callback,
                         void* userdata)
{
  voyeur_context* context = (voyeur_context*) ctx;
  context->exec_opts = opts;
  context->exec_cb = callback;
  context->exec_userdata = userdata;
}

void voyeur_observe_open(voyeur_context_t ctx,
                         uint8_t opts,
                         voyeur_open_callback callback,
                         void* userdata)
{
  voyeur_context* context = (voyeur_context*) ctx;
  context->open_opts = opts;
  context->open_cb = callback;
  context->open_userdata = userdata;
}

void voyeur_observe_close(voyeur_context_t ctx,
                          uint8_t opts,
                          voyeur_close_callback callback,
                          void* userdata)
{
  voyeur_context* context = (voyeur_context*) ctx;
  context->close_opts = opts;
  context->close_cb = callback;
  context->close_userdata = userdata;
}

typedef struct {
  pid_t child_pid;
  int child_pipe_input;
} waitpid_thread_arg;

void* waitpid_thread(void* arg_ptr)
{
  waitpid_thread_arg* arg = (waitpid_thread_arg*) arg_ptr;
  
  int status;
  waitpid(arg->child_pid, &status, 0);

  voyeur_write_int(arg->child_pipe_input, status);

  close(arg->child_pipe_input);
  return NULL;
}

int start_waitpid_thread(pid_t child_pid)
{
  // Create a pipe that will be used to announce the termination of
  // the child process.
  int waitpid_pipe[2];
  TRY(pipe, waitpid_pipe);

  waitpid_thread_arg* arg = malloc(sizeof(waitpid_thread_arg));
  arg->child_pid = child_pid;
  arg->child_pipe_input = waitpid_pipe[1];
  
  pthread_t thread;
  pthread_create(&thread, NULL, waitpid_thread, (void*) arg);
  pthread_detach(thread);

  return waitpid_pipe[0];
}

void run_child(const char* path,
               char* const argv[],
               char* const envp[])
{
  // Start the requested process. Normally this never returns.
  execve(path, argv, envp);

  // If execve *did* return, something bad happened.
  perror("execve");
  _exit(EXIT_FAILURE);
}

int accept_connection(int server_sock)
{
  struct sockaddr_un client_info;
  socklen_t client_info_len = sizeof(struct sockaddr_un);

  int client_sock =
    accept(server_sock, (struct sockaddr *) &client_info, &client_info_len);

  if (client_sock < 0) {
    perror("accept");
    exit(EXIT_FAILURE);
  }

  printf("Client %d connected.\n", client_sock);
  
  return client_sock;
}

void handle_exec(voyeur_context* context, int fd)
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

  // Invoke the callback, if there is one.
  // Note that we always have to read the data, even if the callback
  // isn't present, so we can move on to the next event. In practice
  // that should never happen, so it's not worth worrying about.
  if (context->exec_cb) {
    context->exec_cb(path, argv, envp, cwd, context->exec_userdata);
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

void handle_open(voyeur_context* context, int fd)
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
    context->open_cb(path, oflag, (mode_t) mode, cwd,
                     retval, context->open_userdata);
  }

  free(path);
}

void handle_close(voyeur_context* context, int fd)
{
  int fildes, retval;

  TRY(voyeur_read_int, fd, &fildes);
  TRY(voyeur_read_int, fd, &retval);

  if (context->close_cb) {
    context->close_cb(fildes, retval, context->close_userdata);
  }
}

int run_server(voyeur_context* context,
               int server_sock,
               int child_pipe_output)
{
  fd_set active_fd_set, read_fd_set;
  FD_ZERO(&active_fd_set);
  FD_SET(server_sock, &active_fd_set);
  FD_SET(child_pipe_output, &active_fd_set);
  
  int child_exited = 0;
  int child_status = 0;
  
  while (!child_exited) {
    // Block until input arrives.
    read_fd_set = active_fd_set;
    TRY(select, FD_SETSIZE, &read_fd_set, NULL, NULL, NULL);

    for (int fd = 0 ; fd < FD_SETSIZE ; ++fd) {
      if (FD_ISSET(fd, &read_fd_set)) {
        if (fd == server_sock) {
          int client_sock = accept_connection(server_sock);
          FD_SET(client_sock, &active_fd_set);
        } else if (fd == child_pipe_output) {
          child_exited = 1;
          voyeur_read_int(fd, &child_status);
          close(fd);
          FD_CLR(fd, &active_fd_set);
        } else {
          // Got a voyeur event; dispatch to the appropriate handler.
          voyeur_event_type type;
          if (voyeur_read_event_type(fd, &type) < 0) {
            printf("Client %d disconnected.\n", fd);
            close(fd);
            FD_CLR(fd, &active_fd_set);
          } else if (type == VOYEUR_EVENT_EXEC) {
            handle_exec(context, fd);
          } else if (type == VOYEUR_EVENT_OPEN) {
            handle_open(context, fd);
          } else if (type == VOYEUR_EVENT_CLOSE) {
            handle_close(context, fd);
          } else {
            fprintf(stderr, "libvoyeur: got unknown event type %u\n",
                    (unsigned) type);
            exit(EXIT_FAILURE);
          }
        }
      }
    }
  }

  close(server_sock);
  
  if (WIFEXITED(child_status)) {
    return WEXITSTATUS(child_status);
  } else {
    fprintf(stderr, "libvoyeur: child process did not terminate normally\n");
    return -1;
  }
}

#define LIBS_SIZE 256

char* requested_libs(voyeur_context* context)
{
  char* libs = calloc(1, LIBS_SIZE);
  char prev = 0;
  
  // TODO: This should be relative to the library location, not the current
  // directory. This is just a quick hack.
  char* cwd = getcwd(NULL, 0);

  if (context->exec_cb) {
    strlcat(libs, cwd, LIBS_SIZE);
    strlcat(libs, "/", LIBS_SIZE);
    strlcat(libs, "libvoyeur-exec" LIB_SUFFIX, LIBS_SIZE);
    prev = 1;
  }

  if (context->open_cb) {
    if (prev) strlcat(libs, ":", LIBS_SIZE);
    strlcat(libs, cwd, LIBS_SIZE);
    strlcat(libs, "/", LIBS_SIZE);
    strlcat(libs, "libvoyeur-open" LIB_SUFFIX, LIBS_SIZE);
    prev = 1;
  }

  if (context->close_cb) {
    if (prev) strlcat(libs, ":", LIBS_SIZE);
    strlcat(libs, cwd, LIBS_SIZE);
    strlcat(libs, "/", LIBS_SIZE);
    strlcat(libs, "libvoyeur-close" LIB_SUFFIX, LIBS_SIZE);
    prev = 1;
  }

  free(cwd);

  return libs;
}

// TODO: This is actually just the number of different observable activities.
// (Plus room for a terminating null.)
#define OPTS_SIZE 4

char* requested_opts(voyeur_context* context)
{
  char* opts = calloc(1, OPTS_SIZE);
  opts[0] = voyeur_encode_options(context->exec_opts);
  opts[1] = voyeur_encode_options(context->open_opts);
  opts[2] = voyeur_encode_options(context->close_opts);
  return opts;
}

int voyeur_exec(voyeur_context_t ctx,
                const char* path,
                char* const argv[],
                char* const envp[])
{
  struct sockaddr_un sockinfo;
  voyeur_context* context = (voyeur_context*) ctx;

  // Prepare the server. We need to do this in advance both to avoid
  // racing and so that so we can include the socket path in the
  // environment variables.
  int server_sock = voyeur_create_server_socket(&sockinfo);
  
  pid_t child_pid;
  if ((child_pid = fork()) == 0) {
    // Add libvoyeur-specific environment variables.
    char* libs = requested_libs(context);
    char* opts = requested_opts(context);
    char** newenvp = voyeur_augment_environment(envp, libs, opts, sockinfo.sun_path);

    // Run the child process. This will never return.
    run_child(path, argv, newenvp);
    return 0;
  } else {
    // Run the server.
    int child_pipe_output = start_waitpid_thread(child_pid);
    int res = run_server(context, server_sock, child_pipe_output);

    // Clean up the socket file.
    TRY(unlink, sockinfo.sun_path);

    return res;
  }
}
