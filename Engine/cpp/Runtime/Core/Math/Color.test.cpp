#include <doctest_with_main.h>
#include <cmath>

import core.stdtypes;
import core.color;

using namespace draco;

TEST_CASE("color: Color32 packs and unpacks 0xRRGGBBAA")
{
    const Color32 c{ 12, 34, 56, 78 };
    CHECK(c.toRGBA8() == 0x0C22384Eu);
    CHECK(Color32::fromRGBA8(0x0C22384Eu) == c);
    CHECK(Color32::fromRGBA8(c.toRGBA8()) == c);   // round-trip
}

TEST_CASE("color: named constants")
{
    CHECK(Color32::White == Color32{ 255, 255, 255, 255 });
    CHECK(Color32::Black == Color32{ 0, 0, 0, 255 });
    CHECK(Color32::Transparent.a == 0);
    CHECK(Color::Red == Color{ 1.0f, 0.0f, 0.0f, 1.0f });
    CHECK(Color::White.toRGBA8() == 0xFFFFFFFFu);
}

TEST_CASE("color: Color <-> Color32 round-trips exactly")
{
    const Color32 c{ 10, 20, 30, 255 };
    CHECK(toColor32(toColor(c)) == c);
    CHECK(toColor32(Color{ 1.0f, 0.0f, 0.5f, 1.0f }) == Color32{ 255, 0, 128, 255 });
}

TEST_CASE("color: sRGB transfer round-trips")
{
    for (f32 v : { 0.0f, 0.25f, 0.5f, 1.0f })
    {
        CHECK(std::abs(linearToSrgb(srgbToLinear(v)) - v) < 1e-4f);
    }
}
