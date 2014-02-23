LIBNAMES=libvoyeur libvoyeur-exec libvoyeur-open libvoyeur-close
TESTNAMES=test-exec test-exec-recursive test-open test-exec-and-open test-open-and-close test-exec-variants
TESTHARNESSNAME=voyeur-test
LIBNULLNAME=libnull
EXAMPLENAMES=voyeur-watch-exec voyeur-watch-open

CC=clang
CFLAGS=-I./include -g

UNAME := $(shell uname -s)
ifeq ($(UNAME), Darwin)
  LIBSUFFIX=dylib
else
  LIBSUFFIX=so
  CFLAGS+=-fPIC -pthread -lbsd
endif

HEADERS=$(wildcard src/*.h) $(wildcard include/*.h)
LIBSOURCES=$(wildcard src/*.c)
LIBOBJECTS=$(patsubst src/%.c, build/%.o, $(LIBSOURCES))
TESTSOURCES=$(wildcard test/*.c)
TESTOBJECTS=$(patsubst test/%.c, build/%.o, $(TESTSOURCES))
OBJECTS=$(LIBOBJECTS)
LIBS=$(addprefix build/, $(addsuffix .$(LIBSUFFIX), $(LIBNAMES)))
TESTS=$(addprefix build/, $(TESTNAMES))
TESTHARNESS=$(addprefix build/, $(TESTHARNESSNAME))
LIBNULL=$(addprefix build/, $(addsuffix .$(LIBSUFFIX), $(LIBNULLNAME)))
EXAMPLES=$(addprefix build/, $(EXAMPLENAMES))
BUILDDIR=$(realpath build/)

.PHONY: default check examples clean

default: build $(LIBS)

build:
	mkdir -p $@

$(OBJECTS): build/%.o : src/%.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

$(LIBS): build/lib%.$(LIBSUFFIX) : build/%.o build/net.o build/env.o build/event.o
ifeq ($(UNAME), Darwin)
	$(CC) $(CFLAGS) $^ -dynamiclib -install_name $(BUILDDIR)/lib$*.$(LIBSUFFIX) -o $@
else
	$(CC) $(CFLAGS) $^ -shared -Wl,-soname,lib$*.$(LIBSUFFIX) -o $@ -ldl
endif

check: default $(TESTHARNESS) $(TESTS) $(LIBNULL)
	cd build && ./$(TESTHARNESSNAME)

$(TESTHARNESS): build/% : test/%.c $(LIBS)
ifeq ($(UNAME), Darwin)
	$(CC) $(CFLAGS) -Lbuild -lvoyeur $< -o $@
else
	$(CC) $(CFLAGS) -Lbuild -lvoyeur -Wl,-rpath '-Wl,$$ORIGIN' $< -o $@
endif

$(TESTS): build/% : test/%.c $(LIBS)
	$(CC) $(CFLAGS) $< -o $@

$(LIBNULL): build/%.$(LIBSUFFIX) : test/%.c
ifeq ($(UNAME), Darwin)
	$(CC) $(CFLAGS) $^ -dynamiclib -install_name $(BUILDDIR)/$*.$(LIBSUFFIX) -o $@
else
	$(CC) $(CFLAGS) $^ -shared -Wl,-soname,$*.$(LIBSUFFIX) -o $@
endif

examples: default $(EXAMPLES)

$(EXAMPLES): build/% : examples/%.c $(HEADERS)
ifeq ($(UNAME), Darwin)
	$(CC) $(CFLAGS) -Lbuild -lvoyeur $< -o $@
else
	$(CC) $(CFLAGS) -Lbuild -lvoyeur -Wl,-rpath '-Wl,$$ORIGIN' $< -o $@
endif

clean:
	rm -rf build
