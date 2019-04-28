#pragma once
#include "CL/Common.h"
#include "Atomic.h"

enum Event_Type {
    Event_SET_TIMEOUT = 33,
    Event_TIMEOUT = 34,
};

typedef struct Event_SetTimeout_s {
    uint ms;
} Event_SetTimeout_t;
_Static_assert(sizeof(Event_SetTimeout_t) == 4, "");

typedef struct Event_s {
    enum Event_Type type;
    uint handle;
    union {
        Event_SetTimeout_t setTimeout;
    };
    uint _pad;
} Event_t;
_Static_assert(sizeof(Event_t) == 16, "");
