#ifndef __APPLE__
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <unistd.h>

#include "dyld.h"
#include "env.h"
#include "net.h"

typedef void (*_exit_fptr_t)(int);
VOYEUR_STATIC_DECLARE_NEXT(_exit_fptr_t, _exit)

void VOYEUR_FUNC(_exit)(int status)
{
  // In the case of exit we don't bother caching anything; we are about to exit,
  // after all!
  const char* sockpath = getenv("LIBVOYEUR_SOCKET");
  int sock = voyeur_create_client_socket(sockpath);
  if (sock >= 0) {
    voyeur_write_msg_type(sock, VOYEUR_MSG_EVENT);
    voyeur_write_event_type(sock, VOYEUR_EVENT_EXIT);
    voyeur_write_int(sock, status);
    voyeur_write_pid(sock, getpid());
    voyeur_write_pid(sock, getppid());

    // We might as well close the socket since there's no chance we'll
    // ever be called a second time by the same process.
    voyeur_write_msg_type(sock, VOYEUR_MSG_DONE);
    voyeur_close_socket(sock);
  }

  // Pass through the call to the real _exit.
  VOYEUR_DECLARE_NEXT(_exit_fptr_t, _exit);
  VOYEUR_LOOKUP_NEXT(_exit_fptr_t, _exit);
  return VOYEUR_CALL_NEXT(_exit, status);
}

VOYEUR_INTERPOSE(_exit)
