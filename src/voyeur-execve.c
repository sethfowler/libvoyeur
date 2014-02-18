#include <dlfcn.h>
#include <stdlib.h>
#include <stdio.h>

// FOR NETWORK
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
// FOR NETWORK

#include <voyeur/net.h>

typedef int (*execve_fptr_t)(const char*, char* const[], char* const []);
static char voyeur_execve_initialized = 0;
static execve_fptr_t voyeur_execve_next = NULL;
static int voyeur_execve_sock = 0;

int create_client_socket(const char* sockpath)
{
  int client_sock;
  struct sockaddr_un sockinfo;
  socklen_t socklen;

  // Configure a unix domain socket at a temporary path.
  sockinfo.sun_family = AF_UNIX;
  strcpy(sockinfo.sun_path, sockpath);
  socklen = (socklen_t) (strlen(sockinfo.sun_path) +
                         sizeof(sockinfo.sun_family));

  // Connect to the server.
  client_sock = socket(AF_UNIX, SOCK_STREAM, 0);
  if (connect(client_sock, (struct sockaddr*) &sockinfo, socklen) < 0) {
    perror("connect");
    exit(EXIT_FAILURE);
  }
  
  return client_sock;
}

int execve(const char* path, char* const argv[], char* const envp[])
{
  if (!voyeur_execve_initialized) {
    const char* lvsocket = getenv("LIBVOYEUR_SOCKET");
    if (lvsocket == NULL) printf("No LIBVOYEUR_SOCKET set\n");
    else                  printf("LIBVOYEUR_SOCKET = %s\n", lvsocket);

    voyeur_execve_sock = create_client_socket(lvsocket);
    voyeur_execve_next = (execve_fptr_t) dlsym(RTLD_NEXT, "execve");
  }
  
  // Write the event to the socket.
  voyeur_write_event_type(voyeur_execve_sock, VOYEUR_EVENT_EXEC);
  voyeur_write_string(voyeur_execve_sock, path, 0);

  int argc = 0;
  while (argv[argc]) {
    ++argc;
  }
  voyeur_write_int(voyeur_execve_sock, argc);
  for (int i = 0 ; i < argc ; ++i) {
    voyeur_write_string(voyeur_execve_sock, argv[i], 0);
  }

  int envc = 0;
  while (envp[envc]) {
    ++envc;
  }
  voyeur_write_int(voyeur_execve_sock, envc);
  for (int i = 0 ; i < envc ; ++i) {
    voyeur_write_string(voyeur_execve_sock, envp[i], 0);
  }

  // Pass through the call to the real execve.
  return voyeur_execve_next(path, argv, envp);
}
