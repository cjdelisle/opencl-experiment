#pragma once
#include "CL/Common.h"

typedef struct World_s World_t;

void World_setTimeout(World_t* w, void (^block)(), uint ms);
