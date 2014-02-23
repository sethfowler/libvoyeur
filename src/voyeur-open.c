#ifndef __APPLE__
#define _GNU_SOURCE
#endif

#include <fcntl.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include "dyld.h"
#include "env.h"
#include "net.h"

typedef int (*open_fptr_t)(const char*, int, ...);
VOYEUR_STATIC_DECLARE_NEXT(open_fptr_t, open)

static pthread_mutex_t voyeur_open_mutex = PTHREAD_MUTEX_INITIALIZER;
static char voyeur_open_initialized = 0;
static uint8_t voyeur_open_opts = 0;
static int voyeur_open_sock = 0;

int VOYEUR_FUNC(open)(const char* path, int oflag, ...);
VOYEUR_INTERPOSE(open)

int VOYEUR_FUNC(open)(const char* path, int oflag, ...)
{
  pthread_mutex_lock(&voyeur_open_mutex);
  
  if (!voyeur_open_initialized) {
    const char* voyeur_open_sockpath = getenv("LIBVOYEUR_SOCKET");
    voyeur_open_opts = voyeur_decode_options(getenv("LIBVOYEUR_OPTS"),
                                             VOYEUR_EVENT_OPEN);
    voyeur_open_sock = voyeur_create_client_socket(voyeur_open_sockpath);
    VOYEUR_LOOKUP_NEXT(open_fptr_t, open);
    voyeur_open_initialized = 1;
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
  
  // Pass through the call to the real open.
  int retval;
  if (oflag & O_CREAT) {
    retval = VOYEUR_CALL_NEXT(open, path, oflag, mode);
  } else {
    retval = VOYEUR_CALL_NEXT(open, path, oflag);
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

  voyeur_write_int(voyeur_open_sock, retval);

  if (voyeur_open_opts & OBSERVE_OPEN_CWD) {
    char* cwd = getcwd(NULL, 0);
    voyeur_write_string(voyeur_open_sock, cwd, 0);
    free(cwd);
  }

  pthread_mutex_unlock(&voyeur_open_mutex);

  return retval;
}
