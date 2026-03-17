// platform/simd.h

#pragma once

// Compiler detection.
#if defined(__clang__)
    #define USING_COMPILER_CLANG 1
#elif defined(_MSC_VER)
    #define USING_COMPILER_MSVC 1
#elif defined(__GNUC__)
    #define USING_COMPILER_GCC 1
#else
    #error Unsupported compiler
#endif

// Architecture detection
#if defined(__x86_64__) || defined(_M_X64)
    #define ARCH_X64 1
#elif defined(__aarch64__)
    #define ARCH_ARM64 1
#else
    #error Unsupported architecture
#endif

// SIMD level
#if ARCH_X64
    #define SIMD_AVX2 1

    // We MAY remove this and have it auto-detect later.
    #if defined(ENABLE_AVX512)
        #define SIMD_AVX512 1
    #endif

#elif ARCH_ARM64
    #define SIMD_NEON 1
#endif

// Force inline.
#if USING_COMPILER_MSVC
    #define FORCEINLINE __forceinline
#else
    #define FORCEINLINE inline __attribute__((always_inline))
#endif

// Restrict
#if USING_COMPILER_MSVC
    #define RESTRICT __restrict
#else
    #define RESTRICT __restrict__
#endif

// Alignment helpers.
#if USING_COMPILER_MSVC
    #define ALIGN(N) __declspec(align(N))
#else
    #define ALIGN(N) __attribute__((aligned(N)))
#endif

// assume and unreachable.
#if USING_COMPILER_MSVC
    // TODO: CMakeLists needs to support some kind of debug or something here.
    #if DEBUG
        #define ASSUME(x) do { if (!(x)) __debugbreak(); } while (0)
        #define UNREACHABLE() __debugbreak()
    #else
        #define ASSUME(x) __assume(x)
        #define UNREACHABLE() __assume(false)
    #endif
#else
    #if DEBUG
        #define ASSUME(x) do { if (!(x)) __builtin_trap(); } while (0)
        #define UNREACHABLE() __builtin_trap()
    #else
        #define ASSUME(x) do { if (!(x)) __builtin_unreachable(); } while (0)
        #define UNREACHABLE() __builtin_unreachable()
    #endif
#endif
