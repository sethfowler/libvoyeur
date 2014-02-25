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

typedef int (*close_fptr_t)(int);
VOYEUR_STATIC_DECLARE_NEXT(close_fptr_t, close)

static pthread_mutex_t voyeur_close_mutex = PTHREAD_MUTEX_INITIALIZER;
static char voyeur_close_initialized = 0;
static uint8_t voyeur_close_opts = 0;
static int voyeur_close_sock = 0;

int VOYEUR_FUNC(close)(int fildes)
{
  pthread_mutex_lock(&voyeur_close_mutex);

  if (!voyeur_close_initialized) {
    const char* voyeur_close_sockpath = getenv("LIBVOYEUR_SOCKET");
    voyeur_close_opts = voyeur_decode_options(getenv("LIBVOYEUR_OPTS"),
                                              VOYEUR_EVENT_CLOSE);
    voyeur_close_sock = voyeur_create_client_socket(voyeur_close_sockpath);
    VOYEUR_LOOKUP_NEXT(close_fptr_t, close);
    voyeur_close_initialized = 1;
  }

  // Pass through the call to the real close.
  int retval = VOYEUR_CALL_NEXT(close, fildes);

  // Write the event to the socket.
  voyeur_write_event_type(voyeur_close_sock, VOYEUR_EVENT_CLOSE);
  voyeur_write_int(voyeur_close_sock, fildes);
  voyeur_write_int(voyeur_close_sock, retval);
  voyeur_write_pid(voyeur_close_sock, getpid());

  pthread_mutex_unlock(&voyeur_close_mutex);

  return retval;
}

VOYEUR_INTERPOSE(close)
