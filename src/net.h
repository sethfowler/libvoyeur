#ifndef VOYEUR_NET_H
#define VOYEUR_NET_H

#include <stddef.h>
#include <sys/un.h>
#include <unistd.h>

#include "event.h"

//////////////////////////////////////////////////
// Sock creation and connection.
//////////////////////////////////////////////////

// Creates a socket and starts listening on it. The caller must
// provided an uninitialized sockaddr_un struct. After
// create_server_socket returns, the socket path will be available
// in sockinfo->sun_path.
int voyeur_create_server_socket(struct sockaddr_un* sockinfo);

// Creates a socket and connects to the provided socket path on it.
int voyeur_create_client_socket(const char* sockpath);


//////////////////////////////////////////////////
// Event serialization.
//////////////////////////////////////////////////

// All libvoyeur events consist of an event type followed by a
// sequence of bytes, integers, and strings particular to the event.
//
// A typical sequence of calls for a writer:
//   voyeur_write_event_type(fd, VOYEUR_EVENT_XXX);
//   voyeur_write_string(fd, file, strlen(file));
//   voyeur_write_int(fd, flags);
//
// A matching sequence of calls for a reader:
//   voyeur_read_event_type(fd, &type);
//   /* dispatch to handler for VOYEUR_EVENT_XXX */
//   voyeur_read_string(fd, &file, &len);
//   voyeur_read_int(fd, &flags);
//
// Every read/write function returns 0 on success and -1 on error.

// Reader and writer for event types.
int voyeur_write_event_type(int fd, voyeur_event_type val);
int voyeur_read_event_type(int fd, voyeur_event_type* val);

// Reader and writer for bytes.
int voyeur_write_byte(int fd, char val);
int voyeur_read_byte(int fd, char* val);

// Reader and writer for integers.
int voyeur_write_int(int fd, int val);
int voyeur_read_int(int fd, int* val);

// Reader and writer for size_t.
int voyeur_write_size(int fd, size_t val);
int voyeur_read_size(int fd, size_t* val);

// Reader and writer for pid_t.
int voyeur_write_pid(int fd, pid_t val);
int voyeur_read_pid(int fd, pid_t* val);

#define VOYEUR_MAX_STRLEN 4096

// Write a string. If 'len' is 0, the length is determined by calling
// strnlen(val, VOYEUR_MAX_STRLEN).
int voyeur_write_string(int fd, const char* val, size_t len);

// Read a string. If 'maxlen' is 0, voyeur_read_string will allocate a
// buffer large enough to hold the string, which the caller is
// responsible for freeing. Otherwise, voyeur_read_string will use the
// buffer provided by the caller.
//
// Note that if you use a maximum length when reading, you must make
// sure to use the same limit when writing. If there isn't enough
// space in the buffer, voyeur_read_string will report an error.
int voyeur_read_string(int fd, char** val, size_t maxlen);

#endif
