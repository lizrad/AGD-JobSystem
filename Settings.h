#pragma once
#include <cstdio>

#define VERBOSE

#ifdef VERBOSE
#define PRINT(x) printf(x)
#else
#define PRINT(x)
#endif


#define RUN_ONCE

#define CAPTURE_OPTICK

// Use this to switch betweeen serial and parallel processing (for perf. comparison)
constexpr bool isRunningParallel = true;