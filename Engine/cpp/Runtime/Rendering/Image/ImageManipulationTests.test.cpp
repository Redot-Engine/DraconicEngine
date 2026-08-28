// get/set, clear/fill, flips, format conversion, channel helpers. Mirrors the
// Sedulous assertions (Test.Assert -> CHECK; pixel.R/G/B/A -> .r/.g/.b/.a;
// ConvertFormat returns Image directly so no `.Value`; Sedulous Color32.Lime
// (0,255,0) maps to our Color32::Green).
#include <doctest_with_main.h>
import core;
import image;

using namespace draco;
using namespace draco::image;

TEST_CASE("image.manip: creation")
{
    const Image image(64, 64, PixelFormat::RGBA8);
    CHECK(image.width() == 64u);
    CHECK(image.height() == 64u);
    CHECK(image.format() == PixelFormat::RGBA8);
    CHECK(image.pixelCount() == 64u * 64u);
    CHECK(image.dataSize() == 64u * 64u * 4u);
}

TEST_CASE("image.manip: creation with data")
{
    u8 data[4 * 4] = {}; // 2x2 RGBA
    data[0] = 255; data[1] = 0; data[2] = 0; data[3] = 255;   // pixel 0 = red
    data[4] = 0; data[5] = 255; data[6] = 0; data[7] = 255;   // pixel 1 = green

    const Image image(2, 2, PixelFormat::RGBA8, std::span<const u8>(data, 16));
    const Color32 pixel0 = image.getPixel(0, 0);
    CHECK((pixel0.r == 255 && pixel0.g == 0 && pixel0.b == 0 && pixel0.a == 255));
    const Color32 pixel1 = image.getPixel(1, 0);
    CHECK((pixel1.r == 0 && pixel1.g == 255 && pixel1.b == 0 && pixel1.a == 255));
}

TEST_CASE("image.manip: copy")
{
    Image original(4, 4, PixelFormat::RGBA8);
    original.setPixel(0, 0, Color32::Red);
    original.setPixel(1, 1, Color32::Green); // Sedulous Color32.Lime

    const Image copy(original);
    CHECK(copy.width() == original.width());
    CHECK(copy.height() == original.height());
    CHECK(copy.format() == original.format());

    const Color32 pixel00 = copy.getPixel(0, 0);
    CHECK((pixel00.r == 255 && pixel00.g == 0 && pixel00.b == 0));
    const Color32 pixel11 = copy.getPixel(1, 1);
    CHECK((pixel11.r == 0 && pixel11.g == 255 && pixel11.b == 0));
}

TEST_CASE("image.manip: get/set pixel")
{
    Image image(8, 8, PixelFormat::RGBA8);
    image.setPixel(0, 0, Color32::Red);
    image.setPixel(1, 0, Color32::Green); // Lime
    image.setPixel(2, 0, Color32::Blue);
    image.setPixel(3, 0, Color32::White);
    image.setPixel(4, 0, Color32::Black);
    image.setPixel(5, 0, Color32(128, 64, 32, 200));

    const Color32 red = image.getPixel(0, 0);
    CHECK((red.r == 255 && red.g == 0 && red.b == 0 && red.a == 255));
    const Color32 green = image.getPixel(1, 0);
    CHECK((green.r == 0 && green.g == 255 && green.b == 0 && green.a == 255));
    const Color32 blue = image.getPixel(2, 0);
    CHECK((blue.r == 0 && blue.g == 0 && blue.b == 255 && blue.a == 255));
    const Color32 white = image.getPixel(3, 0);
    CHECK((white.r == 255 && white.g == 255 && white.b == 255 && white.a == 255));
    const Color32 black = image.getPixel(4, 0);
    CHECK((black.r == 0 && black.g == 0 && black.b == 0 && black.a == 255));
    const Color32 custom = image.getPixel(5, 0);
    CHECK((custom.r == 128 && custom.g == 64 && custom.b == 32 && custom.a == 200));
}

TEST_CASE("image.manip: get pixel out of bounds returns black")
{
    const Image image(4, 4, PixelFormat::RGBA8);
    CHECK(image.getPixel(10, 10) == Color32::Black);
}

TEST_CASE("image.manip: set pixel out of bounds is a no-op")
{
    Image image(4, 4, PixelFormat::RGBA8);
    image.setPixel(100, 100, Color32::Red); // must not crash/corrupt

    image.setPixel(0, 0, Color32::Green); // Lime
    const Color32 pixel = image.getPixel(0, 0);
    CHECK((pixel.r == 0 && pixel.g == 255 && pixel.b == 0));
}

TEST_CASE("image.manip: clear with color")
{
    Image image(4, 4, PixelFormat::RGBA8);
    image.setPixel(0, 0, Color32::Red);
    image.setPixel(1, 1, Color32::Green);

    image.clear(Color32::Blue);
    for (u32 y = 0; y < 4; ++y)
        for (u32 x = 0; x < 4; ++x)
        {
            const Color32 pixel = image.getPixel(x, y);
            CHECK((pixel.r == 0 && pixel.g == 0 && pixel.b == 255 && pixel.a == 255));
        }
}

TEST_CASE("image.manip: fill color")
{
    Image image(4, 4, PixelFormat::RGBA8);
    image.fillColor(Color32(100, 150, 200, 250));
    for (u32 y = 0; y < 4; ++y)
        for (u32 x = 0; x < 4; ++x)
        {
            const Color32 pixel = image.getPixel(x, y);
            CHECK((pixel.r == 100 && pixel.g == 150 && pixel.b == 200 && pixel.a == 250));
        }
}

