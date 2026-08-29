// generators + CalculateNormalFromHeight. Mirrors the Sedulous assertions
// (Test.Assert -> CHECK; pixel.R/G/B/A -> .r/.g/.b/.a; math::Vector3.X/Y/Z -> .x/.y/.z).
#include <doctest_with_main.h>
import core;
import image;

using namespace draco;
using namespace draco::image;

namespace
{
    // Neutral up-normal (0,0,1) encoded as (128,128,255).
    constexpr u8 NeutralX = 128;
    constexpr u8 NeutralY = 128;
    constexpr u8 NeutralZ = 255;
}

TEST_CASE("image.normalmap: flat is uniform neutral")
{
    const Image image = Image::createFlatNormalMap(32, 32);
    CHECK(image.width() == 32u);
    CHECK(image.height() == 32u);
    CHECK(image.format() == PixelFormat::RGBA8);

    for (u32 y = 0; y < 32; ++y)
        for (u32 x = 0; x < 32; ++x)
        {
            const Color32 pixel = image.getPixel(x, y);
            CHECK(pixel.r == NeutralX);
            CHECK(pixel.g == NeutralY);
            CHECK(pixel.b == NeutralZ);
            CHECK(pixel.a == 255);
        }
}

TEST_CASE("image.normalmap: flat honors format")
{
    const Image image = Image::createFlatNormalMap(16, 16, PixelFormat::RGB8);
    CHECK(image.format() == PixelFormat::RGB8);
    const Color32 pixel = image.getPixel(8, 8);
    CHECK((pixel.r == NeutralX && pixel.g == NeutralY && pixel.b == NeutralZ));
}

TEST_CASE("image.normalmap: wave varies in X and Y, Z stays up")
{
    const Image image = Image::createWaveNormalMap(64, 64, 4.0f, 4.0f, 0.3f);
    CHECK(image.width() == 64u);
    CHECK(image.height() == 64u);

    bool hasXVariation = false;
    bool hasYVariation = false;
    for (u32 y = 0; y < 64; ++y)
        for (u32 x = 0; x < 64; ++x)
        {
            const Color32 pixel = image.getPixel(x, y);
            if (abs(static_cast<i32>(pixel.r) - static_cast<i32>(NeutralX)) > 5) hasXVariation = true;
            if (abs(static_cast<i32>(pixel.g) - static_cast<i32>(NeutralY)) > 5) hasYVariation = true;
            CHECK(pixel.b >= 128);
        }
    CHECK(hasXVariation);
    CHECK(hasYVariation);
}

TEST_CASE("image.normalmap: brick has variation, Z stays up")
{
    const Image image = Image::createBrickNormalMap(64, 64, 4, 2, 0.3f);
    CHECK(image.width() == 64u);
    CHECK(image.height() == 64u);

    bool hasVariation = false;
    for (u32 y = 0; y < 64; ++y)
        for (u32 x = 0; x < 64; ++x)
        {
            const Color32 pixel = image.getPixel(x, y);
            if (pixel.r != NeutralX || pixel.g != NeutralY || pixel.b != NeutralZ) hasVariation = true;
            CHECK(pixel.b >= 128);
        }
    CHECK(hasVariation);
}

TEST_CASE("image.normalmap: circular bump has variation, Z stays up")
{
    const Image image = Image::createCircularBumpNormalMap(64, 64, 0.5f, 2.0f);
    CHECK(image.width() == 64u);
    CHECK(image.height() == 64u);

    bool hasVariation = false;
    for (u32 y = 0; y < 64 && !hasVariation; ++y)
        for (u32 x = 0; x < 64 && !hasVariation; ++x)
        {
            const Color32 pixel = image.getPixel(x, y);
            if (pixel.r != NeutralX || pixel.g != NeutralY) hasVariation = true;
        }
    CHECK(hasVariation);

    for (u32 y = 0; y < 64; ++y)
        for (u32 x = 0; x < 64; ++x)
            CHECK(image.getPixel(x, y).b >= 128);
}

