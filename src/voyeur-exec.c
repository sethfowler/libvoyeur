#ifndef __APPLE__
#define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "dyld.h"
#include "env.h"
#include "net.h"

typedef int (*execve_fptr_t)(const char*, char* const[], char* const []);

int VOYEUR_FUNC(execve)(const char* path, char* const argv[], char* const envp[]);
VOYEUR_INTERPOSE(execve)

int VOYEUR_FUNC(execve)(const char* path, char* const argv[], char* const envp[])
{
  // In the case of exec we don't bother caching anything, since exec
  // will wipe out this whole process image anyway.
  const char* sockpath = getenv("LIBVOYEUR_SOCKET");
  if (sockpath == NULL)
    printf("No LIBVOYEUR_SOCKET set\n");
  else
    printf("LIBVOYEUR_SOCKET = %s\n", sockpath);

  const char* libs = getenv("LIBVOYEUR_LIBS");
  if (libs == NULL)
    printf("No LIBVOYEUR_LIBS set\n");
  else
    printf("LIBVOYEUR_LIBS = %s\n", libs);

  // Write the event to the socket.
  int sock = create_client_socket(sockpath);

  voyeur_write_event_type(sock, VOYEUR_EVENT_EXEC);
  voyeur_write_string(sock, path, 0);

  int argc = 0;
  while (argv[argc]) {
    ++argc;
  }
  voyeur_write_int(sock, argc);
  for (int i = 0 ; i < argc ; ++i) {
    voyeur_write_string(sock, argv[i], 0);
  }

  int envc = 0;
  while (envp[envc]) {
    ++envc;
  }
  voyeur_write_int(sock, envc);
  for (int i = 0 ; i < envc ; ++i) {
    voyeur_write_string(sock, envp[i], 0);
  }

  // We might as well close the socket since there's no chance we'll
  // ever be called a second time by the same process. (Even if the
  // exec fails, generally the fork'd process will just bail.)
  close(sock);

  // Add libvoyeur-specific environment variables.
  char** newenvp = voyeur_augment_environment(envp, libs, sockpath);

  // Pass through the call to the real execve.
  VOYEUR_DECLARE_NEXT(execve_fptr_t, execve);
  VOYEUR_LOOKUP_NEXT(execve_fptr_t, execve);
  return VOYEUR_CALL_NEXT(execve, path, argv, newenvp);
}
