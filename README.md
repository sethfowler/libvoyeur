libvoyeur
=========

A BSD-licensed library for observing the private behavior of a process.

Mac OS X and Linux are supported.

The following calls are observable:

- Process creation with `exec*`
- File access with `open` and `close`
- Socket access with `accept`, `bind`, `connect`, and `shutdown`

There is a significant performance cost to the observed process, because the
details of the calls are sent to the observing process over IPC, and locking is
required. However, libvoyeur tries to be as efficient as possible: you only pay
a performance penalty for the calls that you actually want to observe.

Installation
============

Just run `make`. You can check that everything built correctly with `make check`.

Usage
=====

The program doing the observing must link against `libvoyeur`. The public API
can be found in `include/voyeur.h`.
