#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef __linux__
// For strlcpy/strlcat.
#include <bsd/string.h>
#endif

void run_test()
{
  int fd;
  char path[256];
  strlcpy(path, "/tmp/voyeur-test-open-XXXXXXXXX", 256);
  mktemp(path);

  fd = open(path, O_CREAT, 0777);
  close(fd);

  fd = open(path, O_RDONLY);
  close(fd);

  unlink(path);
}

int main(int argc, char** argv)
{
  run_test();
  return 0;
}
