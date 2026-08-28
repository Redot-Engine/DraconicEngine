#include <doctest_with_main.h>

import core.stdtypes;
import core.math;

using namespace draco;
using namespace draco::math;

TEST_CASE("aabb: empty() is inverted so the first expand() seats both corners")
{
    AABB box = AABB::empty();
    CHECK(box.min.x > box.max.x);          // inverted before any point

    box.expand(Vector3{ 1.0f, 2.0f, 3.0f });
    CHECK(box.min.x == 1.0f);
    CHECK(box.max.x == 1.0f);
    CHECK(box.min.z == 3.0f);
    CHECK(box.max.z == 3.0f);
}

TEST_CASE("aabb: expand grows to contain all points")
{
    AABB box = AABB::empty();
    box.expand(Vector3{ -1.0f, -2.0f, -3.0f });
    box.expand(Vector3{  4.0f,  5.0f,  6.0f });
    box.expand(Vector3{  0.0f,  0.0f,  0.0f });

    CHECK(box.min.x == -1.0f);
    CHECK(box.min.y == -2.0f);
    CHECK(box.min.z == -3.0f);
    CHECK(box.max.x == 4.0f);
    CHECK(box.max.y == 5.0f);
    CHECK(box.max.z == 6.0f);
}

TEST_CASE("aabb: center and size")
{
    const AABB box{ Vector3{ -2.0f, 0.0f, 1.0f }, Vector3{ 4.0f, 6.0f, 5.0f } };

    const Vector3 c = box.center();
    CHECK(c.x == doctest::Approx(1.0f));
    CHECK(c.y == doctest::Approx(3.0f));
    CHECK(c.z == doctest::Approx(3.0f));

    const Vector3 s = box.size();
    CHECK(s.x == doctest::Approx(6.0f));
    CHECK(s.y == doctest::Approx(6.0f));
    CHECK(s.z == doctest::Approx(4.0f));
}
