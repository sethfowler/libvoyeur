#include <stdio.h>
#include <voyeur.h>

void print_test_header(char* header)
{
  printf("\n");
  printf("==============================\n");
  printf("Test: %s\n", header);
  printf("==============================\n");
}

void print_test_footer(char result)
{
  printf("==============================\n");
  printf("%s\n", result ? "PASSED" : "FAILED");
  printf("==============================\n");
}

void exec_callback(const char* path,
                   char* const argv[],
                   char* const envp[],
                   const char* cwd,
                   void* userdata)
{
  printf("exec_callback called for path [%s]\n", path);

  for (int i = 0 ; argv[i] ; ++i)
    printf("arg[%d] = %s\n", i, argv[i]);

  if (envp) {
    for (int i = 0 ; envp[i] ; ++i)
      printf("env[%d] = %s\n", i, envp[i]);
  }

  if (cwd)
    printf("cwd = %s\n", cwd);

  char* result = (char*) userdata;
  *result = 1;
}

void exec_recursive_callback(const char* path,
                             char* const argv[],
                             char* const envp[],
                             const char* cwd,
                             void* userdata)
{
  printf("exec_callback called for path [%s]\n", path);

  for (int i = 0 ; argv[i] ; ++i)
    printf("arg[%d] = %s\n", i, argv[i]);

  if (envp) {
    for (int i = 0 ; envp[i] ; ++i)
      printf("env[%d] = %s\n", i, envp[i]);
  }

  if (cwd)
    printf("cwd = %s\n", cwd);

  unsigned* result = (unsigned*) userdata;
  *result += 1;
}

void open_callback(const char* path,
                   int oflag,
                   mode_t mode,
                   const char* cwd,
                   int retval,
                   void* userdata)
{
  printf("open_callback called for path [%s] oflag [%d] mode [%o] "
         "cwd [%s] rv [%d]\n",
         path, oflag, (int) mode, cwd ? cwd : "", retval);

  char* result = (char*) userdata;
  *result = 1;
}

void test_exec()
{
  char result = 0;
  voyeur_context_t ctx = voyeur_context_create();
  voyeur_observe_exec(ctx, OBSERVE_EXEC_CWD | OBSERVE_EXEC_ENV,
                      exec_callback, (void*) &result);

  char* path   = "./test-exec";
  char* argv[] = { path, NULL };
  char* envp[] = { NULL };

  print_test_header("exec");
  voyeur_exec(ctx, path, argv, envp);
  print_test_footer(result);

  voyeur_context_destroy(ctx);
}

void test_exec_recursive()
{
  unsigned result = 0;
  voyeur_context_t ctx = voyeur_context_create();
  voyeur_observe_exec(ctx, OBSERVE_EXEC_DEFAULT, exec_recursive_callback, (void*) &result);

  char* path   = "./test-exec-recursive";
  char* argv[] = { path, NULL };
  char* envp[] = { NULL };

  print_test_header("exec-recursive");
  voyeur_exec(ctx, path, argv, envp);
  print_test_footer(result == 8);

  voyeur_context_destroy(ctx);
}

void test_open()
{
  char result = 0;
  voyeur_context_t ctx = voyeur_context_create();
  voyeur_observe_open(ctx, OBSERVE_OPEN_CWD, open_callback, (void*) &result);

  char* path   = "./test-open";
  char* argv[] = { path, NULL };
  char* envp[] = { NULL };

  print_test_header("open");
  voyeur_exec(ctx, path, argv, envp);
  print_test_footer(result);

  voyeur_context_destroy(ctx);
}

void test_exec_and_open()
{
  char exec_result = 0, open_result = 0;
  voyeur_context_t ctx = voyeur_context_create();
  voyeur_observe_exec(ctx, OBSERVE_EXEC_DEFAULT, exec_callback, (void*) &exec_result);
  voyeur_observe_open(ctx, OBSERVE_OPEN_DEFAULT, open_callback, (void*) &open_result);

  char* path   = "./test-exec-and-open";
  char* argv[] = { path, NULL };
  char* envp[] = { NULL };

  print_test_header("exec and open");
  voyeur_exec(ctx, path, argv, envp);
  print_test_footer(exec_result && open_result);

  voyeur_context_destroy(ctx);
}

int main(int argc, char** argv)
{
  test_exec();
  test_exec_recursive();
  test_open();
  test_exec_and_open();
  return 0;
}
