libvoyeur
=========

A BSD-licensed library for observing the private behavior of a process.

Mac OS X and Linux are supported.

The following calls are observable:

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

The program doing the observing must link against `libvoyeur`. You'll need to
ensure that the various helper libraries like `libvoyeur-exec` are in the same
directory as `libvoyeur`.

The public API is documented in [voyeur.h](include/voyeur.h).

For more examples, the tests in [voyeur-test.c](test/voyeur-test.c) might be useful.
