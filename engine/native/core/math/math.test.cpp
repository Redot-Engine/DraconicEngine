import boost.ut;
import core.math;

using namespace boost::ut;

suite<"core.math"> core_math_test = [] {
    "pow"_test = [] {
        double result = draco::math::pow(2., .5);
        constexpr double expected = std::numbers::sqrt2_v<double>;
        expect(result == expected);
    };

    "abs"_test = [] {
        using draco::math::abs;

        expect(abs(-1.f) == 1.f);
        expect(abs(4.56f) == 4.56f);
        expect(abs(-1.) == 1.);
        expect(abs(4.56) == 4.56);
        expect(abs(-5) == 5);
        expect(abs(3L) == 3L);
        expect(abs(-32L) == 32L);
        expect(abs(5000ULL) == 5000ULL);
    };
};

suite<"core.math.vector4"> vector4_tests = [] {
    "construct_and_access"_test = [] {
        using draco::math::Vector4;
        static constexpr Vector4 v{1.0f, 2.0f, 3.0f, 4.0f};
        static_assert(v[0] == 1.0f);
        static_assert(v[1] == 2.0f);
        static_assert(v[2] == 3.0f);
        static_assert(v[3] == 4.0f);
        expect(v[0] == 1.0f);
        expect(v[1] == 2.0f);
        expect(v[2] == 3.0f);
        expect(v[3] == 4.0f);
    };

    "construct_from_vectors"_test = [] {
        using draco::math::Vector2;
        using draco::math::Vector3;
        using draco::math::Vector4;

        Vector2 a{1.0f, 2.0f};
        Vector3 b{3.0f, 4.0f, 5.0f};

        Vector4 c(a, 6.0f, 7.0f);
        Vector4 d(6.0f, a, 7.0f);
        Vector4 e(6.0f, 7.0f, a);
        Vector4 f(a, a);
        Vector4 g(b, 6.0f);
        Vector4 h(6.0f, b);
        Vector4 i(a);
        Vector4 j(b);
        
        expect(c[0] == 1.0f);
        expect(c[1] == 2.0f);
        expect(c[2] == 6.0f);
        expect(c[3] == 7.0f);
        
        expect(d[0] == 6.0f);
        expect(d[1] == 1.0f);
        expect(d[2] == 2.0f);
        expect(d[3] == 7.0f);
        
        expect(e[0] == 6.0f);
        expect(e[1] == 7.0f);
        expect(e[2] == 1.0f);
        expect(e[3] == 2.0f);
        
        expect(f[0] == 1.0f);
        expect(f[1] == 2.0f);
        expect(f[2] == 1.0f);
        expect(f[3] == 2.0f);
        
        expect(g[0] == 3.0f);
        expect(g[1] == 4.0f);
        expect(g[2] == 5.0f);
        expect(g[3] == 6.0f);
        
        expect(h[0] == 6.0f);
        expect(h[1] == 3.0f);
        expect(h[2] == 4.0f);
        expect(h[3] == 5.0f);
        
        expect(i[0] == 1.0f);
        expect(i[1] == 2.0f);
        expect(i[2] == 0.0f);
        expect(i[3] == 0.0f);
        
        expect(j[0] == 3.0f);
        expect(j[1] == 4.0f);
        expect(j[2] == 5.0f);
        expect(j[3] == 0.0f);
    };

    "swizzle"_test = [] {
        using draco::math::Vector2;
        using draco::math::Vector3;
        using draco::math::Vector4;
        
        Vector4 a{1.0f, 2.0f, 3.0f, 4.0f};
        Vector4 b = a[0, 2, 1, 3];
        Vector3 c = a[1, 2, 0];
        Vector2 d = a[1, 0];

        expect(b[0] == 1.0f);
        expect(b[1] == 3.0f);
        expect(b[2] == 2.0f);
        expect(b[3] == 4.0f);

        expect(c[0] == 2.0f);
        expect(c[1] == 3.0f);
        expect(c[2] == 1.0f);

        expect(d[0] == 2.0f);
        expect(d[1] == 1.0f);
    };

    "swap"_test = [] {
        using draco::math::Vector4;

        Vector4 a{1.f, 2.f, 3.f, 4.f};
        Vector4 b{4.f, 3.f, 2.f, 1.f};

        std::swap(a, b);

        expect(a == Vector4{4.f, 3.f, 2.f, 1.f});
        expect(b == Vector4{1.f, 2.f, 3.f, 4.f});
    };

    "dot_basic"_test = [] {
        using draco::math::Vector4;

        static constexpr Vector4 a{1.0f, 2.0f, 3.0f, 4.0f};
        static constexpr Vector4 b{5.0f, 6.0f, 7.0f, 8.0f};

        const float result = draco::math::dot(a, b);
        // 1 * 5 + 2 * 6 + 3 * 7 + 4 * 8
        const float expected = 70.0f;

        expect(result == expected);
    };

    "dot_zero"_test = [] {
        using draco::math::Vector4;
        using draco::math::dot;

        Vector4 a{0.0f, 0.0f, 0.0f, 0.0f};
        Vector4 b{1.0f, 2.0f, 3.0f, 4.0f};

        expect(dot(a, b) == 0.0f);
    };

    "dot_self"_test = [] {
        using draco::math::Vector4;
        using draco::math::dot;

        Vector4 v{1.0f, 2.0f, 3.0f, 4.0f};

        const float result = dot(v, v);
        constexpr float expected = 30.0f;

        expect(result == expected);
    };

    "length"_test = [] {
        using draco::math::Vector4;
        using draco::math::length;
        using draco::math::length_sq;

        Vector4 v{1.0f, 2.0f, 2.0f, 4.0f};

        const float result = length(v);
        const float result_sq = length_sq(v);
        constexpr float expected = 5.0f;
        constexpr float expected_sq = 25.0f;

        expect(result == expected);
        expect(result_sq == expected_sq);
    };

    "distance"_test = [] {
        using draco::math::Vector4;
        using draco::math::distance;
        using draco::math::distance_sq;

        Vector4 a{1.0f, 2.0f, 2.0f, 4.0f};
        Vector4 b{3.0f, 6.0f, 7.0f, 10.0f};

        const float result = distance(a, b);
        const float result_sq = distance_sq(a, b);
        constexpr float expected = 9.0f;
        constexpr float expected_sq = 81.0f;

        expect(result == expected);
        expect(result_sq == expected_sq);
    };

    "normalize"_test = [] {
        using draco::math::Vector4;
        using draco::math::length;
        using draco::math::normalize;
        using draco::math::normalize_fast;

        Vector4 a{2.0f, 4.0f, 5.0f, 6.0f};
        Vector4 b(1e-20);

        const Vector4 result = normalize(a);
        const Vector4 result_fast = normalize_fast(a);
        const Vector4 result_zero = normalize(b);

        expect(length(result) == 1.0f);
        expect(result == result_fast);
        expect(result_zero == Vector4());
    };
};