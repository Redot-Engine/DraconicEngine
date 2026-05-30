// rendering/rhi/macros.h

#pragma once
#include <print>
#include <cstdlib>

#ifndef DRACO_RHI_VALIDATION
	#define DRACO_RHI_VALIDATION 1
#endif

#if DRACO_RHI_VALIDATION
	#define RHI_ASSERT(cond, msg, ...) \
        do { \
            if (!(cond)) { \
                std::println("[RHI ERROR] " msg, ##__VA_ARGS__); \
                std::abort(); \
            } \
        } while(0)

	#define RHI_WARN(cond, msg, ...) \
        do { \
            if (!(cond)) { \
                std::println("[RHI WARNING] " msg, ##__VA_ARGS__); \
            } \
        } while(0)
#else
	#define RHI_ASSERT(cond, msg, ...) do { (void)(cond); } while(0)
	#define RHI_WARN(cond, msg, ...)   do { (void)(cond); } while(0)
#endif
