#include <doctest_with_main.h>

import core;
import image;
import image.io;

using namespace draco;
using namespace draco::image;

TEST_CASE("image: procedural create + pixel access")
{
    Image img = Image::createSolidColor(4, 4, Color32{ 10, 20, 30, 255 });
    CHECK(img.width() == 4u);
    CHECK(img.height() == 4u);
    CHECK(img.format() == PixelFormat::RGBA8);
    const Color32 p = img.getPixel(1, 1);
    CHECK(p.r == 10);
    CHECK(p.g == 20);
    CHECK(p.b == 30);
}

TEST_CASE("image: save PNG and reload via stb")
{
    Image img = Image::createCheckerboard(64);
    REQUIRE(img.width() == 64u);

    const std::u8string_view path = u8"/tmp/draco_image_roundtrip.png";
    REQUIRE(io::saveImage(img, path, io::ImageFileFormat::PNG).isOk());

    Image loaded;
    REQUIRE(io::loadImage(path, loaded).isOk());
    CHECK(loaded.width() == 64u);
    CHECK(loaded.height() == 64u);
    CHECK(loaded.format() == PixelFormat::RGBA8);   // stb loads as RGBA8
}
