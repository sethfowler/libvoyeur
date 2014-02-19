#include <dlfcn.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

#include "dyld.h"
#include "env.h"
#include "net.h"

//typedef int (*open_fptr_t)(const char*, int, ...);
//static open_fptr_t voyeur_open_next = NULL; // Still need this for Linux...

static char voyeur_open_initialized = 0;
static const char* voyeur_open_sockpath = NULL;
static int voyeur_open_sock = 0;

int voyeur_open(const char* path, int oflag, ...);
DYLD_INTERPOSE(voyeur_open, open)

int voyeur_open(const char* path, int oflag, ...)
{
  if (!voyeur_open_initialized) {
    voyeur_open_sockpath = getenv("LIBVOYEUR_SOCKET");
    if (voyeur_open_sockpath == NULL)
      printf("No LIBVOYEUR_SOCKET set\n");
    else
      printf("LIBVOYEUR_SOCKET = %s\n", voyeur_open_sockpath);

    //voyeur_open_next = (execve_fptr_t) dlsym(RTLD_NEXT, "execve"); // Linux...

    voyeur_open_sock = create_client_socket(voyeur_open_sockpath);
  }

  // Extract the mode argument if necessary.
  mode_t mode;
  if (oflag & O_CREAT) {
    va_list args;
    va_start(args, oflag);
    // Note that 'int' is used instead of 'mode_t' due to warnings
    // that mode_t, being in reality a short on OS X, is of promotable
    // type, which is verboten for va_arg.
    mode = va_arg(args, int);
    va_end(args);
  }
  
  // Write the event to the socket.
  voyeur_write_event_type(voyeur_open_sock, VOYEUR_EVENT_OPEN);
  voyeur_write_string(voyeur_open_sock, path, 0);
  voyeur_write_int(voyeur_open_sock, oflag);

  if (oflag & O_CREAT) {
    voyeur_write_int(voyeur_open_sock, (int) mode);
  } else {
    voyeur_write_int(voyeur_open_sock, 0);
  }

  // Pass through the call to the real open.
  //return voyeur_open_next(path, oflag, mode);  // Linux...
  if (oflag & O_CREAT) {
    return open(path, oflag, mode);
  } else {
    return open(path, oflag);
  }
}
