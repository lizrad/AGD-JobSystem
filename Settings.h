#pragma once      
#include <cstdio>


// Use this to switch betweeen serial and parallel processing (for perf. comparison)
constexpr bool isRunningParallel = true;
//#define VERBOSE
#define ESSENTIAL
//#define RUN_ONCE
// Used for automatically record session and saving it
#define CAPTURE_OPTICK
//#define MEASURING_AVERAGE_TIME


#ifdef VERBOSE
#define PRINT(x) printf(x)
#else
#define PRINT(x)
#endif

#ifdef VERBOSE
#define PRINTW(workerId, x) printf(("WORKER #" + std::to_string(workerId) + ":" + x + "\n").c_str())
#else
#define PRINTW(workerId, x)
#endif

#ifdef ESSENTIAL
#define PRINT_ESSENTIAL(x) printf(x)
#else
#define PRINT_ESSENTIAL(x)
#endif



