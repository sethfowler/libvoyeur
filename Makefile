CFLAGS=-I./include -g
OUTDIR=build
OUTPUTS=$(OUTDIR) $(OUTDIR)/libvoyeur.dylib $(OUTDIR)/libvoyeur-exec.dylib $(OUTDIR)/libvoyeur-open.dylib
TESTS=$(OUTDIR)/voyeur-test $(OUTDIR)/test-exec $(OUTDIR)/test-exec-recursive $(OUTDIR)/test-open $(OUTDIR)/test-exec-and-open $(OUTDIR)/libnull.dylib

.PHONY: default clean tests

default: $(OUTPUTS)

$(OUTDIR):
	mkdir -p $@

$(OUTDIR)/libvoyeur.dylib: src/voyeur.c src/net.c src/env.c
	clang $(CFLAGS) $^ -dynamiclib -install_name 'libvoyeur.dylib' -o $@

$(OUTDIR)/libvoyeur-exec.dylib: src/voyeur-exec.c src/net.c src/env.c
	clang $(CFLAGS) $^ -dynamiclib -flat_namespace -install_name 'libvoyeur-exec.dylib' -o $@

$(OUTDIR)/libvoyeur-open.dylib: src/voyeur-open.c src/net.c src/env.c
	clang $(CFLAGS) $^ -dynamiclib -flat_namespace -install_name 'libvoyeur-open.dylib' -o $@

check: $(OUTPUTS) $(TESTS)
	cd $(OUTDIR) && ./voyeur-test

$(OUTDIR)/voyeur-test: test/voyeur-test.c
	clang $(CFLAGS) -L$(OUTDIR) -lvoyeur $^ -o $@

$(OUTDIR)/test-exec: test/test-exec.c
	clang $(CFLAGS) $^ -o $@

$(OUTDIR)/test-exec-recursive: test/test-exec-recursive.c
	clang $(CFLAGS) $^ -o $@

$(OUTDIR)/test-open: test/test-open.c
	clang $(CFLAGS) $^ -o $@

$(OUTDIR)/test-exec-and-open: test/test-exec-and-open.c
	clang $(CFLAGS) $^ -o $@

$(OUTDIR)/libnull.dylib: test/libnull.c
	clang $(CFLAGS) $^ -dynamiclib -install_name 'libnull.dylib' -o $@

clean:
	rm -rf $(OUTDIR)
