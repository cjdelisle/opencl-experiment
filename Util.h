#pragma once
#include "CL/Common.h"

static size_t Util_roundup16(size_t size) { return (size + 15) & (~15); }
