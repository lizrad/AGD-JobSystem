#pragma once      
#include <cstdio>


// Use this to switch betweeen serial and parallel processing (for perf. comparison)
constexpr bool isRunningParallel = true;

//Controls how many frames are run at the same time. Useful for stress testing.
#define SIMULATENOUS_FRAME_COUNT 1

//Controls how many particle jobs are spawned for each frame. Useful for stress testing.
#define PARTICLE_JOB_COUNT 1

//Controls wether verbose information should be printed.
//#define VERBOSE

//Controls wether essential information should be printed. Essential inforamtion is mostly errors and measurements.
#define ESSENTIAL

//Controls wether only one frame should run or if the application runs frame until a user quit happens.
//#define RUN_ONCE

// Used to automatically record and save session
#define CAPTURE_OPTICK

//Controls wether average frame duration should be measured before the normal behaviour starts.
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



