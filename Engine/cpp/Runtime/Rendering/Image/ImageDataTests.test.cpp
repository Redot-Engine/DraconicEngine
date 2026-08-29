// NineSlice, ImageAtlasBuilder, PixelFormat. Mirrors the Sedulous assertions so
// the ported lib inherits that suite's coverage. (Test.Assert -> CHECK; Beef
// scope/new -> stack/Array; nullable RectangleI -> const RectI*.)
#include <doctest_with_main.h>
#include <string>
import core;
import image;

using namespace draco;
using namespace draco::image;

// ============================================================ OwnedImageData

TEST_CASE("image.owned: construct from span")
{
    u8 pixels[16];
    for (i32 i = 0; i < 16; ++i) pixels[i] = static_cast<u8>(i);

    OwnedImageData img(2, 2, PixelFormat::RGBA8, std::span<const u8>(pixels, 16));
    CHECK(img.width() == 2u);
    CHECK(img.height() == 2u);
    CHECK(img.format() == PixelFormat::RGBA8);
    CHECK(img.pixelData().size() == 16u);
    CHECK(img.pixelData().data()[0] == 0);
    CHECK(img.pixelData().data()[4] == 4);
}

TEST_CASE("image.owned: construct from array (move)")
{
    std::vector<u8> data;
    data.resize(8);
    data[0] = 255;
    data[7] = 128;
    OwnedImageData img(2, 1, PixelFormat::RGBA8, move(data));
    CHECK(img.width() == 2u);
    CHECK(img.height() == 1u);
    CHECK(img.pixelData().data()[0] == 255);
    CHECK(img.pixelData().data()[7] == 128);
}

TEST_CASE("image.owned: R8 format")
{
    std::vector<u8> data; data.resize(4);
    OwnedImageData img(2, 2, PixelFormat::R8, move(data));
    CHECK(img.format() == PixelFormat::R8);
    CHECK(img.pixelData().size() == 4u);
}

// ==================================================================== NineSlice

TEST_CASE("image.nineslice: construct four values")
{
    const NineSlice ns(4, 6, 8, 10);
    CHECK(ns.left == 4);
    CHECK(ns.top == 6);
    CHECK(ns.right == 8);
    CHECK(ns.bottom == 10);
}

TEST_CASE("image.nineslice: construct uniform")
{
    const NineSlice ns(5.0f);
    CHECK(ns.left == 5);
    CHECK(ns.top == 5);
    CHECK(ns.right == 5);
    CHECK(ns.bottom == 5);
}

TEST_CASE("image.nineslice: construct symmetric")
{
    const NineSlice ns(3, 7);
    CHECK(ns.left == 3);
    CHECK(ns.right == 3);
    CHECK(ns.top == 7);
    CHECK(ns.bottom == 7);
}

TEST_CASE("image.nineslice: horizontal border")
{
    const NineSlice ns(4, 0, 6, 0);
    CHECK(ns.horizontalBorder() == 10);
}

TEST_CASE("image.nineslice: vertical border")
{
    const NineSlice ns(0, 3, 0, 5);
    CHECK(ns.verticalBorder() == 8);
}

TEST_CASE("image.nineslice: IsValid non-zero")
{
    const NineSlice ns(1, 0, 0, 0);
    CHECK(ns.isValid());
}

TEST_CASE("image.nineslice: IsValid all-zero")
{
    const NineSlice ns(0, 0, 0, 0);
    CHECK_FALSE(ns.isValid());
}

// =========================================================== ImageAtlasBuilder

namespace
{
    OwnedImageData makeImage(u32 w, u32 h, u8 fill)
    {
        std::vector<u8> data;
        data.resize(static_cast<usize>(w) * h * 4);
        for (usize i = 0; i < data.size(); ++i) data[i] = fill;
        return OwnedImageData(w, h, PixelFormat::RGBA8, move(data));
    }

    bool overlaps(const RectI& a, const RectI& b)
    {
        return a.x < b.x + b.width && a.x + a.width > b.x
            && a.y < b.y + b.height && a.y + a.height > b.y;
    }

    bool isPow2(u32 v) { return v > 0 && (v & (v - 1)) == 0; }
}

TEST_CASE("image.atlas: empty build")
{
    ImageAtlasBuilder builder;
    CHECK(builder.build());
    REQUIRE(builder.atlas() != nullptr);
    CHECK(builder.atlas()->width() >= 1u);
    CHECK(builder.atlas()->height() >= 1u);
}

