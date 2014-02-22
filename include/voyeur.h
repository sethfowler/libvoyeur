#ifndef LIBVOYEUR_VOYEUR_H
#define LIBVOYEUR_VOYEUR_H

#include <stdint.h>
#include <stdlib.h>

//////////////////////////////////////////////////
// Creating and destroying libvoyeur contexts.
//////////////////////////////////////////////////

typedef void* voyeur_context_t;

// Creates a context used to specify which events you want to observe
// and as an owner for resources allocated by voyeur_prepare().
voyeur_context_t voyeur_context_create();

// Destroys a context. This must always be called after voyeur_start()
// is finished to release the resources allocated by voyeur_prepare().
// After destroying a context, it is invalid and should not be used again.
void voyeur_context_destroy(voyeur_context_t ctx);


//////////////////////////////////////////////////
// Registering callbacks for particular events.
//////////////////////////////////////////////////

// Observing exec*() calls.
typedef void (*voyeur_exec_callback)(const char* path,
                                     char* const argv[],
                                     char* const envp[],
                                     const char* cwd,
                                     void* userdata);
typedef enum {
  OBSERVE_EXEC_DEFAULT = 0,
  OBSERVE_EXEC_CWD = 1 << 0,
  OBSERVE_EXEC_ENV = 1 << 1
} voyeur_exec_options;
void voyeur_observe_exec(voyeur_context_t ctx,
                         uint8_t opts,
                         voyeur_exec_callback callback,
                         void* userdata);

// Observing open() calls.
typedef void (*voyeur_open_callback)(const char* path,
                                     int oflag,
                                     mode_t mode,
                                     const char* cwd,
                                     int retval,
                                     void* userdata);
typedef enum {
  OBSERVE_OPEN_DEFAULT = 0,
  OBSERVE_OPEN_CWD = 1 << 0
} voyeur_open_options;
void voyeur_observe_open(voyeur_context_t ctx,
                         uint8_t opts,
                         voyeur_open_callback callback,
                         void* userdata);

// Observing close() calls.
typedef void (*voyeur_close_callback)(int fd,
                                      int retval,
                                      void* userdata);
typedef enum {
  OBSERVE_CLOSE_DEFAULT = 0
} voyeur_close_options;
void voyeur_observe_close(voyeur_context_t ctx,
                          uint8_t opts,
                          voyeur_close_callback callback,
                          void* userdata);


//////////////////////////////////////////////////
// Observing processes.
//////////////////////////////////////////////////

// Prepare to observe a child process. This will acquire resources
// that libvoyeur needs to do its work and create a modified
// environment, based upon the provided template, that should be
// passed to the child process when it gets exec'd. It's the caller's
// responsibility to free this environment. This function should be
// called before forking. After calling voyeur_prepare(), it isn't
// safe to call // voyeur_observe_* functions on the same context anymore.
char** voyeur_prepare(voyeur_context_t ctx, char* const envp[]);

// Start observing a child process. This will block until the child
// process completes, at which time it will return the exit status of
// the child process. While the child process is running, the
// callbacks you've registered with the voyeur_observe_* functions
// will be called. Call this after forking, in the parent process.
int voyeur_start(voyeur_context_t ctx, pid_t child_pid);

// Create and observe a new child process. voyeur_exec() is a
// convenience function that behaves just as if you had called
// voyeur_prepare(), forked, called execve() to start your child
// process, and then called voyeur_start() from the parent process.
// Like voyeur_start(), it returns the exit status of the child.
int voyeur_exec(voyeur_context_t ctx,
                const char* path,
                char* const argv[],
                char* const envp[]);

#endif
