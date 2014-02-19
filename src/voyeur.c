#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <voyeur.h>
#include <voyeur/net.h>

typedef struct {
  voyeur_exec_callback exec_cb;
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

void voyeur_add_exec_interest(voyeur_context_t ctx,
                              voyeur_exec_callback callback)
{
  ((voyeur_context*) ctx)->exec_cb = callback;
}

char** augment_environment(char* const* envp,
                           char* sockpath,
                           char** dyld_insert_libraries_env,
                           char** libvoyeur_socket_env)
{
  // Determine the size of the original environment.
  // TODO: Check if DYLD_INSERT_LIBRARIES already exists.
  unsigned envlen = 0;
  for ( ; envp[envlen] != NULL ; ++envlen);

  // Allocate the new environment variables and store in the context
  // to make it possible to free them later.
  *dyld_insert_libraries_env = malloc(sizeof(char) * 1024);
  strlcpy(*dyld_insert_libraries_env, "DYLD_INSERT_LIBRARIES=", 1024);
  strlcat(*dyld_insert_libraries_env, "libvoyeur-execve.dylib", 1024);

  *libvoyeur_socket_env = malloc(sizeof(char) * 1024);
  strlcpy(*libvoyeur_socket_env, "LIBVOYEUR_SOCKET=", 1024);
  strlcat(*libvoyeur_socket_env, sockpath, 1024);

  // Allocate a new environment, including additional space for the 2
  // extra environment variables we'll add and a terminating NULL.
  char** newenvp = malloc(sizeof(char*) * (envlen + 3));
  memcpy(newenvp, envp, sizeof(char*) * envlen);
  newenvp[envlen] = *dyld_insert_libraries_env;
  newenvp[envlen + 1] = *libvoyeur_socket_env;
  newenvp[envlen + 2] = NULL;

  return newenvp;
}

int create_server_socket(struct sockaddr_un* sockinfo)
{
  // Configure a unix domain socket at a temporary path.
  memset(sockinfo, 0, sizeof(struct sockaddr_un));
  sockinfo->sun_family = AF_UNIX;
  strncpy(sockinfo->sun_path,
          "/tmp/voyeur-socket-XXXXXXXXX",
          sizeof(sockinfo->sun_path) - 1);
  mktemp(sockinfo->sun_path);
  unlink(sockinfo->sun_path);

  // Start the server.
  int server_sock = socket(AF_UNIX, SOCK_STREAM, 0);
  if (bind(server_sock,
           (struct sockaddr*) sockinfo,
           sizeof(struct sockaddr_un)) < 0) {
    perror("bind");
    exit(EXIT_FAILURE);
  }
  if (listen(server_sock, 5) < 0) {
    perror("listen");
    exit(EXIT_FAILURE);
  }

  return server_sock;
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

  printf("Client connected.\n");
  
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
    context->exec_cb(path, argv, envp);
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
          FD_CLR(fd, &active_fd_set);
        } else {
          // Got a voyeur event; dispatch to the appropriate handler.
          voyeur_event_type type;
          if (voyeur_read_event_type(fd, &type) < 0) {
            close(fd);
            FD_CLR(fd, &active_fd_set);
          } else if (type == VOYEUR_EVENT_EXEC) {
            handle_exec(context, fd);
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

int voyeur_observe(voyeur_context_t ctx,
                   const char* path,
                   char* const argv[],
                   char* const envp[])
{
  struct sockaddr_un sockinfo;
  voyeur_context* context = (voyeur_context*) ctx;

  // Prepare the server. We need to do this in advance so we can
  // include the socket path in the environment variables.
  int server_sock = create_server_socket(&sockinfo);
  
  // Add libvoyeur-specific environment variables.
  // We return some of the environment variables so we can free them later.
  char* libvoyeur_socket_env;
  char* dyld_insert_libraries_env;
  char** newenvp = augment_environment(envp,
                                       sockinfo.sun_path,
                                       &libvoyeur_socket_env,
                                       &dyld_insert_libraries_env);

  pid_t child_pid;
  if ((child_pid = vfork()) == 0) {
    run_child(path, argv, newenvp);
    return 0;  // Never reached.
  } else {
    // We're done with the environment variables, so free them.
    free(newenvp);
    free(libvoyeur_socket_env);
    free(dyld_insert_libraries_env);

    // Run the server.
    int child_pipe_output = start_waitpid_thread(child_pid);
    int res = run_server(context, server_sock, child_pipe_output);

    // Clean up the socket file.
    if (unlink(sockinfo.sun_path) < 0) {
      perror("unlink");
      printf("Was trying to unlink %s\n", sockinfo.sun_path);
      exit(EXIT_FAILURE);
    }

    return res;
  }
}
