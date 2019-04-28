#include "CL/Clerror.h"
#include "CL/Common.h"
#include "CL/cl.h"

#include "Heap.h"
#include "World.h"
#include "Event.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>

#include <uv.h>


#define CHECK(desc, error) do { \
    if (err == CL_SUCCESS) { break; } \
    printf("Error %d %s -> %s\n", __LINE__, (desc), Clerror_str(error)); \
    abort(); \
} while (0)

#define CHECKED(expr) do { \
    int err; (expr); \
    CHECK(#expr, err); \
} while (0)


static char* getStringInfo(cl_device_id dev, int type) {
    size_t valueSize;
    CHECKED(err = clGetDeviceInfo(dev, type, 0, NULL, &valueSize));
    char* value = (char*) malloc(valueSize);
    CHECKED(err = clGetDeviceInfo(dev, type, valueSize, value, NULL));
    return value;
}

static void printLog(cl_program prog, cl_device_id* dev) {
    size_t len;
    char buffer[2048];
    //printf("Error: Failed to build program executable! %d\n", err);
    clGetProgramBuildInfo(prog, *dev, CL_PROGRAM_BUILD_LOG, sizeof(buffer), buffer, &len);
    printf("%s\n", buffer);
}

static cl_program compileAndLink(
    cl_context ctx,
    cl_device_id* dev,
    const char* options,
    const char* linkOptions,
    uint fileCount,
    char** fileNames
) {
    cl_program* programs = malloc(sizeof(cl_program*) * fileCount);
    assert(programs);

    char _buf[128];
    for (uint i = 0; i < fileCount; i++) {
        snprintf(_buf, 128, "#include \"%s\"", fileNames[i]);
        const char* buf = &_buf[0];
        CHECKED(programs[i] = clCreateProgramWithSource(ctx, 1, &buf, NULL, &err));
        int err = clCompileProgram(programs[i], 1, dev, options, 0, NULL, NULL, NULL, NULL);
        if (err != CL_SUCCESS) {
            printf("Failed to compile %s:\n", fileNames[i]);
            printLog(programs[i], dev);
            exit(1);
        }
    }

    int err = CL_SUCCESS;
    cl_program out = clLinkProgram(ctx, 1, dev, linkOptions, fileCount, programs, NULL, NULL, &err);
    if (err) {
        printf("Failed to link program:\n");
        printLog(out, dev);
        exit(1);
    }
    for (uint i = 0; i < fileCount; i++) {
        clReleaseProgram(programs[i]);
    }
    free(programs);
    return out;
}

typedef struct Context_s {
    cl_platform_id platform;
    cl_device_id dev;
    cl_context clCtx;
    cl_command_queue mainQ;
    cl_command_queue deviceQ;
    cl_program prog;

    cl_kernel eventKernel;
    cl_mem heap;
    cl_mem eventsToDev;
    cl_mem eventsFromDev;

    uv_loop_t uvLoop;
    Heap_t events;
} Context_t;

static void teardown(Context_t* ctx) {
    uv_loop_close(&ctx->uvLoop);

    clReleaseMemObject(ctx->heap);
    clReleaseMemObject(ctx->eventsToDev);
    clReleaseMemObject(ctx->eventsFromDev);
    clReleaseKernel(ctx->eventKernel);
    clReleaseProgram(ctx->prog);
    clReleaseCommandQueue(ctx->mainQ);
    clReleaseCommandQueue(ctx->deviceQ);
    clReleaseContext(ctx->clCtx);
    clReleaseDevice(ctx->dev);
    free(ctx);
}

