// platform/cpu/cpu_info_x64.cpp

#include "platform/cpu/cpu_info.h"

#include <cstdlib> // std::abort

#if defined(_MSC_VER)
    #include <intrin.h>
#else
    #include <immintrin.h> // for mingw.
    #include <cpuid.h>
#endif

#if !defined(__x86_64__) && !defined(_M_X64)
    #error cpu_info_x64.cpp compiled on non-x86-64 platform
#endif

namespace draconic::platform::cpu {

    namespace {
        // Checks if OS has enabled save/restore YMM states. Required for AVX.
        unsigned long long get_xcr0() noexcept {
            #if defined(_MSC_VER) || defined(__GNUC__) || defined(__clang__)
                return _xgetbv(0);
            #else
                return 0;
            #endif
        }

        // XCR0[1] is XMM, XCR0[2] is YMM.
        bool os_has_ymm() noexcept {
            return (get_xcr0() & 0x6ULL) == 0x6ULL;
        }

        // full ZMM state is required for AVX512.
        bool os_has_zmm() noexcept {
            return (get_xcr0() & 0xE6ULL) == 0xE6ULL;
        }

        void cpuid(unsigned int leaf, unsigned int subleaf, unsigned int& eax, unsigned int& ebx, unsigned int& ecx, unsigned int& edx) noexcept {
            #if defined(_MSC_VER)
                int regs[4];
                __cpuidex(regs, leaf, subleaf);
                eax = regs[0]; ebx = regs[1]; ecx = regs[2]; edx = regs[3];
            #else
                __cpuid_count(leaf, subleaf, eax, ebx, ecx, edx);
            #endif
        }

        CpuFeature detect_cpu_feature() noexcept {
            unsigned int eax = 0;
            unsigned int ebx = 0;
            unsigned int ecx = 0;
            unsigned int edx = 0;

            // leaf 1.
            cpuid(1, 0, eax, ebx, ecx, edx);

            constexpr unsigned int OSXSAVE = 1u << 27;
            constexpr unsigned int AVX = 1u << 28;
            constexpr unsigned int FMA = 1u << 12;

            if ((ecx & (OSXSAVE | AVX | FMA)) != (OSXSAVE | AVX | FMA)) {
                return CpuFeature::NONE;
            }

            if (!os_has_ymm()) {
                return CpuFeature::NONE;
            }

            // leaf 7
            cpuid(7, 0, eax, ebx, ecx, edx);

            constexpr unsigned int AVX2 = 1u << 5;
            constexpr unsigned int AVX512F = 1u << 16;

            if (!(ebx & AVX2)) {
                return CpuFeature::NONE;
            }

            if ((ebx & AVX512F) && os_has_zmm()) {
                return CpuFeature::AVX512F;
            }

            return CpuFeature::AVX2;
        }

    } // anonymous namespace.

    void validate_cpu() noexcept {
        #if defined(__x86_64__) || defined(_M_X64)
            const CpuFeature level = detect_cpu_feature();

            if(level == CpuFeature::NONE) {
                std::abort();
            }
        #else
            #error Unsupported architecture.
        #endif
    }
}