TEST_CASE("image.manip: flip vertical")
{
    Image image(2, 4, PixelFormat::RGBA8);
    image.setPixel(0, 0, Color32::Red);
    image.setPixel(1, 0, Color32::Red);
    image.setPixel(0, 3, Color32::Blue);
    image.setPixel(1, 3, Color32::Blue);

    image.flipVertical();

    const Color32 topLeft = image.getPixel(0, 0);
    CHECK((topLeft.b == 255 && topLeft.r == 0));
    const Color32 bottomLeft = image.getPixel(0, 3);
    CHECK((bottomLeft.r == 255 && bottomLeft.b == 0));
}

TEST_CASE("image.manip: flip horizontal")
{
    Image image(4, 2, PixelFormat::RGBA8);
    image.setPixel(0, 0, Color32::Red);
    image.setPixel(0, 1, Color32::Red);
    image.setPixel(3, 0, Color32::Blue);
    image.setPixel(3, 1, Color32::Blue);

    image.flipHorizontal();

    const Color32 left = image.getPixel(0, 0);
    CHECK((left.b == 255 && left.r == 0));
    const Color32 right = image.getPixel(3, 0);
    CHECK((right.r == 255 && right.b == 0));
}

TEST_CASE("image.manip: convert format RGBA8 -> RGB8")
{
    Image rgba(4, 4, PixelFormat::RGBA8);
    rgba.setPixel(0, 0, Color32::Red);
    rgba.setPixel(1, 0, Color32::Green);
    rgba.setPixel(2, 0, Color32::Blue);
    rgba.setPixel(3, 0, Color32(100, 150, 200, 255));

    const Image rgb = rgba.convertFormat(PixelFormat::RGB8);
    CHECK(rgb.format() == PixelFormat::RGB8);
    CHECK((rgb.width() == 4u && rgb.height() == 4u));

    const Color32 red = rgb.getPixel(0, 0);
    CHECK((red.r == 255 && red.g == 0 && red.b == 0));
    const Color32 custom = rgb.getPixel(3, 0);
    CHECK((custom.r == 100 && custom.g == 150 && custom.b == 200));
}

TEST_CASE("image.manip: convert to same format")
{
    Image original(4, 4, PixelFormat::RGBA8);
    original.setPixel(0, 0, Color32::Red);

    const Image converted = original.convertFormat(PixelFormat::RGBA8);
    CHECK(converted.format() == PixelFormat::RGBA8);
    const Color32 pixel = converted.getPixel(0, 0);
    CHECK((pixel.r == 255 && pixel.g == 0 && pixel.b == 0));
}

TEST_CASE("image.manip: HasAlpha")
{
    CHECK(Image(4, 4, PixelFormat::RGBA8).hasAlpha());
    CHECK_FALSE(Image(4, 4, PixelFormat::RGB8).hasAlpha());
    CHECK_FALSE(Image(4, 4, PixelFormat::R8).hasAlpha());
    CHECK(Image(4, 4, PixelFormat::BGRA8).hasAlpha());
}

TEST_CASE("image.manip: GetChannelCount")
{
    CHECK(Image(4, 4, PixelFormat::R8).getChannelCount() == 1);
    CHECK(Image(4, 4, PixelFormat::RG8).getChannelCount() == 2);
    CHECK(Image(4, 4, PixelFormat::RGB8).getChannelCount() == 3);
    CHECK(Image(4, 4, PixelFormat::RGBA8).getChannelCount() == 4);
}

TEST_CASE("image.manip: GetBytesPerPixel")
{
    CHECK(Image::getBytesPerPixel(PixelFormat::R8) == 1);
    CHECK(Image::getBytesPerPixel(PixelFormat::RG8) == 2);
    CHECK(Image::getBytesPerPixel(PixelFormat::RGB8) == 3);
    CHECK(Image::getBytesPerPixel(PixelFormat::RGBA8) == 4);
    CHECK(Image::getBytesPerPixel(PixelFormat::BGR8) == 3);
    CHECK(Image::getBytesPerPixel(PixelFormat::BGRA8) == 4);
    CHECK(Image::getBytesPerPixel(PixelFormat::R32F) == 4);
    CHECK(Image::getBytesPerPixel(PixelFormat::RGBA32F) == 16);
}

TEST_CASE("image.manip: RGB8 format preserves RGB, alpha 255")
{
    Image image(4, 4, PixelFormat::RGB8);
    image.setPixel(0, 0, Color32(100, 150, 200, 255));
    const Color32 pixel = image.getPixel(0, 0);
    CHECK((pixel.r == 100 && pixel.g == 150 && pixel.b == 200 && pixel.a == 255));
}

TEST_CASE("image.manip: BGR8 format swaps channels correctly")
{
    Image image(4, 4, PixelFormat::BGR8);
    image.setPixel(0, 0, Color32(100, 150, 200, 255));
    const Color32 pixel = image.getPixel(0, 0);
    CHECK((pixel.r == 100 && pixel.g == 150 && pixel.b == 200));
}

TEST_CASE("image.manip: BGRA8 format")
{
    Image image(4, 4, PixelFormat::BGRA8);
    image.setPixel(0, 0, Color32(100, 150, 200, 128));
    const Color32 pixel = image.getPixel(0, 0);
    CHECK((pixel.r == 100 && pixel.g == 150 && pixel.b == 200 && pixel.a == 128));
}

TEST_CASE("image.manip: R8 format averages to grayscale")
{
    Image image(4, 4, PixelFormat::R8);
    image.setPixel(0, 0, Color32(90, 120, 150, 255)); // average = 120
    const Color32 pixel = image.getPixel(0, 0);
    CHECK(pixel.r == pixel.g);
    CHECK(pixel.g == pixel.b);
    CHECK(pixel.a == 255);
}
