#include <stdio.h>
#include <voyeur.h>

void exec_callback(const char* path,
                   char* const argv[],
                   char* const envp[])
{
  printf("exec_callback called for path %s\n", path);

  for (int i = 0 ; argv[i] ; ++i)
    printf("arg[%d] = %s\n", i, argv[i]);

  for (int i = 0 ; envp[i] ; ++i)
    printf("env[%d] = %s\n", i, envp[i]);
}

void run_test()
{
  voyeur_context_t ctx = voyeur_context_create();
  voyeur_add_exec_interest(ctx, exec_callback);

  char* path   = "./test-execve";
  char* argv[] = { path, "hello world", NULL };
  char* envp[] = { NULL };
  printf("voyeur-test: about to call voyeur_observe...\n");
  voyeur_observe(ctx, path, argv, envp);
  printf("voyeur-test: done with voyeur_observe\n");

  voyeur_context_destroy(ctx);
}

int main(int argc, char** argv)
{
  run_test();
  return 0;
}
