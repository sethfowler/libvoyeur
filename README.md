libvoyeur
=========

A BSD-licensed library for observing the private behavior of a process.

Currently libvoyeur is compatible with Mac OS X, but Linux compatibility is anticipated soon.

The following system calls will be observable in the near future:

- Process creation with `exec*`
- File access with `open` and `close`
- Socket access with `accept`, `bind`, `connect`, and `shutdown`

Currently only `exec*` is implemented.

There is a significant performance cost to the observed process, because the details of the system calls are sent to the observing process over IPC. However, libvoyeur tries to be as efficient as possible: you only pay a performance penalty for the system calls that you actually want to observe.

Installation
============

For now, `make` should do the job. Once Linux support arrives a `configure` script or the like will be added.

Usage
=====

Link your program against `libvoyeur`. The public API can be found in `include/voyeur.h`.
