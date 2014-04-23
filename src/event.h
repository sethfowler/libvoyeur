#ifndef VOYEUR_EVENTS_H
#define VOYEUR_EVENTS_H

// How to define a new event:
// 1. Add the new event name to MAP_EVENTS.
// 2. Define the public API in voyeur.h. (To keep things readable, avoid using
//    MAP_EVENTS there.)
// 3. Implement voyeur-xxx.c based on one of the existing events, and
//    add it to LIBNAMES in the Makefile.
// 4. Implement a handle_xxx function in event.c.
// 5. Add a test!

#define MAP_EVENTS                              \
  ON_EVENT(EXEC, exec)                          \
  ON_EVENT(EXIT, exit)                          \
  ON_EVENT(OPEN, open)                          \
  ON_EVENT(CLOSE, close)                        \

// Define an enumeration for event types.
#define ON_EVENT(E, _) VOYEUR_EVENT_##E,
typedef enum {
  MAP_EVENTS
  VOYEUR_EVENT_MAX
} voyeur_event_type;
#undef ON_EVENT

// Define the voyeur_context type, which is primarily a container for
// event options and callbacks.
#define ON_EVENT(_, e)                          \
  uint8_t e##_opts;                             \
  void* e##_cb;                                 \
  void* e##_userdata;

typedef struct {
  MAP_EVENTS

  char* resource_path;
  void* server_state;
} voyeur_context;

#undef ON_EVENT

// Dispatch to the correct handler for the given event type.
void voyeur_handle_event(voyeur_context* context, voyeur_event_type type, int fd);

// Create the VOYEUR_LIBS and VOYEUR_OPTS strings based on the
// context. The caller is responsible for freeing them.
char* voyeur_requested_libs(voyeur_context* context);
char* voyeur_requested_opts(voyeur_context* context);

#endif
