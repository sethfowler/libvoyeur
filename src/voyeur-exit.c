#ifndef __APPLE__
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <unistd.h>

#include "dyld.h"
#include "env.h"
#include "net.h"

static char did_exit_already = 0;

//////////////////////////////////////////////////
// Shared code for all exit*() functions.
//////////////////////////////////////////////////

static void write_exit_event(int status)
{
  if (!did_exit_already) {
    did_exit_already = 1;

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
  }
}


//////////////////////////////////////////////////
// exit*() variants.
//////////////////////////////////////////////////

// There are a lot of variants here because none of them seem to be perfectly
// reliable on Linux. We are still likely to miss the case where a process gets
// signaled, unfortunately.

static void voyeur_exit_handler(int status, void* unused)
{
  write_exit_event(status);
}

__attribute__((constructor)) void voyeur_init_exit_handler()
{
  on_exit(voyeur_exit_handler, 0);
}

typedef void (*exit_fptr_t)(int);

void VOYEUR_FUNC(exit)(int status)
{
  write_exit_event(status);

  // Pass through the call to the real exit.
  VOYEUR_DECLARE_NEXT(exit_fptr_t, exit);
  VOYEUR_LOOKUP_NEXT(exit_fptr_t, exit);
  return VOYEUR_CALL_NEXT(exit, status);
}

VOYEUR_INTERPOSE(exit)

void VOYEUR_FUNC(_exit)(int status)
{
  write_exit_event(status);

  // Pass through the call to the real _exit.
  VOYEUR_DECLARE_NEXT(exit_fptr_t, _exit);
  VOYEUR_LOOKUP_NEXT(exit_fptr_t, _exit);
  return VOYEUR_CALL_NEXT(_exit, status);
}

VOYEUR_INTERPOSE(_exit)

void VOYEUR_FUNC(_Exit)(int status)
{
  write_exit_event(status);

  // Pass through the call to the real _Exit.
  VOYEUR_DECLARE_NEXT(exit_fptr_t, _Exit);
  VOYEUR_LOOKUP_NEXT(exit_fptr_t, _Exit);
  return VOYEUR_CALL_NEXT(_Exit, status);
}

VOYEUR_INTERPOSE(_Exit)

void VOYEUR_FUNC(exit_group)(int status)
{
  write_exit_event(status);

  // Pass through the call to the real exit_group.
  VOYEUR_DECLARE_NEXT(exit_fptr_t, exit_group);
  VOYEUR_LOOKUP_NEXT(exit_fptr_t, exit_group);
  return VOYEUR_CALL_NEXT(exit_group, status);
}

VOYEUR_INTERPOSE(exit_group)
