#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>
#include <type_traits>

#define R_CHECK_EQ(L_expr, R_expr) {                                 \
    const auto L_run = (L_expr);                                     \
    const auto R_run = (R_expr);                                     \
    static_assert(std::is_same_v<decltype(L_run), decltype(R_run)>); \
    CHECK_EQ(L_run, R_run);                                          \
}

#define RAC_CHECK_EQ(L_expr, R_expr) {                                 \
    static constexpr auto L_comp = (L_expr);                           \
    static constexpr auto R_comp = (R_expr);                           \
    static_assert(std::is_same_v<decltype(L_comp), decltype(R_comp)>); \
    static_assert(L_comp == R_comp);                                   \
    R_CHECK_EQ(L_expr, R_expr);                                        \
}

#define BASIC_R_SUBCASE(name, L_expr, R_expr) \
    SUBCASE(name) { R_CHECK_EQ(L_expr, R_expr); }

#define BASIC_R_SUBCASE_2(name, L_expr1, R_expr1, L_expr2, R_expr2) \
    SUBCASE(name) { R_CHECK_EQ(L_expr1, R_expr1); R_CHECK_EQ(L_expr2, R_expr2); }

#define BASIC_RAC_SUBCASE(name, L_expr, R_expr) \
    SUBCASE(name) { RAC_CHECK_EQ(L_expr, R_expr); }

#define BASIC_RAC_SUBCASE_2(name, L_expr1, R_expr1, L_expr2, R_expr2) \
    SUBCASE(name) { RAC_CHECK_EQ(L_expr1, R_expr1); RAC_CHECK_EQ(L_expr2, R_expr2); }

import core.math;

using namespace draco;

TEST_SUITE("math") {
	TEST_CASE("pow") {
		f32 result             = draco::math::pow(2.0F, 0.5F);
		constexpr f32 expected = draco::math::SQRT2;
		CHECK_EQ(result, expected);
	}

	TEST_CASE("abs") {
		using draco::math::abs;

		RAC_CHECK_EQ(abs(-1.F), 1.F);
		RAC_CHECK_EQ(abs(4.56F), 4.56F);
		RAC_CHECK_EQ(abs(-1.), 1.);
		RAC_CHECK_EQ(abs(4.56), 4.56);
		RAC_CHECK_EQ(abs(-5), 5);
		RAC_CHECK_EQ(abs(3L), 3L);
		RAC_CHECK_EQ(abs(-32L), 32L);
		RAC_CHECK_EQ(abs(5000ULL), 5000ULL);
	}
}

