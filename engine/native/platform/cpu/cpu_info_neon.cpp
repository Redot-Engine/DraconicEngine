// platform/cpu/cpu_info_neon.cpp

#include "platform/cpu/cpu_info.h"

#if !defined(__aarch64__)
    #error cpu_info_neon.cpp compiled on non-ARM64 platform
#endif

namespace draconic::platform::cpu {
    void validate_cpu() noexcept {
        // NEON is mandatory on AArch64.
        // So, if this compiles - It's valid.
    }
}
