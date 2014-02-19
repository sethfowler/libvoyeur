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


//typedef int (*execve_fptr_t)(const char*, char* const[], char* const []);
//static execve_fptr_t voyeur_exec_next = NULL; // Still need this for Linux...

static char voyeur_exec_initialized = 0;
static const char* voyeur_exec_sockpath = NULL;

int voyeur_exec(const char* path, char* const argv[], char* const envp[]);
DYLD_INTERPOSE(voyeur_exec, execve)

int voyeur_exec(const char* path, char* const argv[], char* const envp[])
{
  if (!voyeur_exec_initialized) {
    voyeur_exec_sockpath = getenv("LIBVOYEUR_SOCKET");
    if (voyeur_exec_sockpath == NULL)
      printf("No LIBVOYEUR_SOCKET set\n");
    else
      printf("LIBVOYEUR_SOCKET = %s\n", voyeur_exec_sockpath);

    //voyeur_exec_next = (execve_fptr_t) dlsym(RTLD_NEXT, "execve"); // Linux...
  }
  
  // Write the event to the socket.
  int voyeur_exec_sock = create_client_socket(voyeur_exec_sockpath);

  voyeur_write_event_type(voyeur_exec_sock, VOYEUR_EVENT_EXEC);
  voyeur_write_string(voyeur_exec_sock, path, 0);

  int argc = 0;
  while (argv[argc]) {
    ++argc;
  }
  voyeur_write_int(voyeur_exec_sock, argc);
  for (int i = 0 ; i < argc ; ++i) {
    voyeur_write_string(voyeur_exec_sock, argv[i], 0);
  }

  int envc = 0;
  while (envp[envc]) {
    ++envc;
  }
  voyeur_write_int(voyeur_exec_sock, envc);
  for (int i = 0 ; i < envc ; ++i) {
    voyeur_write_string(voyeur_exec_sock, envp[i], 0);
  }

  close(voyeur_exec_sock);

  // Add libvoyeur-specific environment variables.
  char** newenvp = augment_environment(envp, voyeur_exec_sockpath);

  // Pass through the call to the real execve.
  //return voyeur_exec_next(path, argv, newenvp);  // Linux...
  return execve(path, argv, newenvp);
}