TEST_SUITE("vector2") {
	TEST_CASE("constructors") {
		using draco::math::Vector2;
		using draco::math::Vector3;
		using draco::math::Vector4;

		static constexpr Vector3 a{1.0F, 2.0F, 3.0F};
		static constexpr Vector4 b{4.0F, 5.0F, 6.0F, 7.0F};

		BASIC_RAC_SUBCASE("float", (Vector2(1.0F)), (Vector2{1.0F, 1.0F}));

		BASIC_RAC_SUBCASE("vec3", (Vector2(a)), (Vector2{1.0F, 2.0F}));

		BASIC_RAC_SUBCASE("vec4", (Vector2(b)), (Vector2{4.0F, 5.0F}));
	}

	TEST_CASE("access") {
		using draco::math::Vector2;

		static constexpr Vector2 v(1.0F, 2.0F);

		RAC_CHECK_EQ(v[0], 1.0F);
		RAC_CHECK_EQ(v[1], 2.0F);
	}

	TEST_CASE("swizzle") {
		using draco::math::Vector2;
		using draco::math::Vector3;
		using draco::math::Vector4;

		static constexpr Vector2 v{1.0F, 2.0F};

		BASIC_RAC_SUBCASE("vec2", (v[1, 0]), (Vector2{2.0F, 1.0F}));

		BASIC_RAC_SUBCASE("vec3", (v[1, 1, 0]), (Vector3{2.0F, 2.0F, 1.0F}));

		BASIC_RAC_SUBCASE("vec4", (v[0, 1, 1, 0]),
		                  (Vector4{1.0F, 2.0F, 2.0F, 1.0F}));
	}

	TEST_CASE("swap") {
		using draco::math::Vector2;

		Vector2 a{1.F, 2.F};
		Vector2 b{2.F, 1.F};

		std::swap(a, b);

		CHECK_EQ(a, Vector2{2.F, 1.F});
		CHECK_EQ(b, Vector2{1.F, 2.F});
	}

	TEST_CASE("dot") {
		using draco::math::Vector2;
		using draco::math::dot;

		static constexpr Vector2 a{1.0F, 2.0F};
		static constexpr Vector2 b{3.0F, 4.0F};

		BASIC_RAC_SUBCASE("basic", (dot(a, b)), (11.0F));

		BASIC_RAC_SUBCASE("self", (dot(a, a)), (5.0F));

		BASIC_RAC_SUBCASE("zero", (dot(a, Vector2())), (0.0F));
	}

	TEST_CASE("length") {
		using draco::math::Vector2;
		using draco::math::length;
		using draco::math::length_sq;

		static constexpr Vector2 v{3.0F, 4.0F};

		BASIC_R_SUBCASE("normal", (length(v)), (5.0F));

		BASIC_RAC_SUBCASE("squared", (length_sq(v)), (25.0F));
	}

	TEST_CASE("distance") {
		using draco::math::Vector2;
		using draco::math::distance;
		using draco::math::distance_sq;

		static constexpr Vector2 a{3.0F, 4.0F};
		static constexpr Vector2 b{-3.0F, 12.0F};

		BASIC_R_SUBCASE("normal", (distance(a, b)), (10.0F));

		BASIC_RAC_SUBCASE("squared", (distance_sq(a, b)), (100.0F));
	}

	TEST_CASE("normalize") {
		using draco::math::Vector2;
		using draco::math::length;
		using draco::math::normalize;
		using draco::math::normalize_fast;

		static constexpr Vector2 a{3.0F, 4.0F};
		static constexpr Vector2 b(1e-99);

		Vector2 const result      = normalize(a);
		Vector2 const result_fast = normalize_fast(a);
		Vector2 const result_zero = normalize(b);

		CHECK_EQ(length(result), 1.0F);
		CHECK_EQ(result, result_fast);
		CHECK_EQ(result_zero, Vector2());
	}

	TEST_CASE("project") {
		using draco::math::Vector2;
		using draco::math::project;

		static constexpr Vector2 a{4.0F, 6.0F};
		static constexpr Vector2 b{2.0F, 2.0F};

		RAC_CHECK_EQ((project(a, b)), (Vector2{5.0F, 5.0F}));
	}

	TEST_CASE("reflect") {
		using draco::math::Vector2;
		using draco::math::reflect;

		static constexpr Vector2 a{1.0F, 2.0F};
		static constexpr Vector2 b{3.0F, 4.0F};

		RAC_CHECK_EQ((reflect(a, b)), (Vector2{-65.0F, -86.0F}));
	}

	TEST_CASE("angle") {
		using draco::math::Vector2;
		using draco::math::angle;
		using draco::math::PI2;

		static constexpr Vector2 a{2.0F, 1.0F};
		static constexpr Vector2 b{-2.0F, 4.0F};

		R_CHECK_EQ((angle(a, b)), (PI2));
	}

	TEST_CASE("lerp") {
		using draco::math::Vector2;
		using draco::math::lerp;

		static constexpr Vector2 a{1.0F, 2.0F};
		static constexpr Vector2 b{3.0F, 4.0F};

		BASIC_RAC_SUBCASE("weight = -1", (lerp(a, b, -1.0F)),
		                  (Vector2{-1.0F, 0.0F}));

		BASIC_RAC_SUBCASE("weight = 0", (lerp(a, b, 0.0F)), (a));

		BASIC_RAC_SUBCASE("weight = 0.5", (lerp(a, b, 0.5F)),
		                  (Vector2{2.0F, 3.0F}));

		BASIC_RAC_SUBCASE("weight = 1", (lerp(a, b, 1.0F)), (b));

		BASIC_RAC_SUBCASE("weight = 2", (lerp(a, b, 2.0F)),
		                  (Vector2{5.0F, 6.0F}));
	}

	TEST_CASE("min") {
		using draco::math::Vector2;
		using draco::math::min;

		static constexpr Vector2 a{5.0F, 3.0F};
		static constexpr Vector2 b{1.0F, 7.0F};

		BASIC_RAC_SUBCASE("vector", (min(a, b)), (Vector2{1.0F, 3.0F}));

		static constexpr Vector2 expected{4.0F, 3.0F};

		BASIC_RAC_SUBCASE_2("float", (min(a, 4.0F)), (expected), (min(4.0F, a)),
		                    (expected));
	}

	TEST_CASE("min_length") {
		using draco::math::Vector2;
		using draco::math::length;
		using draco::math::min_length;

		static constexpr Vector2 a{3.0F, 4.0F};  // len: 5
		static constexpr Vector2 b{5.0F, 12.0F}; // len: 13

		BASIC_RAC_SUBCASE("vector", (min_length(a, b)), (a));

		SUBCASE("float") {
			static constexpr f32 smaller_length = 1.0F;
			static constexpr f32 larger_length  = 10.0F;

			Vector2 const result_smaller = min_length(a, smaller_length);
			Vector2 const result_swapped = min_length(smaller_length, a);
			Vector2 const result_larger  = min_length(larger_length, a);

			CHECK_EQ(length(result_smaller), smaller_length);
			CHECK_EQ(result_smaller, result_swapped);
			CHECK_EQ(result_larger, a);
		}
	}

	TEST_CASE("max") {
		using draco::math::Vector2;
		using draco::math::max;

		static constexpr Vector2 a{5.0F, 3.0F};
		static constexpr Vector2 b{1.0F, 7.0F};

		BASIC_RAC_SUBCASE("vector", (max(a, b)), (Vector2{5.0F, 7.0F}));

		static constexpr Vector2 expected{5.0F, 4.0F};

		BASIC_RAC_SUBCASE_2("float", (max(a, 4.0F)), (expected), (max(4.0F, a)),
		                    (expected));
	}

	TEST_CASE("max_length") {
		using draco::math::Vector2;
		using draco::math::length;
		using draco::math::max_length;

		static constexpr Vector2 a{3.0F, 4.0F};  // len: 5
		static constexpr Vector2 b{5.0F, 12.0F}; // len: 13

		BASIC_RAC_SUBCASE("vector", (max_length(a, b)), (b));

		SUBCASE("float") {
			static constexpr f32 smaller_length = 1.0F;
			static constexpr f32 larger_length  = 10.0F;

			Vector2 const result_smaller = max_length(a, smaller_length);
			Vector2 const result_swapped = max_length(larger_length, a);
			Vector2 const result_larger  = max_length(larger_length, a);

			CHECK_EQ(length(result_larger), larger_length);
			CHECK_EQ(result_larger, result_swapped);
			CHECK_EQ(result_smaller, a);
		}
	}

	TEST_CASE("clamp") {
		using draco::math::Vector2;
		using draco::math::clamp;

		static constexpr Vector2 a{5.0F, 3.0F};
		static constexpr Vector2 b{1.0F, 7.0F};
		static constexpr Vector2 c{3.0F, 8.0F};

		BASIC_RAC_SUBCASE("vector", (clamp(a, b, c)), (Vector2{3.0F, 7.0F}));

		BASIC_RAC_SUBCASE("float", (clamp(a, 4.0F, 5.0F)),
		                  (Vector2{5.0F, 4.0F}));
	}

	TEST_CASE("clamp_length") {
		using draco::math::Vector2;
		using draco::math::length;
		using draco::math::clamp_length;

		static constexpr Vector2 v{3.0F, 4.0F}; // len: 5

		BASIC_R_SUBCASE("length < min", (clamp_length(v, 10.0F, 15.0F)),
		                (Vector2{6.0F, 8.0F}));

		BASIC_R_SUBCASE("length == min", (clamp_length(v, 5.0F, 10.0F)), (v));

		BASIC_R_SUBCASE("length == max", (clamp_length(v, 3.0F, 5.0F)), (v));

		BASIC_R_SUBCASE("length > max", (clamp_length(v, 1.0F, 2.5F)),
		                (Vector2{1.5F, 2.0F}));
	}

	TEST_CASE("abs") {
		using draco::math::Vector2;
		using draco::math::abs;

		RAC_CHECK_EQ((abs(Vector2{1.0F, -2.0F})), (Vector2{1.0F, 2.0F}));
	}

	TEST_CASE("rounding") {
		using draco::math::Vector2;
		using draco::math::floor;
		using draco::math::ceil;
		using draco::math::trunc;
		using draco::math::round;

		static constexpr Vector2 a{0.5F, 1.4F};
		static constexpr Vector2 b{-1.0F, 1.0F};

		BASIC_RAC_SUBCASE_2("floor", (floor(a)), (Vector2{0.0F, 1.0F}),
		                    (floor(b)), (b));

		BASIC_RAC_SUBCASE_2("ceil", (ceil(a)), (Vector2{1.0F, 2.0F}), (ceil(b)),
		                    (b));

		BASIC_RAC_SUBCASE_2("trunc", (trunc(a)), (Vector2{0.0F, 1.0F}),
		                    (trunc(b)), (b));

		BASIC_RAC_SUBCASE_2("round", (round(a)), (Vector2{1.0F, 1.0F}),
		                    (round(b)), (b));
	}

	TEST_CASE("sign") {
		using draco::math::Vector2;
		using draco::math::sign;

		RAC_CHECK_EQ((sign(Vector2{1.0F, -1.0F})), (Vector2{1.0F, -1.0F}));
	}

	TEST_CASE("approx_eq") {
		using draco::math::Vector2;
		using draco::math::approx_eq;
		using draco::math::CMP_EPSILON;

		static constexpr Vector2 v{1.0F, 2.0F};
		static constexpr Vector2 offset = Vector2::x_axis(CMP_EPSILON);

		BASIC_R_SUBCASE("distance < epsilon", (approx_eq(v, v + offset * 0.5F)),
		                (true));

		BASIC_R_SUBCASE("distance == epsilon", (approx_eq(v, v + offset)),
		                (true));

		BASIC_R_SUBCASE("distance > epsilon", (approx_eq(v, v + offset * 2.0F)),
		                (false));
	}
}

