#ifndef LIBVOYEUR_VOYEUR_H
#define LIBVOYEUR_VOYEUR_H

#include <stdlib.h>

// Creating and destroying voyeur contexts.
typedef void* voyeur_context_t;
voyeur_context_t voyeur_context_create();
void voyeur_context_destroy(voyeur_context_t ctx);

// Registering interest in particular activities.
// You can unregister interest in an activity by passing a NULL callback.
typedef void (*voyeur_exec_callback)(const char* path,
                                     char* const argv[],
                                     char* const envp[],
                                     void* userdata);
typedef enum {
  OBSERVE_EXEC_DEFAULT = 0,
  OBSERVE_EXEC_CWD = 1 << 0,
  OBSERVE_EXEC_ENV = 1 << 1
} voyeur_exec_options;
// TODO: Encode options in ascii. | with '@'. Gives 5 bits.
// (Which means need to & each individual opts with 0x1F.)
void voyeur_observe_exec(voyeur_context_t ctx,
                         unsigned char opts,
                         voyeur_exec_callback callback,
                         void* userdata);

typedef void (*voyeur_open_callback)(const char* path,
                                     int oflag,
                                     mode_t mode,
                                     int retval,
                                     void* userdata);
typedef enum {
  OBSERVE_OPEN_DEFAULT = 0,
  OBSERVE_OPEN_CWD = 1 << 0
} voyeur_open_options;
void voyeur_observe_open(voyeur_context_t ctx,
                         unsigned char opts,
                         voyeur_open_callback callback,
                         void* userdata);

typedef void (*voyeur_close_callback)(int fd,
                                      int retval,
                                      void* userdata);
typedef enum {
  OBSERVE_CLOSE_DEFAULT = 0
} voyeur_close_options;
void voyeur_observe_close(voyeur_context_t ctx,
                          unsigned char opts,
                          voyeur_close_callback callback,
                          void* userdata);

// Create and observe a new process.
// TODO: This should just be a convenience function for C callers. We
// also need an API that will perform setup but allow the actual
// vfork/exec to be performed by external code. This will make
// bindings much simpler in the end, since we won't have to duplicate
// all of the ugly infrastructure needed to get portable execvpe and
// handle forking with green threads and all of that.
// However, to create that low-level API, I'll need to write the linux
// version of this function as well.
int voyeur_exec(voyeur_context_t ctx,
                const char* path,
                char* const argv[],
                char* const envp[]);

#endif
