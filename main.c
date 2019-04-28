
#include "CL/Common.h"
#include "World.h"
#include "Heap.h"

void cl_main(World_t* w) {

    World_setTimeout(w, ^{
        printf("Two Seconds\n");
    }, 2000);

    World_setTimeout(w, ^{
        printf("One Second\n");
    }, 1000);

    printf("Hello World!\n");
}