TEST_SUITE("vector3") {
	TEST_CASE("constructors") {
		using draco::math::Vector2;
		using draco::math::Vector3;
		using draco::math::Vector4;

		static constexpr Vector2 a{1.0F, 2.0F};
		static constexpr Vector4 b{3.0F, 4.0F, 5.0F, 6.0F};

		BASIC_RAC_SUBCASE("float", (Vector3(1.0F)),
		                  (Vector3{1.0F, 1.0F, 1.0F}));

		BASIC_RAC_SUBCASE("vec2, float", (Vector3(a, 7.0F)),
		                  (Vector3{1.0F, 2.0F, 7.0F}));

		BASIC_RAC_SUBCASE("float, vec2", (Vector3(7.0F, a)),
		                  (Vector3{7.0F, 1.0F, 2.0F}));

		BASIC_RAC_SUBCASE("vec4", (Vector3(b)), (Vector3{3.0F, 4.0F, 5.0F}));
	}

	TEST_CASE("access") {
		using draco::math::Vector3;

		static constexpr Vector3 v(1.0F, 2.0F, 3.0F);

		RAC_CHECK_EQ(v[0], 1.0F);
		RAC_CHECK_EQ(v[1], 2.0F);
		RAC_CHECK_EQ(v[2], 3.0F);
	}

	TEST_CASE("swizzle") {
		using draco::math::Vector2;
		using draco::math::Vector3;
		using draco::math::Vector4;

		static constexpr Vector3 v{1.0F, 2.0F, 3.0F};

		BASIC_RAC_SUBCASE("vec2", (v[1, 0]), (Vector2{2.0F, 1.0F}));

		BASIC_RAC_SUBCASE("vec3", (v[1, 2, 0]), (Vector3{2.0F, 3.0F, 1.0F}));

		BASIC_RAC_SUBCASE("vec4", (v[0, 2, 1, 0]),
		                  (Vector4{1.0F, 3.0F, 2.0F, 1.0F}));
	}

	TEST_CASE("swap") {
		using draco::math::Vector3;

		Vector3 a{1.F, 2.F, 3.F};
		Vector3 b{3.F, 2.F, 1.F};

		std::swap(a, b);

		CHECK_EQ(a, Vector3{3.F, 2.F, 1.F});
		CHECK_EQ(b, Vector3{1.F, 2.F, 3.F});
	}

	TEST_CASE("dot") {
		using draco::math::Vector3;
		using draco::math::dot;

		static constexpr Vector3 a{1.0F, 2.0F, 3.0F};
		static constexpr Vector3 b{4.0F, 5.0F, 6.0F};

		BASIC_RAC_SUBCASE("basic", (dot(a, b)), (32.0F));

		BASIC_RAC_SUBCASE("self", (dot(a, a)), (14.0F));

		BASIC_RAC_SUBCASE("zero", (dot(a, Vector3())), (0.0F));
	}

	TEST_CASE("length") {
		using draco::math::Vector3;
		using draco::math::length;
		using draco::math::length_sq;

		static constexpr Vector3 v{2.0F, 4.0F, 4.0F};

		BASIC_R_SUBCASE("normal", (length(v)), (6.0F));

		BASIC_RAC_SUBCASE("squared", (length_sq(v)), (36.0F));
	}

	TEST_CASE("distance") {
		using draco::math::Vector3;
		using draco::math::distance;
		using draco::math::distance_sq;

		static constexpr Vector3 a{2.0F, 4.0F, 4.0F};
		static constexpr Vector3 b{-1.0F, -2.0F, -2.0F};

		BASIC_R_SUBCASE("normal", (distance(a, b)), (9.0F));

		BASIC_RAC_SUBCASE("squared", (distance_sq(a, b)), (81.0F));
	}

	TEST_CASE("normalize") {
		using draco::math::Vector3;
		using draco::math::length;
		using draco::math::normalize;
		using draco::math::normalize_fast;

		static constexpr Vector3 a{0.0F, 6.4F, 4.8F};
		static constexpr Vector3 b(1e-99);

		Vector3 const result      = normalize(a);
		Vector3 const result_fast = normalize_fast(a);
		Vector3 const result_zero = normalize(b);

		CHECK_EQ(length(result), 1.0F);
		CHECK_EQ(result, result_fast);
		CHECK_EQ(result_zero, Vector3());
	}

	TEST_CASE("project") {
		using draco::math::Vector3;
		using draco::math::project;

		static constexpr Vector3 a{2.0F, 8.0F, 4.0F};
		static constexpr Vector3 b{1.0F, 1.0F, 2.0F};

		RAC_CHECK_EQ((project(a, b)), (Vector3{3.0F, 3.0F, 6.0F}));
	}

	TEST_CASE("reflect") {
		using draco::math::Vector3;
		using draco::math::reflect;

		static constexpr Vector3 a{1.0F, 2.0F, 3.0F};
		static constexpr Vector3 b{4.0F, 5.0F, 6.0F};

		RAC_CHECK_EQ((reflect(a, b)), (Vector3{-255.0F, -318.0F, -381.0F}));
	}

	TEST_CASE("angle") {
		using draco::math::Vector3;
		using draco::math::angle;
		using draco::math::PI;

		static constexpr Vector3 a{2.0F, 4.0F, 4.0F};
		static constexpr Vector3 b{-4.0F, -8.0F, -8.0F};

		R_CHECK_EQ((angle(a, b)), (PI));
	}

	TEST_CASE("lerp") {
		using draco::math::Vector3;
		using draco::math::lerp;

		static constexpr Vector3 a{1.0F, 2.0F, 3.0F};
		static constexpr Vector3 b{4.0F, 5.0F, 6.0F};

		BASIC_RAC_SUBCASE("weight = -1", (lerp(a, b, -1.0F)),
		                  (Vector3{-2.0F, -1.0F, -0.0F}));

		BASIC_RAC_SUBCASE("weight = 0", (lerp(a, b, 0.0F)), (a));

		BASIC_RAC_SUBCASE("weight = 0.5", (lerp(a, b, 0.5F)),
		                  (Vector3{2.5F, 3.5F, 4.5F}));

		BASIC_RAC_SUBCASE("weight = 1", (lerp(a, b, 1.0F)), (b));

		BASIC_RAC_SUBCASE("weight = 2", (lerp(a, b, 2.0F)),
		                  (Vector3{7.0F, 8.0F, 9.0F}));
	}

	TEST_CASE("min") {
		using draco::math::Vector3;
		using draco::math::min;

		static constexpr Vector3 a{5.0F, 8.0F, 3.0F};
		static constexpr Vector3 b{1.0F, 6.0F, 7.0F};

		BASIC_RAC_SUBCASE("vector", (min(a, b)), (Vector3{1.0F, 6.0F, 3.0F}));

		static constexpr Vector3 expected{4.0F, 4.0F, 3.0F};

		BASIC_RAC_SUBCASE_2("float", (min(a, 4.0F)), (expected), (min(4.0F, a)),
		                    (expected));
	}

	TEST_CASE("min_length") {
		using draco::math::Vector3;
		using draco::math::length;
		using draco::math::min_length;

		static constexpr Vector3 a{2.0F, 4.0F, 4.0F};   // len: 6
		static constexpr Vector3 b{5.0F, 10.0F, 10.0F}; // len: 15

		BASIC_RAC_SUBCASE("vector", (min_length(a, b)), (a));

		SUBCASE("float") {
			static constexpr f32 smaller_length = 1.0F;
			static constexpr f32 larger_length  = 10.0F;

			Vector3 const result_smaller = min_length(a, smaller_length);
			Vector3 const result_swapped = min_length(smaller_length, a);
			Vector3 const result_larger  = min_length(larger_length, a);

			CHECK_EQ(length(result_smaller), smaller_length);
			CHECK_EQ(result_smaller, result_swapped);
			CHECK_EQ(result_larger, a);
		}
	}

	TEST_CASE("max") {
		using draco::math::Vector3;
		using draco::math::max;

		static constexpr Vector3 a{5.0F, 8.0F, 3.0F};
		static constexpr Vector3 b{1.0F, 6.0F, 7.0F};

		BASIC_RAC_SUBCASE("vector", (max(a, b)), (Vector3{5.0F, 8.0F, 7.0F}));

		static constexpr Vector3 expected{5.0F, 8.0F, 4.0F};

		BASIC_RAC_SUBCASE_2("float", (max(a, 4.0F)), (expected), (max(4.0F, a)),
		                    (expected));
	}

	TEST_CASE("max_length") {
		using draco::math::Vector3;
		using draco::math::length;
		using draco::math::max_length;

		static constexpr Vector3 a{2.0F, 4.0F, 4.0F};   // len: 6
		static constexpr Vector3 b{5.0F, 10.0F, 10.0F}; // len: 15

		BASIC_RAC_SUBCASE("vector", (max_length(a, b)), (b));

		SUBCASE("float") {
			static constexpr f32 smaller_length = 1.0F;
			static constexpr f32 larger_length  = 10.0F;

			Vector3 const result_smaller = max_length(a, smaller_length);
			Vector3 const result_swapped = max_length(larger_length, a);
			Vector3 const result_larger  = max_length(larger_length, a);

			CHECK_EQ(length(result_larger), larger_length);
			CHECK_EQ(result_larger, result_swapped);
			CHECK_EQ(result_smaller, a);
		}
	}

	TEST_CASE("clamp") {
		using draco::math::Vector3;
		using draco::math::clamp;

		static constexpr Vector3 a{5.0F, 8.0F, 3.0F};
		static constexpr Vector3 b{1.0F, 6.0F, 7.0F};
		static constexpr Vector3 c{3.0F, 9.0F, 8.0F};

		BASIC_RAC_SUBCASE("vector", (clamp(a, b, c)),
		                  (Vector3{3.0F, 8.0F, 7.0F}));

		BASIC_RAC_SUBCASE("float", (clamp(a, 4.0F, 5.0F)),
		                  (Vector3{5.0F, 5.0F, 4.0F}));
	}

	TEST_CASE("clamp_length") {
		using draco::math::Vector3;
		using draco::math::length;
		using draco::math::clamp_length;

		static constexpr Vector3 v{2.0F, 4.0F, 4.0F}; // len: 6

		BASIC_R_SUBCASE("length < min", (clamp_length(v, 12.0F, 14.0F)),
		                (Vector3{4.0F, 8.0F, 8.0F}));

		BASIC_R_SUBCASE("length == min", (clamp_length(v, 6.0F, 9.0F)), (v));

		BASIC_R_SUBCASE("length == max", (clamp_length(v, 3.0F, 6.0F)), (v));

		BASIC_R_SUBCASE("length > max", (clamp_length(v, 1.0F, 3.0F)),
		                (Vector3{1.0F, 2.0F, 2.0F}));
	}

	TEST_CASE("abs") {
		using draco::math::Vector3;
		using draco::math::abs;

		RAC_CHECK_EQ((abs(Vector3{1.0F, -2.0F, 0.0F})),
		             (Vector3{1.0F, 2.0F, 0.0F}));
	}

	TEST_CASE("rounding") {
		using draco::math::Vector3;
		using draco::math::floor;
		using draco::math::ceil;
		using draco::math::trunc;
		using draco::math::round;

		static constexpr Vector3 a{0.5F, -0.5F, 1.4F};
		static constexpr Vector3 b{-1.0F, 0.0F, 1.0F};

		BASIC_RAC_SUBCASE_2("floor", (floor(a)), (Vector3{0.0F, -1.0F, 1.0F}),
		                    (floor(b)), (b));

		BASIC_RAC_SUBCASE_2("ceil", (ceil(a)), (Vector3{1.0F, 0.0F, 2.0F}),
		                    (ceil(b)), (b));

		BASIC_RAC_SUBCASE_2("trunc", (trunc(a)), (Vector3{0.0F, 0.0F, 1.0F}),
		                    (trunc(b)), (b));

		BASIC_RAC_SUBCASE_2("round", (round(a)), (Vector3{1.0F, -1.0F, 1.0F}),
		                    (round(b)), (b));
	}

	TEST_CASE("sign") {
		using draco::math::Vector3;
		using draco::math::sign;

		RAC_CHECK_EQ((sign(Vector3{1.0F, -1.0F, 0.0F})),
		             (Vector3{1.0F, -1.0F, 0.0F}));
	}

	TEST_CASE("approx_eq") {
		using draco::math::Vector3;
		using draco::math::approx_eq;
		using draco::math::CMP_EPSILON;

		static constexpr Vector3 v{1.0F, 2.0F, 3.0F};
		static constexpr Vector3 offset = Vector3::x_axis(CMP_EPSILON);

		BASIC_R_SUBCASE("distance < epsilon", (approx_eq(v, v + offset * 0.5F)),
		                (true));

		BASIC_R_SUBCASE("distance == epsilon", (approx_eq(v, v + offset)),
		                (true));

		BASIC_R_SUBCASE("distance > epsilon", (approx_eq(v, v + offset * 2.0F)),
		                (false));
	}

	TEST_CASE("cross") {
		using draco::math::Vector3;
		using draco::math::cross;

		RAC_CHECK_EQ((cross(Vector3::x_axis(), Vector3::y_axis())),
		             (Vector3::z_axis()))
	}
}

