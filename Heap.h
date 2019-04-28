#pragma once
#include "CL/Common.h"
#include "Util.h"

typedef struct Heap_s {
    atomic_uint offset;
    uint _pad0;
    uint2 _pad1;
    uint4 sixteens[1023];
} Heap_t;
_Static_assert(sizeof(uint) == 4, "");
_Static_assert(sizeof(atomic_uint) == 4, "");
_Static_assert(sizeof(uint2) == 8, "");
_Static_assert(sizeof(uint4) == 16, "");
_Static_assert(sizeof(Heap_t) == 1024*16, "");

static void* Heap_ptrForNum(const Heap_t* h, uint num) {
    if (!num) { return NULL; }
    return (void*) (((uintptr_t)num) + ((uintptr_t)&h->sixteens));
}

static uint Heap_numForPtr(const Heap_t* h, void* ptr) {
    if (!ptr) { return 0; }
    return (uint) (((uintptr_t)ptr) - ((uintptr_t)&h->sixteens));
}

static void* Heap_dumbMalloc(Heap_t* h, size_t size) {
    uint os = atomic_fetch_add(&h->offset, Util_roundup16(size));
    return (void*) &h->sixteens[os / 16];
}

static void Heap_free(Heap_t* h, void* whatever) { }