TEST_CASE("image.atlas: single image")
{
    const OwnedImageData img = makeImage(32, 32, 128);

    ImageAtlasBuilder builder;
    builder.addImage(u8"test", &img);
    CHECK(builder.build());

    const RectI* region = builder.getRegion(u8"test");
    REQUIRE(region != nullptr);
    CHECK(region->width == 32);
    CHECK(region->height == 32);
}

TEST_CASE("image.atlas: multiple images do not overlap")
{
    const OwnedImageData img1 = makeImage(64, 64, 100);
    const OwnedImageData img2 = makeImage(32, 32, 200);
    const OwnedImageData img3 = makeImage(48, 48, 150);

    ImageAtlasBuilder builder;
    builder.addImage(u8"a", &img1);
    builder.addImage(u8"b", &img2);
    builder.addImage(u8"c", &img3);
    CHECK(builder.build());

    const RectI* r1 = builder.getRegion(u8"a");
    const RectI* r2 = builder.getRegion(u8"b");
    const RectI* r3 = builder.getRegion(u8"c");
    REQUIRE(r1 != nullptr);
    REQUIRE(r2 != nullptr);
    REQUIRE(r3 != nullptr);

    CHECK_FALSE(overlaps(*r1, *r2));
    CHECK_FALSE(overlaps(*r1, *r3));
    CHECK_FALSE(overlaps(*r2, *r3));
}

TEST_CASE("image.atlas: dimensions are powers of two")
{
    const OwnedImageData img = makeImage(100, 100, 0);

    ImageAtlasBuilder builder;
    builder.addImage(u8"big", &img);
    CHECK(builder.build());

    CHECK(isPow2(builder.atlas()->width()));
    CHECK(isPow2(builder.atlas()->height()));
}

TEST_CASE("image.atlas: grows when needed")
{
    ImageAtlasBuilder builder(256, 4096);
    std::vector<OwnedImageData> images;
    images.reserve(20);
    for (i32 i = 0; i < 20; ++i)
        images.push_back(makeImage(64, 64, static_cast<u8>(i)));

    // Names must outlive build(); build them up front.
    for (i32 i = 0; i < 20; ++i)
    {
        std::u8string name = u8"img";
        const std::string digits = std::to_string(i);
        name.append(reinterpret_cast<const char8_t*>(digits.data()), digits.size());
        builder.addImage(name, &images[static_cast<usize>(i)]);
    }

    CHECK(builder.build());
    // 20 x 64x64 = 81920 px; 256x256=65536 too small -> should have grown.
    CHECK((builder.atlas()->width() >= 512u || builder.atlas()->height() >= 512u));
}

TEST_CASE("image.atlas: GetRegion missing")
{
    ImageAtlasBuilder builder;
    builder.build();
    CHECK(builder.getRegion(u8"nonexistent") == nullptr);
}

TEST_CASE("image.atlas: pixel data copied")
{
    const OwnedImageData img = makeImage(4, 4, 42);

    ImageAtlasBuilder builder(256);
    builder.addImage(u8"px", &img);
    CHECK(builder.build());

    const RectI* region = builder.getRegion(u8"px");
    REQUIRE(region != nullptr);
    const ImageData* atlas = builder.atlas();
    const u32 stride = atlas->width() * 4;

    const usize offset = static_cast<usize>(region->y) * stride + static_cast<usize>(region->x) * 4;
    CHECK(atlas->pixelData().data()[offset] == 42);
}

TEST_CASE("image.atlas: large image (bigger than min size)")
{
    const OwnedImageData img = makeImage(300, 300, 77);

    ImageAtlasBuilder builder(256, 4096);
    builder.addImage(u8"large", &img);
    CHECK(builder.build());

    const RectI* region = builder.getRegion(u8"large");
    REQUIRE(region != nullptr);
    CHECK(region->width == 300);
    CHECK(region->height == 300);
    CHECK(builder.atlas()->width() >= 302u); // 300 + padding
}

// =================================================================== PixelFormat

TEST_CASE("image.pixelformat: distinct values")
{
    CHECK(PixelFormat::R8 != PixelFormat::RGBA8);
    CHECK(PixelFormat::RGBA8 != PixelFormat::BGRA8);
}