TEST_CASE("image.normalmap: noise varies across most pixels, Z stays up")
{
    const Image image = Image::createNoiseNormalMap(64, 64, 0.1f, 0.2f, 12345);
    CHECK(image.width() == 64u);
    CHECK(image.height() == 64u);

    bool hasVariation = false;
    i32 variationCount = 0;
    for (u32 y = 0; y < 64; ++y)
        for (u32 x = 0; x < 64; ++x)
        {
            const Color32 pixel = image.getPixel(x, y);
            if (pixel.r != NeutralX || pixel.g != NeutralY) { hasVariation = true; ++variationCount; }
            CHECK(pixel.b >= 128);
        }
    CHECK(hasVariation);
    CHECK(variationCount > 64 * 64 / 2);
}

TEST_CASE("image.normalmap: noise is deterministic for a seed")
{
    const Image image1 = Image::createNoiseNormalMap(32, 32, 0.1f, 0.2f, 42);
    const Image image2 = Image::createNoiseNormalMap(32, 32, 0.1f, 0.2f, 42);
    bool identical = true;
    for (u32 y = 0; y < 32; ++y)
        for (u32 x = 0; x < 32; ++x)
        {
            const Color32 p1 = image1.getPixel(x, y);
            const Color32 p2 = image2.getPixel(x, y);
            if (!(p1.r == p2.r && p1.g == p2.g && p1.b == p2.b)) identical = false;
        }
    CHECK(identical);
}

TEST_CASE("image.normalmap: different seeds differ")
{
    const Image image1 = Image::createNoiseNormalMap(32, 32, 0.1f, 0.2f, 100);
    const Image image2 = Image::createNoiseNormalMap(32, 32, 0.1f, 0.2f, 200);
    bool hasDifference = false;
    for (u32 y = 0; y < 32 && !hasDifference; ++y)
        for (u32 x = 0; x < 32 && !hasDifference; ++x)
        {
            const Color32 p1 = image1.getPixel(x, y);
            const Color32 p2 = image2.getPixel(x, y);
            if (p1.r != p2.r || p1.g != p2.g || p1.b != p2.b) hasDifference = true;
        }
    CHECK(hasDifference);
}

TEST_CASE("image.normalmap: test pattern varies in X and Y, Z stays up")
{
    const Image image = Image::createTestPatternNormalMap(64, 64);
    CHECK(image.width() == 64u);
    CHECK(image.height() == 64u);

    bool hasXVariation = false;
    bool hasYVariation = false;
    for (u32 y = 0; y < 64; ++y)
        for (u32 x = 0; x < 64; ++x)
        {
            const Color32 pixel = image.getPixel(x, y);
            if (pixel.r != NeutralX) hasXVariation = true;
            if (pixel.g != NeutralY) hasYVariation = true;
            CHECK(pixel.b >= 128);
        }
    CHECK(hasXVariation);
    CHECK(hasYVariation);
}

TEST_CASE("image.normalmap: CalculateNormalFromHeight")
{
    const math::Vector3 flatNormal = Image::calculateNormalFromHeight(0.5f, 0.5f, 0.5f, 0.5f);
    CHECK(abs(flatNormal.x) < 0.01f);
    CHECK(abs(flatNormal.y) < 0.01f);
    CHECK(flatNormal.z > 0.99f);

    const math::Vector3 rightSlope = Image::calculateNormalFromHeight(0.0f, 1.0f, 0.5f, 0.5f);
    CHECK(rightSlope.x < 0.0f);  // normal points against the slope
    CHECK(rightSlope.z > 0.0f);

    const math::Vector3 downSlope = Image::calculateNormalFromHeight(0.5f, 0.5f, 0.0f, 1.0f);
    CHECK(downSlope.y < 0.0f);
    CHECK(downSlope.z > 0.0f);
}

TEST_CASE("image.normalmap: all generators keep Z positive (B >= 128)")
{
    std::vector<Image> images;
    images.push_back(Image::createFlatNormalMap(16, 16));
    images.push_back(Image::createWaveNormalMap(16, 16));
    images.push_back(Image::createBrickNormalMap(16, 16));
    images.push_back(Image::createCircularBumpNormalMap(16, 16));
    images.push_back(Image::createNoiseNormalMap(16, 16));
    images.push_back(Image::createTestPatternNormalMap(16, 16));

    for (usize i = 0; i < images.size(); ++i)
        for (u32 y = 0; y < 16; ++y)
            for (u32 x = 0; x < 16; ++x)
                CHECK(images[i].getPixel(x, y).b >= 128);
}
