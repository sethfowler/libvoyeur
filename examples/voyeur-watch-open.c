#include <stdio.h>
#include <voyeur.h>

extern char** environ;

void open_callback(const char* path,
                   int oflag,
                   mode_t mode,
                   const char* cwd,
                   int retval,
                   pid_t pid,
                   void* userdata)
{
  fprintf(stderr, "[OPEN] %s (flags %d) (mode %o) (in %s) (rv %d) (pid %u)\n",
                  path, oflag, (unsigned) mode, cwd, retval, pid);
}

void close_callback(int fd, int retval, pid_t pid, void* userdata)
{
  fprintf(stderr, "[CLOSE] %d (rv %d) (pid %u)\n", fd, retval, pid);
}

void go(int argc, char** argv)
{
  voyeur_context_t ctx = voyeur_context_create();
  voyeur_observe_open(ctx,
                      OBSERVE_OPEN_CWD,
                      open_callback,
                      NULL);
  voyeur_observe_close(ctx,
                       OBSERVE_CLOSE_DEFAULT,
                       close_callback,
                       NULL);

  voyeur_exec(ctx, argv[0], argv, environ);

  voyeur_context_destroy(ctx);
}

int main(int argc, char** argv)
{
  if (argc == 1) {
    printf("Usage: %s [command]\n", argv[0]);
    return -1;
  }
  
  go(argc - 1, argv + 1);

  return 0;
}
