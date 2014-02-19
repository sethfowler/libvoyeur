#ifndef VOYEUR_ENV_H
#define VOYEUR_ENV_H


// Augments the provided list of environment variables with the
// variables required for libvoyeur to observe a process. It is
// assumed that the caller is calling this function between calling
// fork() and calling execve(), so the ownership of the allocated
// memory is assumed to be irrelevant.
char** augment_environment(char* const* envp, const char* sockpath);

#endif
