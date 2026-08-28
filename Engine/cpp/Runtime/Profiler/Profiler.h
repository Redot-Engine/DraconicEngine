// Profiler instrumentation macros.
//
// Include this header (it's just macros) AND `import profiler` in a TU that instruments.
// DRACO_PROFILE_SCOPE("Name") profiles the enclosing block; the frame macros bracket a frame.
// When DRACO_PROFILING is off (shipping builds), every macro compiles to nothing.
#pragma once

#ifndef DRACO_PROFILING
#define DRACO_PROFILING 0
#endif

#define DRACO_PROFILE_CONCAT_(a, b) a##b
#define DRACO_PROFILE_CONCAT(a, b)  DRACO_PROFILE_CONCAT_(a, b)

#if DRACO_PROFILING

#define DRACO_PROFILE_SCOPE(name) \
    ::draco::profiler::ScopedProfile DRACO_PROFILE_CONCAT(dracoProfScope_, __LINE__){ (name) }
#define DRACO_PROFILE_FRAME_BEGIN() ::draco::profiler::Profiler::get().beginFrame()
#define DRACO_PROFILE_FRAME_END()   ::draco::profiler::Profiler::get().endFrame()

#else

#define DRACO_PROFILE_SCOPE(name)   ((void)0)
#define DRACO_PROFILE_FRAME_BEGIN() ((void)0)
#define DRACO_PROFILE_FRAME_END()   ((void)0)

#endif
