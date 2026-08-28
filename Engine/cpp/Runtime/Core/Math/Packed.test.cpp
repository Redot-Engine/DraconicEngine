#include <doctest_with_main.h>
#include <cmath>

import core.stdtypes;
import core.math;

using namespace draco;
using namespace draco::math;

TEST_CASE("packed: sizes are byte-tight (no SIMD padding)")
{
    CHECK(sizeof(Float2) == 8);
    CHECK(sizeof(Float3) == 12);
    CHECK(sizeof(Float4) == 16);
    CHECK(alignof(Float3) == 4);   // unlike alignas(16) Vector3
}

TEST_CASE("packed: converts to and from the aligned VectorN")
{
    const Float3 f{ 1.0f, 2.0f, 3.0f };
    const Vector3 v = f;                  // Float3 -> Vector3
    CHECK(v.x == 1.0f);
    CHECK(v.y == 2.0f);
    CHECK(v.z == 3.0f);

    const Float3 back = Vector3{ 4.0f, 5.0f, 6.0f };   // Vector3 -> Float3
    CHECK(back.x == 4.0f);
    CHECK(back.y == 5.0f);
    CHECK(back.z == 6.0f);

    const Float2 f2 = Vector2{ 7.0f, 8.0f };
    CHECK(f2.x == 7.0f);
    CHECK(f2.y == 8.0f);
    const Float4 f4 = Vector4{ 1.0f, 2.0f, 3.0f, 4.0f };
    CHECK(f4.w == 4.0f);
}

TEST_CASE("packed: arithmetic operators")
{
    const Float3 a{ 1.0f, 2.0f, 3.0f };
    const Float3 b{ 4.0f, 5.0f, 6.0f };

    const Float3 sum = a + b;
    CHECK(sum.x == 5.0f);
    CHECK(sum.z == 9.0f);

    const Float3 diff = b - a;
    CHECK(diff.x == 3.0f);
    CHECK(diff.y == 3.0f);

    const Float3 scaled = a * 2.0f;
    CHECK(scaled.x == 2.0f);
    CHECK(scaled.z == 6.0f);

    Float3 acc{ 0, 0, 0 };
    acc += a;
    acc += b;
    CHECK(acc.x == 5.0f);
    CHECK(acc.z == 9.0f);

    const Float2 p{ 1.0f, 2.0f };
    const Float2 q{ 3.0f, 5.0f };
    CHECK((q - p).x == 2.0f);
    CHECK((q + p).y == 7.0f);
}

TEST_CASE("packed: vector helpers")
{
    const Float3 x{ 1.0f, 0.0f, 0.0f };
    const Float3 y{ 0.0f, 1.0f, 0.0f };

    CHECK(dot(x, y) == 0.0f);
    CHECK(dot(x, x) == 1.0f);

    const Float3 z = cross(x, y);          // right-handed: x cross y = z
    CHECK(z.x == 0.0f);
    CHECK(z.y == 0.0f);
    CHECK(z.z == 1.0f);

    const Float3 v{ 3.0f, 4.0f, 0.0f };
    CHECK(lengthSquared(v) == 25.0f);
    CHECK(length(v) == doctest::Approx(5.0f));

    const Float3 n = normalize(v);
    CHECK(length(n) == doctest::Approx(1.0f));
    CHECK(n.x == doctest::Approx(0.6f));
    CHECK(n.y == doctest::Approx(0.8f));

    CHECK(normalize(Float3{ 0, 0, 0 }).x == 0.0f);   // zero stays zero
}
