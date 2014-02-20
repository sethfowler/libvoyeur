#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#ifdef __linux__
// For strlcpy/strlcat.
#include <bsd/string.h>
#endif

void run_exec_test()
{
  char* path   = "/bin/echo";
  char* argv[] = { path, "hello world", NULL };
  char* envp[] = { NULL };

  pid_t child_pid;
  if ((child_pid = fork()) == 0) {
    execve(path, argv, envp);
  } else {
    int status;
    waitpid(child_pid, &status, 0);
  }
}

void run_open_test()
{
  int fd;
  char path[256];
  strlcpy(path, "/tmp/voyeur-test-open-XXXXXXXXX", 256);
  mktemp(path);

  fd = open(path, O_CREAT, 0777);
  close(fd);
  unlink(path);
}

int main(int argc, char** argv)
{
  run_exec_test();
  run_open_test();
  return 0;
}
