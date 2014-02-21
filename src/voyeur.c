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

#ifdef __darwin__
#define LIB_SUFFIX ".dylib"
#else
#define LIB_SUFFIX ".so"
#endif

typedef struct {
  voyeur_exec_callback exec_cb;
  void* exec_userdata;

  voyeur_open_callback open_cb;
  void* open_userdata;
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
                         voyeur_exec_callback callback,
                         void* userdata)
{
  voyeur_context* context = (voyeur_context*) ctx;
  context->exec_cb = callback;
  context->exec_userdata = userdata;
}

void voyeur_observe_open(voyeur_context_t ctx,
                         voyeur_open_callback callback,
                         void* userdata)
{
  voyeur_context* context = (voyeur_context*) ctx;
  context->open_cb = callback;
  context->open_userdata = userdata;
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
  if (pipe(waitpid_pipe)) {
    perror("pipe");
    exit(EXIT_FAILURE);
  }

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
  if (voyeur_read_string(fd, &path, 0) < 0) {
    perror("read");
    exit(EXIT_FAILURE);
  }

  // Read the arguments.
  int argc;
  if (voyeur_read_int(fd, &argc) < 0) {
    perror("read");
    exit(EXIT_FAILURE);
  }

  char** argv = malloc(sizeof(char*) * (argc + 1));
  for (int i = 0 ; i < argc ; ++i) {
    char* arg;
    if (voyeur_read_string(fd, &arg, 0) < 0) {
      perror("read");
      exit(EXIT_FAILURE);
    }
    argv[i] = arg;
  }
  argv[argc] = NULL;

  // Read the environment.
  int envc;
  if (voyeur_read_int(fd, &envc) < 0) {
    perror("read");
    exit(EXIT_FAILURE);
  }

  char** envp = malloc(sizeof(char*) * (envc + 1));
  for (int i = 0 ; i < envc ; ++i) {
    char* envvar;
    if (voyeur_read_string(fd, &envvar, 0) < 0) {
      perror("read");
      exit(EXIT_FAILURE);
    }
    envp[i] = envvar;
  }
  envp[envc] = NULL;

  // Invoke the callback, if there is one.
  // Note that we always have to read the data, even if the callback
  // isn't present, so we can move on to the next event. In practice
  // that should never happen, so it's not worth worrying about.
  if (context->exec_cb) {
    context->exec_cb(path, argv, envp, context->exec_userdata);
  }

  // Free everything.
  free(path);

  for (int i = 0 ; i < argc ; ++i) {
    free(argv[i]);
  }
  free(argv);

  for (int i = 0 ; i < envc ; ++i) {
    free(envp[i]);
  }
  free(envp);
}

void handle_open(voyeur_context* context, int fd)
{
  // Read the path.
  char* path;
  if (voyeur_read_string(fd, &path, 0) < 0) {
    perror("read");
    exit(EXIT_FAILURE);
  }

  // Read the flags.
  int oflag;
  if (voyeur_read_int(fd, &oflag) < 0) {
    perror("read");
    exit(EXIT_FAILURE);
  }

  // Read the mode.
  int mode;
  if (voyeur_read_int(fd, &mode) < 0) {
    perror("read");
    exit(EXIT_FAILURE);
  }

  // Invoke the callback, if there is one.
  // Note that we always have to read the data, even if the callback
  // isn't present, so we can move on to the next event. In practice
  // that should never happen, so it's not worth worrying about.
  if (context->open_cb) {
    context->open_cb(path, oflag, (mode_t) mode, context->open_userdata);
  }

  // Free everything.
  free(path);
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
    if (select(FD_SETSIZE, &read_fd_set, NULL, NULL, NULL) < 0) {
      perror("select");
      exit(EXIT_FAILURE);
    }

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
#define LIB_PREFIX "/home/mfowler/Code/libvoyeur/build/"

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

  free(cwd);

  return libs;
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
  int server_sock = create_server_socket(&sockinfo);
  
  pid_t child_pid;
  if ((child_pid = fork()) == 0) {
    // Add libvoyeur-specific environment variables.
    char* libs = requested_libs(context);
    char** newenvp = voyeur_augment_environment(envp, libs, sockinfo.sun_path);

    // Run the child process. This will never return.
    run_child(path, argv, newenvp);
    return 0;
  } else {
    // Run the server.
    int child_pipe_output = start_waitpid_thread(child_pid);
    int res = run_server(context, server_sock, child_pipe_output);

    // Clean up the socket file.
    if (unlink(sockinfo.sun_path) < 0) {
      perror("unlink");
      exit(EXIT_FAILURE);
    }

    return res;
  }
}
