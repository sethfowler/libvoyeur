CFLAGS=-I./include -g
OUTDIR=build
OUTPUTS=$(OUTDIR) $(OUTDIR)/libvoyeur.dylib $(OUTDIR)/libvoyeur-execve.dylib

.PHONY: default clean tests

default: $(OUTPUTS)

$(OUTDIR):
	mkdir -p $@

$(OUTDIR)/libvoyeur.dylib: src/voyeur.c src/net.c
	clang $(CFLAGS) $^ -dynamiclib -install_name 'libvoyeur.dylib' -o $@

$(OUTDIR)/libvoyeur-execve.dylib: src/voyeur-execve.c src/net.c
	clang $(CFLAGS) $^ -dynamiclib -flat_namespace -install_name 'libvoyeur-execve.dylib' -o $@

check: $(OUTPUTS) $(OUTDIR)/voyeur-test $(OUTDIR)/test-execve
	cd $(OUTDIR) && ./voyeur-test

$(OUTDIR)/voyeur-test: test/voyeur-test.c
	clang $(CFLAGS) -L$(OUTDIR) -lvoyeur $^ -o $@

$(OUTDIR)/test-execve: test/test-execve.c
	clang $(CFLAGS) $^ -o $@

clean:
	rm -rf $(OUTDIR)
