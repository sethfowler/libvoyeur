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
#include "event.h"
#include "net.h"

#define TRY(_f, ...)                            \
  do {                                          \
    if (_f(__VA_ARGS__) < 0) {                  \
      perror(#_f);                              \
      exit(EXIT_FAILURE);                       \
    }                                           \
  } while (0)

voyeur_context_t voyeur_context_create()
{
  voyeur_context* ctx = calloc(1, sizeof(voyeur_context));
  return (voyeur_context_t) ctx;
}

void voyeur_context_destroy(voyeur_context_t ctx)
{
  free((voyeur_context*) ctx);
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
          } else {
            voyeur_handle_event(context, type, fd);
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
    char* libs = voyeur_requested_libs(context);
    char* opts = voyeur_requested_opts(context);
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
