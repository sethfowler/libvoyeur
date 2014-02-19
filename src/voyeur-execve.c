#include <dlfcn.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

// FOR NETWORK
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
// FOR NETWORK

#include "env.h"
#include "net.h"

// From dyld-interposing.h:
#define DYLD_INTERPOSE(_replacment,_replacee) \
  __attribute__((used)) static struct{ const void* replacment; const void* replacee; } _interpose_##_replacee \
  __attribute__ ((section ("__DATA,__interpose"))) = { (const void*)(unsigned long)&_replacment, (const void*)(unsigned long)&_replacee }; 


typedef int (*execve_fptr_t)(const char*, char* const[], char* const []);
static char voyeur_execve_initialized = 0;
//static execve_fptr_t voyeur_execve_next = NULL; // Still need this for Linux...
static const char* voyeur_execve_sockpath = NULL;
static int voyeur_execve_sock = 0;

int create_client_socket(const char* sockpath)
{
  struct sockaddr_un sockinfo;

  // Configure a unix domain socket at a temporary path.
  memset(&sockinfo, 0, sizeof(struct sockaddr_un));
  sockinfo.sun_family = AF_UNIX;
  strncpy(sockinfo.sun_path,
          sockpath,
          sizeof(sockinfo.sun_path) - 1);

  // Connect to the server.
  int client_sock = socket(AF_UNIX, SOCK_STREAM, 0);
  if (connect(client_sock,
              (struct sockaddr*) &sockinfo,
              sizeof(struct sockaddr_un)) < 0) {
    perror("connect");
    exit(EXIT_FAILURE);
  }
  
  return client_sock;
}

int voyeur_execve(const char* path, char* const argv[], char* const envp[]);
DYLD_INTERPOSE(voyeur_execve, execve)

int voyeur_execve(const char* path, char* const argv[], char* const envp[])
{
  if (!voyeur_execve_initialized) {
    voyeur_execve_sockpath = getenv("LIBVOYEUR_SOCKET");
    if (voyeur_execve_sockpath == NULL)
      printf("No LIBVOYEUR_SOCKET set\n");
    else
      printf("LIBVOYEUR_SOCKET = %s\n", voyeur_execve_sockpath);

    voyeur_execve_sock = create_client_socket(voyeur_execve_sockpath);
    //voyeur_execve_next = (execve_fptr_t) dlsym(RTLD_NEXT, "execve"); // Linux...
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

  // Add libvoyeur-specific environment variables.
  char** newenvp = augment_environment(envp, voyeur_execve_sockpath);

  // Pass through the call to the real execve.
  //return voyeur_execve_next(path, argv, newenvp);  // Linux...
  return execve(path, argv, newenvp);
}
