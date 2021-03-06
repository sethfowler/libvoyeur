#ifdef __linux__
#define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

pid_t run_test()
{
  char* path   = "./test-exec";
  char* argv[] = { path, "recursive exec", NULL };
  char* envp[] = { "envvar=val2", NULL };

  pid_t pid;
  if ((pid = fork()) == 0) {
    // We're the child process.
    execve(path, argv, envp);
    _exit(EXIT_FAILURE);
  }

  return pid;
}

int main(int argc, char** argv)
{
  // Start four child processes that will themselves call exec.
  pid_t pid1 = run_test();
  pid_t pid2 = run_test();
  pid_t pid3 = run_test();
  pid_t pid4 = run_test();

  // Wait for all child processes to finish.
  int status;
  waitpid(pid1, &status, 0);
  waitpid(pid2, &status, 0);
  waitpid(pid3, &status, 0);
  waitpid(pid4, &status, 0);

  return 0;
}
