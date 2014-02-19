#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "net.h"

int do_write(int fd, void* buf, size_t buf_size)
{
  ssize_t total_out = 0;
  while (total_out < (ssize_t) buf_size) {
    ssize_t out = write(fd,
                        buf + total_out,
                        buf_size - total_out);

    // Handle errors.
    if (out < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
        continue;
      } else {
        return -1;
      }
    }

    total_out += out;
  }

  return 0;
}

int do_read(int fd, void* buf, size_t buf_size)
{
  ssize_t total_in = 0;
  while (total_in < (ssize_t) buf_size) {
    ssize_t in = read(fd,
                      buf + total_in,
                      buf_size - total_in);

    // Handle errors.
    if (in < 0) {
      if (errno == EAGAIN || errno == EINTR) {
        continue;
      } else {
        return -1;
      }
    }

    // Handle EOF.
    if (in == 0) {
      return -1;
    }

    total_in += in;
  }

  return 0;
}
  
int voyeur_write_event_type(int fd, voyeur_event_type val)
{
  return do_write(fd, (void*) &val, sizeof(voyeur_event_type));
}

int voyeur_read_event_type(int fd, voyeur_event_type* val)
{
  return do_read(fd, (void*) val, sizeof(voyeur_event_type));
}

int voyeur_write_byte(int fd, char val)
{
  return do_write(fd, (void*) &val, sizeof(char));
}

int voyeur_read_byte(int fd, char* val)
{
  return do_read(fd, (void*) val, sizeof(char));
}

int voyeur_write_int(int fd, int val)
{
  return do_write(fd, (void*) &val, sizeof(int));
}

int voyeur_read_int(int fd, int* val)
{
  return do_read(fd, (void*) val, sizeof(int));
}

int voyeur_write_size(int fd, size_t val)
{
  return do_write(fd, (void*) &val, sizeof(size_t));
}

int voyeur_read_size(int fd, size_t* val)
{
  return do_read(fd, (void*) val, sizeof(size_t));
}

int voyeur_write_string(int fd, const char* val, size_t len)
{
  if (len == 0) {
    len = strnlen(val, VOYEUR_MAX_STRLEN);
  }

  if (voyeur_write_size(fd, len) < 0) {
    return -1;
  }
  
  return do_write(fd, (void*) val, len);
}

int voyeur_read_string(int fd, char** val, size_t maxlen)
{
  size_t len;
  if (voyeur_read_size(fd, &len) < 0) {
    return -1;
  }

  if (maxlen == 0) {
    *val = malloc(len * (sizeof(char) + 1));
  } else if (maxlen <= len) {
    fprintf(stderr,
            "libvoyeur: string buffer of size %zu is too small for string of length %zu\n",
            maxlen,
            len);
    exit(EXIT_FAILURE);
  }
  
  if (do_read(fd, (void*) *val, len) < 0) {
    if (maxlen == 0) {
      free(*val);
    }
    return -1;
  }

  (*val)[len] = '\0';
  return 0;
}
