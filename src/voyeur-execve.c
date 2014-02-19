#include <dlfcn.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

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

    //voyeur_execve_next = (execve_fptr_t) dlsym(RTLD_NEXT, "execve"); // Linux...
  }
  
  // Write the event to the socket.
  int voyeur_execve_sock = create_client_socket(voyeur_execve_sockpath);

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

  close(voyeur_execve_sock);

  // Add libvoyeur-specific environment variables.
  char** newenvp = augment_environment(envp, voyeur_execve_sockpath);

  // Pass through the call to the real execve.
  //return voyeur_execve_next(path, argv, newenvp);  // Linux...
  return execve(path, argv, newenvp);
}
