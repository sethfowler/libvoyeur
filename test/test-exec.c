#include <unistd.h>

void run_test()
{
  char* path   = "/bin/echo";
  char* argv[] = { path, "hello world", NULL };
  char* envp[] = { NULL };
  execve(path, argv, envp);
}

int main(int argc, char** argv)
{
  run_test();
  return 0;
}