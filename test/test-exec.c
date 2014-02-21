#include <unistd.h>

void run_test()
{
  char* path   = "/bin/echo";
  char* argv[] = { path, "hello world", NULL };

#ifdef __APPLE__
  char* envp[] = { "DYLD_INSERT_LIBRARIES=libnull.dylib", NULL };
#else
  char* envp[] = { "LD_PRELOAD=./libnull.so", NULL };
#endif

  execve(path, argv, envp);
}

int main(int argc, char** argv)
{
  run_test();
  return 0;
}
