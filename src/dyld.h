#ifndef VOYEUR_DYLD_H
#define VOYEUR_DYLD_H

#include <dlfcn.h>

#ifdef __APPLE__

#define VOYEUR_FUNC(_foo) voyeur_##_foo
#define VOYEUR_DECLARE_NEXT(_foo_t, _foo)
#define VOYEUR_STATIC_DECLARE_NEXT(_foo_t, _foo)
#define VOYEUR_LOOKUP_NEXT(_foo_t, _foo)
#define VOYEUR_CALL_NEXT(_foo, ...) _foo(__VA_ARGS__)

// Based on DYLD_INTERPOSE in dyld-interposing.h:
#define VOYEUR_INTERPOSE(_replacee) \
  __attribute__((used)) static struct{ const void* replacment; const void* replacee; } _interpose_##_replacee \
  __attribute__ ((section ("__DATA,__interpose"))) = { (const void*)(unsigned long)&VOYEUR_FUNC(_replacee), (const void*)(unsigned long)&_replacee }; 

#else

#define VOYEUR_FUNC(_foo) _foo
#define VOYEUR_DECLARE_NEXT(_foo_t, _foo) _foo_t voyeur_##_foo##_next = NULL;
#define VOYEUR_STATIC_DECLARE_NEXT(_foo_t, _foo) static _foo_t voyeur_##_foo##_next = NULL;
#define VOYEUR_LOOKUP_NEXT(_foo_t, _foo) voyeur_##_foo##_next = (_foo_t) dlsym(RTLD_NEXT, #_foo)
#define VOYEUR_CALL_NEXT(_foo, ...) voyeur_##_foo##_next(__VA_ARGS__)
#define VOYEUR_INTERPOSE(_replacee)

#endif

#endif
