#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

import core.math;

TEST_CASE("pow") {
    double result = draco::math::pow(2., .5);
    constexpr double expected = draco::math::SQRT2;
    REQUIRE(result == expected);
}

TEST_CASE("abs") {
    using draco::math::abs;

    REQUIRE(abs(-1.f) == 1.f);
    REQUIRE(abs(4.56f) == 4.56f);
    REQUIRE(abs(-1.) == 1.);
    REQUIRE(abs(4.56) == 4.56);
    REQUIRE(abs(-5) == 5);
    REQUIRE(abs(3L) == 3L);
    REQUIRE(abs(-32L) == 32L);
    REQUIRE(abs(5000ULL) == 5000ULL);
}

TEST_CASE("Vector4 ctor and dtor") {
    using draco::math::Vector4;

    static constexpr Vector4 v{1.0f, 2.0f, 3.0f, 4.0f};
    static_assert(v[0] == 1.0f);
    static_assert(v[1] == 2.0f);
    static_assert(v[2] == 3.0f);
    static_assert(v[3] == 4.0f);

    REQUIRE(v[0] == 1.0f);
    REQUIRE(v[1] == 2.0f);
    REQUIRE(v[2] == 3.0f);
    REQUIRE(v[3] == 4.0f);
}

TEST_CASE("Vector4 swap") {
    using draco::math::Vector4;

    Vector4 a{1.f, 2.f, 3.f, 4.f};
    Vector4 b{4.f, 3.f, 2.f, 1.f};

    std::swap(a, b);

    REQUIRE(a == Vector4{4.f, 3.f, 2.f, 1.f});
    REQUIRE(b == Vector4{1.f, 2.f, 3.f, 4.f});
}

TEST_CASE("Vector4 dot") {
    using draco::math::Vector4;
    using draco::math::dot;

    static constexpr Vector4 a{1.0f, 2.0f, 3.0f, 4.0f};
    static constexpr Vector4 b{5.0f, 6.0f, 7.0f, 8.0f};

    const float result = dot(a, b);
    // 1 * 5 + 2 * 6 + 3 * 7 + 4 * 8
    static constexpr float expected = 70.0f;

    REQUIRE(result == expected);
}

TEST_CASE("Vector4 dot zero") {
    using draco::math::Vector4;
    using draco::math::dot;

    Vector4 a{0.0f, 0.0f, 0.0f, 0.0f};
    Vector4 b{1.0f, 2.0f, 3.0f, 4.0f};

    REQUIRE(dot(a, b) == 0.0f);
}

TEST_CASE("Vector4 dot self") {
    using draco::math::Vector4;
    using draco::math::dot;

    Vector4 v{1.0f, 2.0f, 3.0f, 4.0f};

    const float result = dot(v, v);
    constexpr float expected = 30.0f;

    REQUIRE(result == expected);
}
