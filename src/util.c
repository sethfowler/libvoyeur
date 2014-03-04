#include <unistd.h>

#include "util.h"

extern void voyeur_log(const char* str);

void voyeur_request_debug(const char* reason)
{
  perror(reason);
  char keep_looping = 1;
  while (keep_looping) {
    printf("ATTACH DEBUGGER TO %u\n", (unsigned) getpid());
    sleep(10);
  }
}
