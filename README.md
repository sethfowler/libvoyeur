libvoyeur
=========

A BSD-licensed library for observing the private behavior of a child process.

Mac OS X and Linux are supported.

The following calls are currently observable:

- Process creation with `exec*`
- File access with `open` and `close`

There is a significant performance cost to the observed process, because the
details of the calls are sent to the observing process over IPC, and locking is
required. However, libvoyeur tries to be as efficient as possible: you only pay
a performance penalty for the calls that you actually want to observe.

Compilation
===========

Just run `make`. You can check that everything built correctly with `make check`.

Usage
=====

The program doing the observing must link against `libvoyeur`. If the various
the various helper libraries like `libvoyeur-exec` are not in the same directory
as `libvoyeur`, you'll need to tell the library where to find them using
`voyeur_set_resource_path`.

The public API is documented in [voyeur.h](include/voyeur.h).

The [examples directory](examples/) contains some sample programs built using
`libvoyeur`. You can build the examples with `make examples`.
