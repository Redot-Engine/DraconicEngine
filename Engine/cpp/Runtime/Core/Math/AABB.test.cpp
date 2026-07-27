#include <doctest_with_main.h>

import core.stdtypes;
import core.math.types;
import core.math.aabb;

using namespace draco;
using namespace draco::math;

TEST_SUITE("math::AABB")
{

    TEST_CASE("AABB: empty() initialization behaves correctly on first expand")
    {
        constexpr AABB box = AABB::empty();

        // Assert bounding box properties are correctly inverted initially
        CHECK(box.min.x > box.max.x);
        CHECK(box.min.y > box.max.y);
        CHECK(box.min.z > box.max.z);

        AABB mutableBox = box;
        mutableBox.expand(Vector3{1.0f, 2.0f, 3.0f});

        CHECK(mutableBox.min.x == 1.0f);
        CHECK(mutableBox.max.x == 1.0f);
        CHECK(mutableBox.min.y == 2.0f);
        CHECK(mutableBox.max.y == 2.0f);
        CHECK(mutableBox.min.z == 3.0f);
        CHECK(mutableBox.max.z == 3.0f);
    }

    TEST_CASE("AABB: expand properly handles multiple points")
    {
        AABB box = AABB::empty();
        box.expand(Vector3{-1.0f, -2.0f, -3.0f});
        box.expand(Vector3{4.0f, 5.0f, 6.0f});
        box.expand(Vector3{0.0f, 0.0f, 0.0f});

        CHECK(box.min.x == -1.0f);
        CHECK(box.min.y == -2.0f);
        CHECK(box.min.z == -3.0f);

        CHECK(box.max.x == 4.0f);
        CHECK(box.max.y == 5.0f);
        CHECK(box.max.z == 6.0f);
    }

    TEST_CASE("AABB: center and size calculations match expected properties")
    {
        constexpr AABB box{Vector3{-2.0f, 0.0f, 1.0f}, Vector3{4.0f, 6.0f, 5.0f}};

        constexpr Vector3 c = box.center();
        CHECK(c.x == doctest::Approx(1.0f));
        CHECK(c.y == doctest::Approx(3.0f));
        CHECK(c.z == doctest::Approx(3.0f));

        constexpr Vector3 s = box.size();
        CHECK(s.x == doctest::Approx(6.0f));
        CHECK(s.y == doctest::Approx(6.0f));
        CHECK(s.z == doctest::Approx(4.0f));
    }
}

TEST_SUITE("math::types alignment and behavior validation")
{

    TEST_CASE("Vector2 layout constraints")
    {
        Vector2 v1{1.0f, 2.0f};
        Vector2 v2{3.0f, 4.0f};

        CHECK(sizeof(Vector2) == 8);
        CHECK(alignof(Vector2) == 8);
        CHECK((v1 + v2).x == 4.0f);
        CHECK((v1 + v2).y == 6.0f);
    }

    TEST_CASE("Vector3 layout constraints")
    {
        Vector3 v{3.0f, 4.0f, 0.0f};

        CHECK(sizeof(Vector3) == 16);
        CHECK(alignof(Vector3) == 16);
        CHECK(length(v) == doctest::Approx(5.0f));
    }

    TEST_CASE("Vector4 layout constraints")
    {
        Vector4 v1{1.0f, 2.0f, 3.0f, 4.0f};
        Vector4 v2{2.0f, 2.0f, 2.0f, 2.0f};

        CHECK(sizeof(Vector4) == 16);
        CHECK(alignof(Vector4) == 16);
        CHECK(dot(v1, v2) == doctest::Approx(20.0f));
    }
}
