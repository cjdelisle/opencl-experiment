#pragma once
#include "CL/Common.h"
#include "Heap.h"

#define Atomic_T(x) Atomic_ ## x ## _t
#define Atomic_DEFTYPE(__TYPE__) \
    typedef struct Atomic_ ## __TYPE__ ## _s { atomic_uint ptr; } Atomic_T(__TYPE__); \
    _Static_assert(sizeof(Atomic_T(__TYPE__)) == 4, ""); \
    static __TYPE__ ## _t * Atomic_ ## __TYPE__ ## _get (Heap_t* h, Atomic_T(__TYPE__)* p) { \
        return (__TYPE__ ## _t *) Heap_ptrForNum(h, atomic_load(&p->ptr)); \
    } \
    static void Atomic_ ## __TYPE__ ## _put (Heap_t* h, Atomic_T(__TYPE__)* p, __TYPE__ ## _t *val) { \
        atomic_store(&p->ptr, Heap_numForPtr(h, val)); \
    } \
    static void Atomic_ ## __TYPE__ ## _swap (Heap_t* h, \
        Atomic_T(__TYPE__)* atomic, \
        __TYPE__ ## _t *val, \
        Atomic_T(__TYPE__)* next \
    ) { \
        uint num = Heap_numForPtr(h, val); \
        uint expect; \
        do { \
            expect = atomic_load(&atomic->ptr); \
            atomic_store(&next->ptr, expect); \
        } while (!atomic_compare_exchange_weak(&atomic->ptr, &expect, num)); \
    }
