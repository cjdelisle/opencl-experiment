# OpenCL Experiment

Status: experiment/unmaintained

This is an experiment to see if asynchronous programming can be implemented in OpenCL.

The idea is that entire programs could be written on-device with only generic support code
running on the host side.

## Why

The tragedy of NodeJS is that it's a world of high performance design patterns implemented in a
single-thread interpreted language. The idea of this experiment is to see if a version of the
async design patterns from NodeJS can be implemented in OpenCL.

A Vega64 GPU has 64 entirely autonamous "cores", each core has 4 "sub-cores" which are able to
carry out independent execution but share some hardware such as the branch handler. Each sub-core
has 16 SIMD lanes so it is capable of performing one operation (add, subtract, etc) on 16 pieces
of data simultaneously. So if you're doing completely different things with nothing in common, a
Vega64 looks something between a 64 core and a 256 core processor, but if you can find commonality
between the things you're doing, a Vegs64 can perform like a 4096 core monster.

Of course these cores aren't something you can just install linux on and be done. Today's
processors are designed for *low latency* execution, so they are best for programs where every
instruction depends on the output from the last. These processors have significant on-die caches
to reduce the amount of time spent waiting for data, they also have advanced branch predictor
hardware in order to speculatively execute code before a decision has been reached about what
code should run. This is why "general purpose" processors remain 16 or 32 core affairs rather than
256 or 4096 core.

In GPUs, the program to be executed has traditionally been
[shaders](https://en.wikipedia.org/wiki/Shader), programs with minimal branching and memory access
which run for every pixel on the screen. In this scenario it's clear that SIMD lanes are a win
if they can reduce the amount of silicon per core. As video game demands became more complex,
demands for branching and memory access have begun to increase, but rather than go the
"general purpose processor" route of increasing expensive cache and branch prediction logic, GPU
designers invested in
[simultaneous multithreading](https://en.wikipedia.org/wiki/Simultaneous_multithreading) research
which allowed them to keep as many as 10 differnet programs ready to execute on the same core,
so that when one program needs to branch or access memory, there's always another one to use the
GPU resources.

## What

This experiment demonstrates a very simple opencl program running and using
[libuv](https://libuv.org/) to manage access to the operating system. Currently it only supports
one function `World_setTimeout()` which demonstrates that a task can be scheduled from inside of
an OpenCL function and then it can execute later when it's awoken by libuv.

This is the example code:

```c
void cl_main(World_t* w) {

    World_setTimeout(w, ^{
        printf("Two Seconds\n");
    }, 2000);

    World_setTimeout(w, ^{
        printf("One Second\n");
    }, 1000);

    printf("Hello World!\n");
}
```

## How to test it

First, you need a system with OpenCL **2.0 or newer**, this makes heavy use of
[device-side enqueue](https://www.khronos.org/registry/OpenCL/sdk/2.0/docs/man/xhtml/enqueue_kernel.html)
which was not present in OpenCL 1.1.

One place where you can get this is by installing the
[opencl2.0-intel-cpu](https://github.com/cwpearson/opencl2.0-intel-cpu/) docker image, which will
allow you to run OpenCL 2.0 on your CPU.

Once you have a shell inside of this docker image, you will need to make sure you have a recent
version of [clang](http://clang.llvm.org/). This example cannot be compiled with gcc because the
host-side code makes use of clang-specific extensions which exist to support OpenCL.

You will also need libuv.

### Installing the stuff

```
apt install libuv1-dev
apt install clang-6.0
```

### Compiling the source

```
clang-6.0 -fblocks -o ./init ./CL/init.c -I. -Wl,/usr/lib/x86_64-linux-gnu/libOpenCL.so.1.0.0 -luv
```

### Running it

```
./init
```

## IDE-support and other errata

The files `main.c` and `World.c` as well as most of the header files are actually OpenCL code,
despite being named `.c`. The reason for this is to trick Visual Studio Code into doing
auto-completion and error checking as if the files was C. In order to get good auto-completion and
error checking, you can use
[clangd](https://clang.llvm.org/extra/clangd/Installation.html#editor-plugins)
with a plugin for your favorite editor. The file `CL/opencl-c.h` is borrowed from the LLVM project
and patched to work as a shim for IDE syntax highlighting and auto-complete.

### Test-compiling OpenCL

Since the error messages from OpenCL drivers can be difficult or cryptic, you can test-compile
OpenCL code using clang, as follows:

```
clang-6.0 -I. -x cl -cl-std=CL2.0 -Xclang -finclude-default-header -c -o /dev/null ./main.c
```

## Things necessary to make this useful

* [] Self-contained OpenCL (compiles to a binary which runs on computers without OpenCL installed)
  * Ideal case is to have a "fat binary" which runs as this one does if it detects an appropriate
  OpenCL implementation, but otherwise falls back to running in host side code.
  * Probably best to use clang to compile the kernels and the pocl kernel support library to
  support them and the pocl pthread runtime to manage them.
  * Fully using pocl has 3 problems:
    1. It requires bundling a large amount of code, including LLVM, which is not appropriate for
    a small project.
    2. pocl lacks [device-side enqueue](https://github.com/pocl/pocl/issues/715)
    3. pocl strips debugging info during optimization passes
  * To achieve partial usage of pocl, we need:
    * [] Extract the pthread runtime into it's own project
    * [] Implement `enqueue_kernel` and `create_user_event`
    * [] Migrate init.c to use this backend when none other is available
* Memory
  * [] Full malloc implementation
  * [] [Allocator.c](https://github.com/cjdelisle/cjdns/blob/master/memory/Allocator.c)
  * [] Garbage collector (?)
* Events
  * [] Events carrying arbitrary payload
  * [] Multiple events per cycle
  * [] Cancelling events (e.g. `clearTimeout()`)
* [] Define a concurrency story
  * `withLock(lock, ^{ inner code... })` ?
  * Re-create objects and switch atomics pointers ?
* [] Strings
  * [] implement basic functions: slice, substr, concat w/ shared memory
  * [] port grep/egrep code to opencl
  * [] implement grep-based indexOf, lastIndexOf, equals
  * [] implement egrep-based regex test, replace
* [] Lists
  * [] automatic re-allocation
  * [] push/pop/shift/unshift
  * [] indexOf
* [] Maps
  * [] dynamic re-allocation
* System APIs
  * [] UDP
    * [] sendto
    * [] bind
    * [] ondata
  * [] TCP
    * [] listen
    * [] accept
    * [] ondata
  * [] getaddrinfo
  * [] signal
  * [] fs
  * [] gettimeofday
* [] HTTP parser
* [] Demo HTTP server
* [] Package manager
