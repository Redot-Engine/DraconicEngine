// Tests for texture: descriptor factories, format conversion, mip sizes.
#include <doctest_with_main.h>
import core;
import rhi;
import image;
import texture;
using namespace draco;
using namespace draco::texture;
namespace rhi = draco::rhi;
namespace image = draco::image;

TEST_CASE("textures.data: Create2D / mips / cube / array")
{
    u8 pixels[16] = {};
    TextureData t = TextureData::create2D(pixels, 16, 2, 2, rhi::TextureFormat::RGBA8Unorm);
    CHECK(t.width == 2u);
    CHECK(t.height == 2u);
    CHECK(t.depthOrArrayLayers == 1u);
    CHECK(t.mipLevels == 1u);
    CHECK(t.dimension == rhi::TextureDimension::Texture2D);
    CHECK(t.format == rhi::TextureFormat::RGBA8Unorm);

    TextureData mips = TextureData::create2DWithMips(pixels, 16, 4, 4, 3, rhi::TextureFormat::RGBA8Unorm);
    CHECK(mips.mipLevels == 3u);

    TextureData cube = TextureData::createCube(pixels, 16, 8, rhi::TextureFormat::RGBA8Unorm);
    CHECK(cube.depthOrArrayLayers == 6u);
    CHECK(cube.width == 8u);
    CHECK(cube.height == 8u);

    TextureData arr = TextureData::create2DArray(pixels, 16, 4, 4, 5, rhi::TextureFormat::RGBA8Unorm);
    CHECK(arr.depthOrArrayLayers == 5u);
}

TEST_CASE("textures.data: bytes per pixel")
{
    CHECK(TextureData::getBytesPerPixel(rhi::TextureFormat::R8Unorm) == 1u);
    CHECK(TextureData::getBytesPerPixel(rhi::TextureFormat::RG8Unorm) == 2u);
    CHECK(TextureData::getBytesPerPixel(rhi::TextureFormat::RGBA8Unorm) == 4u);
    CHECK(TextureData::getBytesPerPixel(rhi::TextureFormat::RGBA8UnormSrgb) == 4u);
    CHECK(TextureData::getBytesPerPixel(rhi::TextureFormat::RGBA16Float) == 8u);
    CHECK(TextureData::getBytesPerPixel(rhi::TextureFormat::RGBA32Float) == 16u);
    CHECK(TextureData::getBytesPerPixel(rhi::TextureFormat::Depth16Unorm) == 2u);
}

TEST_CASE("textures.data: mip size halves")
{
    u8 pixels[1] = {};
    TextureData t = TextureData::create2D(pixels, 0, 8, 8, rhi::TextureFormat::RGBA8Unorm);
    CHECK(t.calculateMipSize(0) == static_cast<u64>(8 * 8 * 4));   // 256
    CHECK(t.calculateMipSize(1) == static_cast<u64>(4 * 4 * 4));   // 64
    CHECK(t.calculateMipSize(3) == static_cast<u64>(1 * 1 * 4));   // clamps to 1x1
}

TEST_CASE("textures.format: PixelFormat -> TextureFormat with color space")
{
    using image::PixelFormat;
    using image::ImageColorSpace;
    // sRGB color imagery selects the sRGB GPU format.
    CHECK(TextureFormatUtils::convert(PixelFormat::RGBA8, ImageColorSpace::Srgb) == rhi::TextureFormat::RGBA8UnormSrgb);
    CHECK(TextureFormatUtils::convert(PixelFormat::RGB8, ImageColorSpace::Srgb) == rhi::TextureFormat::RGBA8UnormSrgb);
    CHECK(TextureFormatUtils::convert(PixelFormat::BGRA8, ImageColorSpace::Srgb) == rhi::TextureFormat::BGRA8UnormSrgb);
    // Linear data textures stay unorm.
    CHECK(TextureFormatUtils::convert(PixelFormat::RGBA8, ImageColorSpace::Linear) == rhi::TextureFormat::RGBA8Unorm);
    CHECK(TextureFormatUtils::convert(PixelFormat::R8, ImageColorSpace::Linear) == rhi::TextureFormat::R8Unorm);
    // 3-channel maps to RGBA; floats pass through (no sRGB variant).
    CHECK(TextureFormatUtils::convert(PixelFormat::RGB8, ImageColorSpace::Linear) == rhi::TextureFormat::RGBA8Unorm);
    CHECK(TextureFormatUtils::convert(PixelFormat::RGB16F, ImageColorSpace::Srgb) == rhi::TextureFormat::RGBA16Float);
    CHECK(TextureFormatUtils::convert(PixelFormat::R32F, ImageColorSpace::Linear) == rhi::TextureFormat::R32Float);
}

TEST_CASE("textures.data: FromImage")
{
    image::Image image(2, 2, image::PixelFormat::RGBA8);
    TextureData t = TextureData::fromImage(image, image::ImageColorSpace::Srgb);
    CHECK(t.width == 2u);
    CHECK(t.height == 2u);
    CHECK(t.format == rhi::TextureFormat::RGBA8UnormSrgb);
    CHECK(t.pixels == image.pixelData().data());
    CHECK(t.size == static_cast<u64>(image.pixelData().size()));
}
