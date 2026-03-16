// platform/cpu/cpu_info.h

#pragma once
namespace draconic::platform::cpu {
    enum class CpuFeature : unsigned char {
        NONE,
        AVX2,
        AVX512F,
        NEON,
    };

    void validate_cpu() noexcept;
}
