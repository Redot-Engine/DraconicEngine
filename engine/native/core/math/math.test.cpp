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
};