static Context_t* setup() {
    Context_t* ctx = calloc(sizeof(Context_t), 1);
    assert(ctx);
    uv_loop_init(&ctx->uvLoop);

    CHECKED(err = clGetPlatformIDs(1, &ctx->platform, NULL));
    CHECKED(err = clGetDeviceIDs(ctx->platform, CL_DEVICE_TYPE_ALL, 1, &ctx->dev, NULL));
    CHECKED(ctx->clCtx = clCreateContext(0, 1, &ctx->dev, NULL, NULL, &err));
    CHECKED(ctx->mainQ = clCreateCommandQueueWithProperties(ctx->clCtx, ctx->dev, NULL, &err));
    cl_queue_properties qprop[] = {
        CL_QUEUE_SIZE, 128*1024,
        CL_QUEUE_PROPERTIES, (cl_command_queue_properties) (
            CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE |
            CL_QUEUE_ON_DEVICE |
            CL_QUEUE_ON_DEVICE_DEFAULT |
            CL_QUEUE_PROFILING_ENABLE
        ),
        0
    };
    CHECKED(ctx->deviceQ = clCreateCommandQueueWithProperties(ctx->clCtx, ctx->dev, qprop, &err));
    ctx->prog = compileAndLink(ctx->clCtx, &ctx->dev, "-cl-std=CL2.0", "", 2, (char*[]) {
        "main.c",
        "World.c"
    });
    cl_kernel World_init;

    CHECKED(ctx->heap = clCreateBuffer(ctx->clCtx, CL_MEM_READ_WRITE, sizeof(Heap_t), NULL, &err));
    CHECKED(ctx->eventsToDev = clCreateBuffer(ctx->clCtx, CL_MEM_READ_ONLY, sizeof(Heap_t), NULL, &err));
    CHECKED(ctx->eventsFromDev = clCreateBuffer(ctx->clCtx, CL_MEM_READ_WRITE, sizeof(Heap_t), NULL, &err));

    CHECKED(World_init = clCreateKernel(ctx->prog, "World_init", &err));
    CHECKED(err = clSetKernelArg(World_init, 0, sizeof(cl_mem), &ctx->heap));
    CHECKED(err = clSetKernelArg(World_init, 1, sizeof(cl_mem), &ctx->eventsFromDev));
    CHECKED(err = clSetKernelArg(World_init, 2, sizeof(cl_mem), &ctx->eventsToDev));

    CHECKED(ctx->eventKernel = clCreateKernel(ctx->prog, "World_events", &err));
    CHECKED(err = clSetKernelArg(ctx->eventKernel, 0, sizeof(cl_mem), &ctx->heap));
    CHECKED(err = clSetKernelArg(ctx->eventKernel, 1, sizeof(cl_mem), &ctx->eventsFromDev));
    CHECKED(err = clSetKernelArg(ctx->eventKernel, 2, sizeof(cl_mem), &ctx->eventsToDev));

    size_t globalWorkers = 1;
    CHECKED(err = clEnqueueNDRangeKernel(
        ctx->mainQ, World_init, 1, NULL, &globalWorkers, NULL, 0, NULL, NULL));

    clFinish(ctx->mainQ);
    clFinish(ctx->deviceQ);

    CHECKED(err = clEnqueueReadBuffer(
        ctx->mainQ, ctx->eventsFromDev, CL_TRUE, 0, sizeof(Heap_t), &ctx->events, 0, NULL, NULL));

    clReleaseKernel(World_init);

    return ctx;
}

static void sendEvents(Context_t* ctx) {
    //printf("sendEvents %d\n", ctx->events.offset);
    CHECKED(err = clEnqueueWriteBuffer(
        ctx->mainQ, ctx->eventsToDev, CL_TRUE, 0, sizeof(Heap_t), &ctx->events, 0, NULL, NULL));
    ctx->events.offset = 0;

    size_t globalWorkers = 1;
    CHECKED(err = clEnqueueNDRangeKernel(
        ctx->mainQ, ctx->eventKernel, 1, NULL, &globalWorkers, NULL, 0, NULL, NULL));

    clFinish(ctx->mainQ);
    clFinish(ctx->deviceQ);
}

typedef struct Timeout_s {
    uv_timer_t timer;
    Context_t* ctx;
    Event_t ev;
} Timeout_t;

void timeoutCb(uv_timer_t* handle) {
    //printf("timeoutCb\n");
    Timeout_t* t = (Timeout_t*) handle;
    Event_t* reply = Heap_dumbMalloc(&t->ctx->events, sizeof t->ev);
    memcpy(reply, &t->ev, sizeof t->ev);
    reply->type = Event_TIMEOUT;
    sendEvents(t->ctx);
    free(t);
}

static void createTimeout(Context_t* ctx, Event_t* ev) {
    //printf("setTimeout(%d)\n", ev->setTimeout.ms);
    Timeout_t* t = malloc(sizeof(Timeout_t));
    t->ctx = ctx;
    memcpy(&t->ev, ev, sizeof *ev);
    uv_timer_init(&ctx->uvLoop, &t->timer);
    uv_timer_start(&t->timer, timeoutCb, ev->setTimeout.ms, 0);
}

static void processEvents(Context_t* ctx) {
    //printf("processEvents %d\n", ctx->events.offset);
    char* overTop = Heap_ptrForNum(&ctx->events, ctx->events.offset);
    Event_t* ev = (Event_t*) &ctx->events.sixteens[0];
    while ((char*)ev < overTop) {
        switch (ev->type) {
            case Event_SET_TIMEOUT: createTimeout(ctx, ev); break;
            case Event_TIMEOUT: break;
            default: {
                printf("unexpected event %d\n", ev->type);
            }
        }
        ev = &ev[1];
    }
    ctx->events.offset = 0;
}

int main(int argc, char** argv)
{
    Context_t* ctx = setup();
    processEvents(ctx);
    uv_run(&ctx->uvLoop, 0);
    teardown(ctx);
}

uint __attribute__((overloadable)) atomic_fetch_add(volatile atomic_uint* x, uint y) {
    uint z = *x;
    *x += y;
    return z;
}
