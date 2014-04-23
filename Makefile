###############################################################################
# Customizable values
###############################################################################

CC?=clang
CFLAGS?=-I./include
PREFIX?=/usr/local/bin

ifdef DEBUG
CFLAGS+=-g -DDEBUG
endif


###############################################################################
# Outputs
###############################################################################

MAINLIBNAME=libvoyeur
LIBNAMES=libvoyeur-recurse libvoyeur-exec libvoyeur-exit libvoyeur-open libvoyeur-close
TESTNAMES=test-exec test-exec-recursive test-open test-exec-and-open test-open-and-close test-exec-variants
TESTHARNESSNAME=voyeur-test
LIBNULLNAME=libnull
EXAMPLENAMES=voyeur-watch-exec voyeur-watch-open


###############################################################################
# Computed values
###############################################################################

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
MAINLIB=$(addprefix build/, $(addsuffix .$(LIBSUFFIX), $(MAINLIBNAME)))
MAINSTATICLIB=$(addprefix build/, $(addsuffix .a, $(MAINLIBNAME)))
TESTS=$(addprefix build/, $(TESTNAMES))
TESTHARNESS=$(addprefix build/, $(TESTHARNESSNAME))
LIBNULL=$(addprefix build/, $(addsuffix .$(LIBSUFFIX), $(LIBNULLNAME)))
EXAMPLES=$(addprefix build/, $(EXAMPLENAMES))
BUILDDIR=$(realpath build/)

ifeq ($(UNAME), Darwin)
  define make-dynamic-lib
  $(CC) $(CFLAGS) $^ -dynamiclib -install_name lib$*.$(LIBSUFFIX) -o $@
  endef

  define make-exec
  $(CC) $(CFLAGS) -Lbuild -lvoyeur $< -o $@
  endef
else
  define make-dynamic-lib
  $(CC) $(CFLAGS) $^ -shared -Wl,-soname,lib$*.$(LIBSUFFIX) -o $@ -ldl
  endef

  define make-exec
  $(CC) $(CFLAGS) -Lbuild -lvoyeur -Wl,-rpath '-Wl,$$ORIGIN' $< -o $@
  endef
endif

.PHONY: default check examples install clean


###############################################################################
# Library targets
###############################################################################

default: build $(LIBS) $(MAINLIB) $(MAINSTATICLIB)

build:
	mkdir -p $@

$(OBJECTS): build/%.o : src/%.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

$(LIBS): build/lib%.$(LIBSUFFIX) : build/%.o build/net.o build/env.o build/event.o build/util.o
	$(make-dynamic-lib)

$(MAINLIB): build/lib%.$(LIBSUFFIX) : build/%.o build/net.o build/env.o build/event.o build/util.o
	$(make-dynamic-lib)

$(MAINSTATICLIB): build/lib%.a : build/%.o build/net.o build/env.o build/event.o build/util.o
	$(AR) rcs $@ $^


###############################################################################
# Test targets
###############################################################################

check: default $(TESTHARNESS) $(TESTS) $(LIBNULL)
	cd build && ./$(TESTHARNESSNAME)

$(TESTHARNESS): build/% : test/%.c $(LIBS)
	$(make-exec)

$(TESTS): build/% : test/%.c $(LIBS)
	$(CC) $(CFLAGS) $< -o $@

$(LIBNULL): build/lib%.$(LIBSUFFIX) : test/%.c
	$(make-dynamic-lib)


###############################################################################
# Example targets
###############################################################################

examples: default $(EXAMPLES)

$(EXAMPLES): build/% : examples/%.c $(HEADERS)
	$(make-exec)


###############################################################################
# Install targets
###############################################################################

install: default
	install -c $(MAINLIB) $(MAINSTATICLIB) $(LIBS) $(PREFIX)

uninstall:
	rm $(addprefix $(PREFIX)/, $(notdir $(MAINLIB)))
	rm $(addprefix $(PREFIX)/, $(notdir $(MAINSTATICLIB)))
	rm $(addprefix $(PREFIX)/, $(notdir $(LIBS)))

###############################################################################
# Utility targets
###############################################################################

clean:
	rm -rf build
