#define ANKERL_NANOBENCH_IMPLEMENT
#include <nanobench.h>

#include <cstddef>
#include <cstdio>
#include <random>
#include <vector>

import core.math;

namespace {

// Build config output:
void print_metadata() {
#if defined(__clang__)
    std::printf("# compiler : clang %d.%d.%d\n", __clang_major__, __clang_minor__, __clang_patchlevel__);
#elif defined(__GNUC__)
    std::printf("# compiler : gcc %d.%d.%d\n", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#elif defined(_MSC_VER)
    std::printf("# compiler : msvc %d\n", _MSC_VER);
#endif

    std::printf("# sizeof(Float2)   = %zu, alignof = %zu\n", sizeof(draco::math::Float2), alignof(draco::math::Float2));
    std::printf("# sizeof(Float3)   = %zu, alignof = %zu\n", sizeof(draco::math::Float3), alignof(draco::math::Float3));
    std::printf("# sizeof(Float4)   = %zu, alignof = %zu\n", sizeof(draco::math::Float4), alignof(draco::math::Float4));
    std::printf("# sizeof(Float4x4) = %zu, alignof = %zu\n", sizeof(draco::math::Float4x4), alignof(draco::math::Float4x4));
}

void benchmark_vector_operations() {
    constexpr std::size_t COUNT = 1024;
    
    std::mt19937 rng(0xDEADBABE);
    std::uniform_real_distribution<draco::f32> dist(-1.0f, 1.0f);
    auto rnd = [&] { return dist(rng); };

    std::vector<draco::math::Float4> a(COUNT), b(COUNT);
    std::vector<draco::math::Float4x4> mats(COUNT);

    for (std::size_t i = 0; i < COUNT; ++i) {
        a[i] = { rnd(), rnd(), rnd(), rnd() };
        b[i] = { rnd(), rnd(), rnd(), rnd() };
        mats[i] = draco::math::Float4x4(
            draco::math::Float4{rnd(), rnd(), rnd(), rnd()},
            draco::math::Float4{rnd(), rnd(), rnd(), rnd()},
            draco::math::Float4{rnd(), rnd(), rnd(), rnd()},
            draco::math::Float4{rnd(), rnd(), rnd(), rnd()}
        );
    }

    const auto configure = [&](ankerl::nanobench::Bench& bench, const char* title) {
        bench
            .title(title)
            .relative(true)
            .epochs(20)
            .epochIterations(5000);
    };

    ankerl::nanobench::Bench throughputBench;
    configure(throughputBench, "Float4 dot throughput");
    throughputBench.run("packed dot", [&] {
        draco::f32 acc0 = 0.0f, acc1 = 0.0f, acc2 = 0.0f, acc3 = 0.0f;
        for (std::size_t i = 0; i < COUNT; i += 4) {
            acc0 += draco::math::dot(a[i + 0], b[i + 0]);
            acc1 += draco::math::dot(a[i + 1], b[i + 1]);
            acc2 += draco::math::dot(a[i + 2], b[i + 2]);
            acc3 += draco::math::dot(a[i + 3], b[i + 3]);
        }
        ankerl::nanobench::doNotOptimizeAway(acc0 + acc1 + acc2 + acc3);
    });

    ankerl::nanobench::Bench chainBench;
    configure(chainBench, "Float4 dot latency chain");
    chainBench.run("packed dependency chain", [&] {
        draco::math::Float4 v = a[0];
        draco::f32 s = 0.25f;
        for (std::size_t i = 0; i < COUNT; ++i) {
            v = { s, s, s, s };
            s = draco::math::dot(v, b[i]);
        }
        ankerl::nanobench::doNotOptimizeAway(s);
        ankerl::nanobench::doNotOptimizeAway(v);
    });

    // Matrix-Vector multiplication 
    ankerl::nanobench::Bench matBench;
    configure(matBench, "Float4x4 * Float4 performance");
    matBench.run("matrix * vector throughput", [&] {
        draco::math::Float4 total{0.0f};
        for (std::size_t i = 0; i < COUNT; ++i) {
            total += mats[i] * a[i];
        }
        ankerl::nanobench::doNotOptimizeAway(total);
    });
}

} // namespace

int main() {
    print_metadata();
    benchmark_vector_operations();
}
