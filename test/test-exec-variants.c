#include <spawn.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

static char* new_path        = "/bin/echo";
static char* new_envp[]      = { NULL };
static char* new_search_path = "/usr/bin:/bin";

void run_execve_test()
{
  static char* new_argv[] = { "/bin/echo", "execve", NULL };
  pid_t child_pid;
  if ((child_pid = fork()) == 0) {
    execve(new_path, new_argv, new_envp);
  } else {
    int status;
    waitpid(child_pid, &status, 0);
  }
}

void run_execl_test()
{
  static char* new_argv[] = { "/bin/echo", "execl", NULL };
  pid_t child_pid;
  if ((child_pid = fork()) == 0) {
    execl(new_path, new_argv[0], new_argv[1], NULL);
  } else {
    int status;
    waitpid(child_pid, &status, 0);
  }
}

void run_execle_test()
{
  static char* new_argv[] = { "/bin/echo", "execle", NULL };
  pid_t child_pid;
  if ((child_pid = fork()) == 0) {
    execle(new_path, new_argv[0], new_argv[1], NULL, new_envp);
  } else {
    int status;
    waitpid(child_pid, &status, 0);
  }
}

void run_execlp_test()
{
  static char* new_argv[] = { "/bin/echo", "execlp", NULL };
  pid_t child_pid;
  if ((child_pid = fork()) == 0) {
    execlp(new_path, new_argv[0], new_argv[1], NULL);
  } else {
    int status;
    waitpid(child_pid, &status, 0);
  }
}

void run_execv_test()
{
  static char* new_argv[] = { "/bin/echo", "execv", NULL };
  pid_t child_pid;
  if ((child_pid = fork()) == 0) {
    execv(new_path, new_argv);
  } else {
    int status;
    waitpid(child_pid, &status, 0);
  }
}

void run_execvp_test()
{
  static char* new_argv[] = { "/bin/echo", "execvp", NULL };
  pid_t child_pid;
  if ((child_pid = fork()) == 0) {
    execvp(new_path, new_argv);
  } else {
    int status;
    waitpid(child_pid, &status, 0);
  }
}

void run_execvP_test()
{
#ifdef __linux__

  // No execvP on this platform.
  run_execvp_test();

#else

  static char* new_argv[] = { "/bin/echo", "execvP", NULL };
  pid_t child_pid;
  if ((child_pid = fork()) == 0) {
    execvP(new_path, new_search_path, new_argv);
  } else {
    int status;
    waitpid(child_pid, &status, 0);
  }

#endif
}

void run_system_test()
{
  system("/bin/echo system");
}

void run_posix_spawn_test()
{
  static char* new_argv[] = { "/bin/echo", "posix_spawn", NULL };
  pid_t pid;
  posix_spawn(&pid, new_path, NULL, NULL, new_argv, new_envp);
}

void run_posix_spawnp_test()
{
  static char* new_argv[] = { "/bin/echo", "posix_spawnp", NULL };
  pid_t pid;
  posix_spawnp(&pid, new_path, NULL, NULL, new_argv, new_envp);
}

int main(int argc, char** argv)
{
  run_execve_test();
  run_execl_test();
  run_execle_test();
  run_execlp_test();
  run_execv_test();
  run_execvp_test();
  run_execvP_test();
  run_system_test();
  run_posix_spawn_test();
  run_posix_spawnp_test();
  return 0;
}