TEST_SUITE("vector4") {
	TEST_CASE("constructors") {
		using draco::math::Vector2;
		using draco::math::Vector3;
		using draco::math::Vector4;

		static constexpr Vector2 a{1.0F, 2.0F};
		static constexpr Vector3 b{3.0F, 4.0F, 5.0F};

		BASIC_RAC_SUBCASE("float", (Vector4(1.0F)),
		                  (Vector4{1.0F, 1.0F, 1.0F, 1.0F}));

		BASIC_RAC_SUBCASE("vec2, float, float", (Vector4(a, 6.0F, 7.0F)),
		                  (Vector4{1.0F, 2.0F, 6.0F, 7.0F}));

		BASIC_RAC_SUBCASE("float, vec2, float", (Vector4(6.0F, a, 7.0F)),
		                  (Vector4{6.0F, 1.0F, 2.0F, 7.0F}));

		BASIC_RAC_SUBCASE("float, float, vec2", (Vector4(6.0F, 7.0F, a)),
		                  (Vector4{6.0F, 7.0F, 1.0F, 2.0F}));

		BASIC_RAC_SUBCASE("vec2, vec2", (Vector4(a, a)),
		                  (Vector4{1.0F, 2.0F, 1.0F, 2.0F}));

		BASIC_RAC_SUBCASE("vec3, float", (Vector4(b, 6.0F)),
		                  (Vector4{3.0F, 4.0F, 5.0F, 6.0F}));

		BASIC_RAC_SUBCASE("float, vec3", (Vector4(6.0F, b)),
		                  (Vector4{6.0F, 3.0F, 4.0F, 5.0F}));

		BASIC_RAC_SUBCASE("vec2", (Vector4(a)),
		                  (Vector4{1.0F, 2.0F, 0.0F, 0.0F}));

		BASIC_RAC_SUBCASE("vec3", (Vector4(b)),
		                  (Vector4{3.0F, 4.0F, 5.0F, 0.0F}));
	}

	TEST_CASE("access") {
		using draco::math::Vector4;

		static constexpr Vector4 v(1.0F, 2.0F, 3.0F, 4.0F);

		RAC_CHECK_EQ(v[0], 1.0F);
		RAC_CHECK_EQ(v[1], 2.0F);
		RAC_CHECK_EQ(v[2], 3.0F);
		RAC_CHECK_EQ(v[3], 4.0F);
	}

	TEST_CASE("swizzle") {
		using draco::math::Vector2;
		using draco::math::Vector3;
		using draco::math::Vector4;

		static constexpr Vector4 v{1.0F, 2.0F, 3.0F, 4.0F};

		BASIC_RAC_SUBCASE("vec2", (v[1, 0]), (Vector2{2.0F, 1.0F}));

		BASIC_RAC_SUBCASE("vec3", (v[1, 2, 0]), (Vector3{2.0F, 3.0F, 1.0F}));

		BASIC_RAC_SUBCASE("vec4", (v[0, 2, 1, 3]),
		                  (Vector4{1.0F, 3.0F, 2.0F, 4.0F}));
	}

	TEST_CASE("swap") {
		using draco::math::Vector4;

		Vector4 a{1.F, 2.F, 3.F, 4.F};
		Vector4 b{4.F, 3.F, 2.F, 1.F};

		std::swap(a, b);

		CHECK_EQ(a, Vector4{4.F, 3.F, 2.F, 1.F});
		CHECK_EQ(b, Vector4{1.F, 2.F, 3.F, 4.F});
	}

	TEST_CASE("dot") {
		using draco::math::Vector4;
		using draco::math::dot;

		static constexpr Vector4 a{1.0F, 2.0F, 3.0F, 4.0F};
		static constexpr Vector4 b{5.0F, 6.0F, 7.0F, 8.0F};

		BASIC_RAC_SUBCASE("basic", (dot(a, b)), (70.0F));

		BASIC_RAC_SUBCASE("self", (dot(a, a)), (30.0F));

		BASIC_RAC_SUBCASE("zero", (dot(a, Vector4())), (0.0F));
	}

	TEST_CASE("length") {
		using draco::math::Vector4;
		using draco::math::length;
		using draco::math::length_sq;

		static constexpr Vector4 v{1.0F, 2.0F, 2.0F, 4.0F};

		BASIC_R_SUBCASE("normal", (length(v)), (5.0F));

		BASIC_RAC_SUBCASE("squared", (length_sq(v)), (25.0F));
	}

	TEST_CASE("distance") {
		using draco::math::Vector4;
		using draco::math::distance;
		using draco::math::distance_sq;

		static constexpr Vector4 a{1.0F, 2.0F, 2.0F, 4.0F};
		static constexpr Vector4 b{3.0F, 6.0F, 7.0F, 10.0F};

		BASIC_R_SUBCASE("normal", (distance(a, b)), (9.0F));

		BASIC_RAC_SUBCASE("squared", (distance_sq(a, b)), (81.0F));
	}

	TEST_CASE("normalize") {
		using draco::math::Vector4;
		using draco::math::length;
		using draco::math::normalize;
		using draco::math::normalize_fast;

		static constexpr Vector4 a{2.0F, 4.0F, 5.0F, 6.0F};
		static constexpr Vector4 b(1e-99);

		Vector4 const result      = normalize(a);
		Vector4 const result_fast = normalize_fast(a);
		Vector4 const result_zero = normalize(b);

		CHECK_EQ(length(result), 1.0F);
		CHECK_EQ(result, result_fast);
		CHECK_EQ(result_zero, Vector4());
	}

	TEST_CASE("project") {
		using draco::math::Vector4;
		using draco::math::project;

		static constexpr Vector4 a{8.0F, 2.0F, 6.0F, 8.0F};
		static constexpr Vector4 b{12.0F, 14.0F, 8.0F, 6.0F};

		RAC_CHECK_EQ((project(a, b)), (Vector4{6.0F, 7.0F, 4.0F, 3.0F}));
	}

	TEST_CASE("reflect") {
		using draco::math::Vector4;
		using draco::math::reflect;

		static constexpr Vector4 a{1.0F, 2.0F, 3.0F, 4.0F};
		static constexpr Vector4 b{5.0F, 6.0F, 7.0F, 8.0F};

		RAC_CHECK_EQ((reflect(a, b)),
		             (Vector4{-699.0F, -838.0F, -977.0F, -1116.0F}));
	}

	TEST_CASE("angle") {
		using draco::math::Vector4;
		using draco::math::angle;
		using draco::math::PI2;

		static constexpr Vector4 a{1.0F, 5.0F, 1.0F, 3.0F};
		static constexpr Vector4 b{2.0F, -6.0F, -2.0F, 10.0F};

		R_CHECK_EQ((angle(a, b)), (PI2));
	}

	TEST_CASE("lerp") {
		using draco::math::Vector4;
		using draco::math::lerp;

		static constexpr Vector4 a{1.0F, 2.0F, 3.0F, 4.0F};
		static constexpr Vector4 b{5.0F, 6.0F, 7.0F, 8.0F};

		BASIC_RAC_SUBCASE("weight = -1", (lerp(a, b, -1.0F)),
		                  (Vector4{-3.0F, -2.0F, -1.0F, 0.0F}));

		BASIC_RAC_SUBCASE("weight = 0", (lerp(a, b, 0.0F)), (a));

		BASIC_RAC_SUBCASE("weight = 0.5", (lerp(a, b, 0.5F)),
		                  (Vector4{3.0F, 4.0F, 5.0F, 6.0F}));

		BASIC_RAC_SUBCASE("weight = 1", (lerp(a, b, 1.0F)), (b));

		BASIC_RAC_SUBCASE("weight = 2", (lerp(a, b, 2.0F)),
		                  (Vector4{9.0F, 10.0F, 11.0F, 12.0F}));
	}

	TEST_CASE("min") {
		using draco::math::Vector4;
		using draco::math::min;

		static constexpr Vector4 a{5.0F, 8.0F, 3.0F, 4.0F};
		static constexpr Vector4 b{1.0F, 6.0F, 7.0F, 2.0F};

		BASIC_RAC_SUBCASE("vector", (min(a, b)),
		                  (Vector4{1.0F, 6.0F, 3.0F, 2.0F}));

		static constexpr Vector4 expected{4.0F, 4.0F, 3.0F, 4.0F};

		BASIC_RAC_SUBCASE_2("float", (min(a, 4.0F)), (expected), (min(4.0F, a)),
		                    (expected));
	}

	TEST_CASE("min_length") {
		using draco::math::Vector4;
		using draco::math::length;
		using draco::math::min_length;

		static constexpr Vector4 a{1.0F, 2.0F, 2.0F, 4.0F};   // len: 5
		static constexpr Vector4 b{1.0F, -3.0F, -1.0F, 5.0F}; // len: 6

		BASIC_RAC_SUBCASE("vector", (min_length(a, b)), (a));

		SUBCASE("float") {
			static constexpr f32 smaller_length = 1.0F;
			static constexpr f32 larger_length  = 10.0F;

			Vector4 const result_smaller = min_length(a, smaller_length);
			Vector4 const result_swapped = min_length(smaller_length, a);
			Vector4 const result_larger  = min_length(larger_length, a);

			CHECK_EQ(length(result_smaller), smaller_length);
			CHECK_EQ(result_smaller, result_swapped);
			CHECK_EQ(result_larger, a);
		}
	}

	TEST_CASE("max") {
		using draco::math::Vector4;
		using draco::math::max;

		static constexpr Vector4 a{5.0F, 8.0F, 3.0F, 4.0F};
		static constexpr Vector4 b{1.0F, 6.0F, 7.0F, 2.0F};

		BASIC_RAC_SUBCASE("vector", (max(a, b)),
		                  (Vector4{5.0F, 8.0F, 7.0F, 4.0F}));

		static constexpr Vector4 expected{5.0F, 8.0F, 4.0F, 4.0F};

		BASIC_RAC_SUBCASE_2("float", (max(a, 4.0F)), (expected), (max(4.0F, a)),
		                    (expected));
	}

	TEST_CASE("max_length") {
		using draco::math::Vector4;
		using draco::math::length;
		using draco::math::max_length;

		static constexpr Vector4 a{1.0F, 2.0F, 2.0F, 4.0F};   // len: 5
		static constexpr Vector4 b{1.0F, -3.0F, -1.0F, 5.0F}; // len: 6

		BASIC_RAC_SUBCASE("vector", (max_length(a, b)), (b));

		SUBCASE("float") {
			static constexpr f32 smaller_length = 1.0F;
			static constexpr f32 larger_length  = 10.0F;

			Vector4 const result_smaller = max_length(a, smaller_length);
			Vector4 const result_swapped = max_length(larger_length, a);
			Vector4 const result_larger  = max_length(larger_length, a);

			CHECK_EQ(length(result_larger), larger_length);
			CHECK_EQ(result_larger, result_swapped);
			CHECK_EQ(result_smaller, a);
		}
	}

	TEST_CASE("clamp") {
		using draco::math::Vector4;
		using draco::math::clamp;

		static constexpr Vector4 a{5.0F, 8.0F, 3.0F, 4.0F};
		static constexpr Vector4 b{1.0F, 6.0F, 7.0F, 2.0F};
		static constexpr Vector4 c{3.0F, 9.0F, 8.0F, 4.0F};

		BASIC_RAC_SUBCASE("vector", (clamp(a, b, c)),
		                  (Vector4{3.0F, 8.0F, 7.0F, 4.0F}));

		BASIC_RAC_SUBCASE("float", (clamp(a, 4.0F, 5.0F)),
		                  (Vector4{5.0F, 5.0F, 4.0F, 4.0F}));
	}

	TEST_CASE("clamp_length") {
		using draco::math::Vector4;
		using draco::math::length;
		using draco::math::clamp_length;

		static constexpr Vector4 v{1.0F, -3.0F, -1.0F, 5.0F}; // len: 6

		BASIC_R_SUBCASE("length < min", (clamp_length(v, 12.0F, 14.0F)),
		                (Vector4{2.0F, -6.0F, -2.0F, 10.0F}));

		BASIC_R_SUBCASE("length == min", (clamp_length(v, 6.0F, 9.0F)), (v));

		BASIC_R_SUBCASE("length == max", (clamp_length(v, 3.0F, 6.0F)), (v));

		BASIC_R_SUBCASE("length > max", (clamp_length(v, 1.0F, 3.0F)),
		                (Vector4{0.5F, -1.5F, -0.5F, 2.5F}));
	}

	TEST_CASE("abs") {
		using draco::math::Vector4;
		using draco::math::abs;

		RAC_CHECK_EQ((abs(Vector4{1.0F, -2.0F, -3.0F, 0.0F})),
		             (Vector4{1.0F, 2.0F, 3.0F, 0.0F}));
	}

	TEST_CASE("rounding") {
		using draco::math::Vector4;
		using draco::math::floor;
		using draco::math::ceil;
		using draco::math::trunc;
		using draco::math::round;

		static constexpr Vector4 a{0.5F, -0.5F, 1.4F, 1.6F};
		static constexpr Vector4 b{-1.0F, 0.0F, 1.0F, 2.0F};

		BASIC_RAC_SUBCASE_2("floor", (floor(a)),
		                    (Vector4{0.0F, -1.0F, 1.0F, 1.0F}), (floor(b)),
		                    (b));

		BASIC_RAC_SUBCASE_2("ceil", (ceil(a)),
		                    (Vector4{1.0F, 0.0F, 2.0F, 2.0F}), (ceil(b)), (b));

		BASIC_RAC_SUBCASE_2("trunc", (trunc(a)),
		                    (Vector4{0.0F, 0.0F, 1.0F, 1.0F}), (trunc(b)), (b));

		BASIC_RAC_SUBCASE_2("round", (round(a)),
		                    (Vector4{1.0F, -1.0F, 1.0F, 2.0F}), (round(b)),
		                    (b));
	}

	TEST_CASE("sign") {
		using draco::math::Vector4;
		using draco::math::sign;

		RAC_CHECK_EQ((sign(Vector4{1.0F, -1.0F, 0.0F, -0.0F})),
		             (Vector4{1.0F, -1.0F, 0.0F, 0.0F}));
	}

	TEST_CASE("approx_eq") {
		using draco::math::Vector4;
		using draco::math::approx_eq;
		using draco::math::CMP_EPSILON;

		static constexpr Vector4 v{1.0F, 2.0F, 3.0F, 4.0F};
		static constexpr Vector4 offset = Vector4::x_axis(CMP_EPSILON);

		BASIC_R_SUBCASE("distance < epsilon", (approx_eq(v, v + offset * 0.5F)),
		                (true));

		BASIC_R_SUBCASE("distance == epsilon", (approx_eq(v, v + offset)),
		                (true));

		BASIC_R_SUBCASE("distance > epsilon", (approx_eq(v, v + offset * 2.0F)),
		                (false));
	}
}
