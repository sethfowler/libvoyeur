#include <stdio.h>
#include <voyeur.h>

extern char** environ;

void exec_callback(const char* path,
                   char* const argv[],
                   char* const envp[],
                   const char* cwd,
                   void* userdata)
{
  fprintf(stderr, "[EXEC] %s", path);

  if (argv[0] != NULL) {
    for (int i = 1 ; argv[i] ; ++i)
      fprintf(stderr, " %s", argv[i]);
  }

  fprintf(stderr, " (in %s)\n", cwd);
}

void go(int argc, char** argv)
{
  voyeur_context_t ctx = voyeur_context_create();
  voyeur_observe_exec(ctx,
                      OBSERVE_EXEC_CWD,
                      exec_callback,
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
