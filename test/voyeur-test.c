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

void test_exec()
{
  voyeur_context_t ctx = voyeur_context_create();
  voyeur_add_exec_interest(ctx, exec_callback);

  char* path   = "./test-exec";
  char* argv[] = { path, NULL };
  char* envp[] = { NULL };

  print_test_header("exec");
  voyeur_observe(ctx, path, argv, envp);
  print_test_footer();

  voyeur_context_destroy(ctx);
}

void test_exec_recursive()
{
  voyeur_context_t ctx = voyeur_context_create();
  voyeur_add_exec_interest(ctx, exec_callback);

  char* path   = "./test-exec-recursive";
  char* argv[] = { path, NULL };
  char* envp[] = { NULL };

  print_test_header("exec-recursive");
  voyeur_observe(ctx, path, argv, envp);
  print_test_footer();

  voyeur_context_destroy(ctx);
}

int main(int argc, char** argv)
{
  test_exec();
  test_exec_recursive();
  return 0;
}