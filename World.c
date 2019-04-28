#include "CL/Common.h"
#include "World.h"
#include "Atomic.h"
#include "Event.h"

typedef struct World_Request_s World_Request_t;
Atomic_DEFTYPE(World_Request)
struct World_Request_s {
    Atomic_World_Request_t next;
    clk_event_t ev;
};

struct World_s {
    Heap_t* outgoingEvents;
    // impl defined size
    Heap_t* heap;
    queue_t q;
};

void World_setTimeout(World_t* w, void (^block)(), uint ms) {
    clk_event_t ev = create_user_event();
    enqueue_kernel(w->q,
        CLK_ENQUEUE_FLAGS_WAIT_KERNEL,
        ndrange_1D(1),
        1, &ev,
        NULL,
        block);
    World_Request_t* req = Heap_dumbMalloc(w->heap, sizeof(World_Request_t));
    req->ev = ev;

    Event_t* hostEvent = Heap_dumbMalloc(w->outgoingEvents, sizeof(Event_t));
    hostEvent->type = Event_SET_TIMEOUT;
    hostEvent->setTimeout.ms = ms;
    hostEvent->handle = Heap_numForPtr(w->heap, req);
}

extern void cl_main(World_t*);

kernel void World_malloc(
    global Heap_t* heap,
    global uint* buf
) {
    int num = get_global_id(0);
    void* ptr = Heap_dumbMalloc(heap, buf[num]);
    buf[num] = Heap_numForPtr(heap, ptr);
}

kernel void World_init(
    global Heap_t* heap,
    global Heap_t* outgoingEvents,
    global const Heap_t* incomingEvents
) {
    atomic_init(&heap->offset, 0);
    atomic_init(&outgoingEvents->offset, 0);
    World_t* w = Heap_dumbMalloc(heap, sizeof(World_t));
    w->q = get_default_queue();
    w->outgoingEvents = outgoingEvents;
    w->heap = heap;

    cl_main(w);
}

kernel void World_events(
    global Heap_t* heap,
    global Heap_t* outgoingEvents,
    global Heap_t* incomingEvents
) {
    World_t* w = (World_t*) &heap->sixteens[0];
    Event_t* hostEvent = (Event_t*) &incomingEvents->sixteens[0];

    Event_t* hostEventsEnd = Heap_ptrForNum(incomingEvents, atomic_load(&incomingEvents->offset));

    while (hostEvent < hostEventsEnd) {
        World_Request_t* req = Heap_ptrForNum(heap, hostEvent->handle);
        set_user_event_status(req->ev, CL_COMPLETE);
        Heap_free(w->heap, req);
        hostEvent = &hostEvent[1];
    }
}
