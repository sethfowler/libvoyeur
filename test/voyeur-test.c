#include <stdio.h>
#include <voyeur.h>

void print_test_header(char* header)
{
  printf("\n");
  printf("==============================\n");
  printf("Test: %s\n", header);
  printf("==============================\n");
}

void print_test_footer()
{
  printf("==============================\n");
}

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

void test_execve()
{
  voyeur_context_t ctx = voyeur_context_create();
  voyeur_add_exec_interest(ctx, exec_callback);

  char* path   = "./test-execve";
  char* argv[] = { path, NULL };
  char* envp[] = { NULL };

  print_test_header("execve");
  voyeur_observe(ctx, path, argv, envp);
  print_test_footer();

  voyeur_context_destroy(ctx);
}

void test_execve_recursive()
{
  voyeur_context_t ctx = voyeur_context_create();
  voyeur_add_exec_interest(ctx, exec_callback);

  char* path   = "./test-execve-recursive";
  char* argv[] = { path, NULL };
  char* envp[] = { NULL };

  print_test_header("execve-recursive");
  voyeur_observe(ctx, path, argv, envp);
  print_test_footer();

  voyeur_context_destroy(ctx);
}

int main(int argc, char** argv)
{
  test_execve();
  test_execve_recursive();
  return 0;
}
