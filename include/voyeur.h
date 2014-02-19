#ifndef LIBVOYEUR_VOYEUR_H
#define LIBVOYEUR_VOYEUR_H

// Creating and destroying voyeur contexts.
typedef void* voyeur_context_t;
voyeur_context_t voyeur_context_create();
void voyeur_context_destroy(voyeur_context_t ctx);

// Registering interest in particular activities.
// You can unregister interest in an activity by passing a NULL callback.
typedef void (*voyeur_exec_callback)(const char* path,
                                     char* const argv[],
                                     char* const envp[]);
void voyeur_add_exec_interest(voyeur_context_t ctx,
                              voyeur_exec_callback callback);

// Create and observe a new process.
// TODO: This should just be a convenience function for C callers. We
// also need an API that will perform setup but allow the actual
// vfork/exec to be performed by external code. This will make
// bindings much simpler in the end, since we won't have to duplicate
// all of the ugly infrastructure needed to get portable execvpe and
// handle forking with green threads and all of that.
// However, to create that low-level API, I'll need to write the linux
// version of this function as well.
int voyeur_observe(voyeur_context_t ctx,
                   const char* path,
                   char* const argv[],
                   char* const envp[]);

#endif
