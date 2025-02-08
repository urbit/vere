#ifndef VERE_ARENA_H
#define VERE_ARENA_H

#include <stddef.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

typedef struct {
    char* dat;
    char* beg;
    char* end;
} arena;

#define new(a, t, n)  (t*)arena_alloc(a, sizeof(t), _Alignof(t), n)

static void* arena_alloc(arena* a, ptrdiff_t size, ptrdiff_t align, ptrdiff_t count)
{
    ptrdiff_t padding = -(uintptr_t)a->beg & (align - 1);
    ptrdiff_t available = a->end - a->beg - padding;
    if (available < 0 || count > available/size) {
        abort();
    }
    void *p = a->beg + padding;
    a->beg += padding + count*size;
    return p;
    // return memset(p, 0, count*size);
}

static arena arena_create(ptrdiff_t cap)
{
    arena a = {0};
    a.dat = (char*)malloc(cap);
    a.beg = a.dat;
    a.end = a.beg ? a.beg+cap : 0;
    return a;
}


static void arena_free(arena* a)
{
  free(a->dat);
  a = (arena*)0;
}

#endif
