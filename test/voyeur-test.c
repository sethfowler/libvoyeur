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

void exec_callback(const char* file,
                   char* const argv[],
                   char* const envp[],
                   const char* path,
                   const char* cwd,
                   pid_t pid,
                   pid_t ppid,
                   void* userdata)
{
  printf("[EXEC] %s", file);

  if (argv[0] != NULL) {
    for (int i = 1 ; argv[i] ; ++i)
      printf(" %s", argv[i]);
  }

  printf(" (in %s) (pid %u) (ppid %u)\n",
         cwd ? cwd : "?", (unsigned) pid, (unsigned) ppid);

  if (path) {
    printf("  path: %s\n", path);
  }

  if (envp) {
    printf("  environment:\n");
    for (int i = 0 ; envp[i] ; ++i)
      printf("    %s\n", envp[i]);
  }

  unsigned* result = (unsigned*) userdata;
  *result += 1;
}

void exit_callback(int status,
                   pid_t pid,
                   pid_t ppid,
                   void* userdata)
{
  printf("[EXIT] %d (pid %u) (ppid %u)\n", status, pid, ppid);

  char* result = (char*) userdata;
  *result += 1;
}

void open_callback(const char* path,
                   int oflag,
                   mode_t mode,
                   const char* cwd,
                   int retval,
                   pid_t pid,
                   void* userdata)
{
  printf("[OPEN] %s (flags %d) (mode %o) (in %s) (rv %d) (pid %u)\n",
         path, oflag, (unsigned) mode, cwd, retval, pid);

  char* result = (char*) userdata;
  *result = 1;
}

void close_callback(int fd, int retval, pid_t pid, void* userdata)
{
  printf("[CLOSE] %d (rv %d) (pid %u)\n", fd, retval, pid);

  char* result = (char*) userdata;
  *result += 1;
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
  voyeur_observe_exec(ctx, OBSERVE_EXEC_DEFAULT, exec_callback, (void*) &result);

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

void test_open_and_close()
{
  char open_result = 0, close_result = 0;
  voyeur_context_t ctx = voyeur_context_create();
  voyeur_observe_open(ctx, OBSERVE_OPEN_DEFAULT, open_callback, (void*) &open_result);
  voyeur_observe_close(ctx, OBSERVE_CLOSE_DEFAULT, close_callback, (void*) &close_result);

  char* path   = "./test-open-and-close";
  char* argv[] = { path, NULL };
  char* envp[] = { NULL };

  print_test_header("open and close");
  voyeur_exec(ctx, path, argv, envp);
  print_test_footer(open_result && close_result);

  voyeur_context_destroy(ctx);
}

void test_exec_variants()
{
  unsigned result = 0;
  voyeur_context_t ctx = voyeur_context_create();
  voyeur_observe_exec(ctx, OBSERVE_EXEC_DEFAULT, exec_callback, (void*) &result);

  char* path   = "./test-exec-variants";
  char* argv[] = { path, NULL };
  char* envp[] = { NULL };

  print_test_header("exec-variants");
  voyeur_exec(ctx, path, argv, envp);

  // The expected result is 11 even though there are only 10 variants
  // because 'system' spawns a 'sh' process to do the real work.
  print_test_footer(result == 11);

  voyeur_context_destroy(ctx);
}

void test_exit()
{
  unsigned result = 0;
  voyeur_context_t ctx = voyeur_context_create();
  voyeur_observe_exit(ctx, OBSERVE_EXIT_DEFAULT, exit_callback, (void*) &result);

  char* path   = "./test-exec-recursive";
  char* argv[] = { path, NULL };
  char* envp[] = { NULL };

  print_test_header("exit");
  voyeur_exec(ctx, path, argv, envp);
  // You'd expect 9, but /bin/echo doesn't call _exit...
  print_test_footer(result == 5);

  voyeur_context_destroy(ctx);
}

int main(int argc, char** argv)
{
  test_exec();
  test_exec_recursive();
  test_open();
  test_exec_and_open();
  //test_open_and_close();
  test_exec_variants();
  test_exit();
  return 0;
}
