#include <errno.h>
#include <pthread.h>
#include <spawn.h>
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
#include "event.h"
#include "net.h"
#include "util.h"

typedef struct {
  struct sockaddr_un sockinfo;
  int server_sock;
  void* env_buf;
} server_state;

voyeur_context_t voyeur_context_create()
{
  voyeur_context* ctx = calloc(1, sizeof(voyeur_context));
  return (voyeur_context_t) ctx;
}

void voyeur_context_destroy(voyeur_context_t ctx)
{
  voyeur_context* context = (voyeur_context*) ctx;

  if (context->resource_path) {
    free(context->resource_path);
  }

  if (context->server_state) {
    server_state* state = (server_state*) context->server_state;
    free(state->env_buf);
    free(state);
  }
  
  free(context);
}

void voyeur_set_resource_path(voyeur_context_t ctx,
                              const char* path)
{
  voyeur_context* context = (voyeur_context*) ctx;
  context->resource_path = calloc(1, strnlen(path, 4096));
  strlcat(context->resource_path, path, 4096);
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

  voyeur_close_socket(arg->child_pipe_input);
  free(arg);

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

int accept_connection(int server_sock)
{
  struct sockaddr_un client_info;
  socklen_t client_info_len = sizeof(struct sockaddr_un);

  int client_sock =
    accept(server_sock, (struct sockaddr *) &client_info, &client_info_len);
  CHECK(client_sock, "accept");
  
  return client_sock;
}

int handle_message(voyeur_context* context, int sock)
{
  voyeur_msg_type msgtype;
  if (voyeur_read_msg_type(sock, &msgtype) < 0) {
    return -1;
  }

  if (msgtype == VOYEUR_MSG_DONE) {
    // The client is done sending messages on this socket.
    return -1;
  } else if (msgtype == VOYEUR_MSG_EVENT) {
    voyeur_event_type type;
    if (voyeur_read_event_type(sock, &type) < 0) {
      return -1;
    }

    // Got a voyeur event; dispatch to the appropriate handler.
    voyeur_handle_event(context, type, sock);
    return 0;
  } else {
    // Got an unknown message type.
    voyeur_log("Unknown message type\n");
    return -1;
  }
}

int run_server(voyeur_context* context,
               int server_sock,
               int child_pipe_output)
{
  fd_set active_fd_set, read_fd_set, error_fd_set;
  FD_ZERO(&active_fd_set);
  FD_SET(server_sock, &active_fd_set);
  FD_SET(child_pipe_output, &active_fd_set);
  
  int child_exited = 0;
  int child_status = 0;
  
  while (!child_exited) {
    // Block until input arrives.
    FD_COPY(&active_fd_set, &read_fd_set);
    FD_COPY(&active_fd_set, &error_fd_set);
    if (select(FD_SETSIZE, &read_fd_set, NULL, &error_fd_set, NULL) < 0) {
      if (errno == EAGAIN || errno == EINTR) {
        continue;  // This is a temporary error.
      } else {
        perror("select");
        break;     // This is unrecoverable.
      }
    }

    for (int fd = 0 ; fd < FD_SETSIZE ; ++fd) {
      if (FD_ISSET(fd, &error_fd_set)) {
        voyeur_log("Closed file descriptor due to error\n");
        voyeur_close_socket(fd);
        FD_CLR(fd, &active_fd_set);
      } else if (FD_ISSET(fd, &read_fd_set)) {
        if (fd == server_sock) {
          int client_sock = accept_connection(server_sock);
          FD_SET(client_sock, &active_fd_set);
        } else if (fd == child_pipe_output) {
          child_exited = 1;
          voyeur_read_int(fd, &child_status);
          voyeur_close_socket(fd);
          FD_CLR(fd, &active_fd_set);
        } else if (handle_message(context, fd) < 0) {
          voyeur_close_socket(fd);
          FD_CLR(fd, &active_fd_set);
        }
      }
    }
  }

  voyeur_close_socket(server_sock);

  // Clean up any stragglers.
  for (int fd = 0 ; fd < FD_SETSIZE ; ++fd) {
    if (FD_ISSET(fd, &active_fd_set)) {
      voyeur_close_socket(fd);
    }
  }
  
  if (WIFEXITED(child_status)) {
    return WEXITSTATUS(child_status);
  } else {
    fprintf(stderr, "libvoyeur: child process did not terminate normally\n");
    return -1;
  }
}

char** voyeur_prepare(voyeur_context_t ctx, char* const envp[])
{
  voyeur_context* context = (voyeur_context*) ctx;
  server_state* state = calloc(1, sizeof(server_state));
  context->server_state = (void*) state;

  // Prepare the server. We need to do this in advance both to avoid
  // racing and so that we can include the socket path in the
  // environment variables.
  state->server_sock = voyeur_create_server_socket(&state->sockinfo);
  
  // Add libvoyeur-specific environment variables.
  char* libs = voyeur_requested_libs(context);
  char* opts = voyeur_requested_opts(context);
  char** voyeur_envp = voyeur_augment_environment(envp, libs, opts,
                                                  state->sockinfo.sun_path,
                                                  &state->env_buf);

  return voyeur_envp;
}

int voyeur_start(voyeur_context_t ctx, pid_t child_pid)
{
  voyeur_context* context = (voyeur_context*) ctx;
  server_state* state = (server_state*) context->server_state;

  // Run the server.
  int child_pipe_output = start_waitpid_thread(child_pid);
  int res = run_server(context,
                       state->server_sock,
                       child_pipe_output);

  // Clean up the socket file.
  TRY(unlink, state->sockinfo.sun_path);
  char* last_slash = strrchr(state->sockinfo.sun_path, '/');
  if (last_slash) {
    *last_slash = '\0';
  }
  TRY(rmdir, state->sockinfo.sun_path);

  return res;
}

int voyeur_exec(voyeur_context_t ctx,
                const char* path,
                char* const argv[],
                char* const envp[])
{
  char** voyeur_envp = voyeur_prepare(ctx, envp);
  
  pid_t child_pid;
  if (posix_spawnp(&child_pid, path, NULL, NULL, argv, voyeur_envp) != 0) {
    free(voyeur_envp);
    return -1;
  }
  
  int retval = voyeur_start(ctx, child_pid);
  free(voyeur_envp);
  return retval;
}
