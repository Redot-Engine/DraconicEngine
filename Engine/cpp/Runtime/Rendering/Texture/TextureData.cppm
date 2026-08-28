// Draconic::Texture - :data partition
//
// TextureData: a CPU-side descriptor of pixel data staged for GPU upload (it
// owns no GPU handle - the consumer creates the rhi::Texture from this). Ported
// from Sedulous.Textures/TextureData.bf.

module;

#include <span>

export module texture:data;

import core;
import rhi;
import image;
import :format_utils;

using namespace draco;

export namespace draco::texture
{
    namespace rhi = draco::rhi;
    namespace image = draco::image;

    // Raw texture data for upload to the GPU. Fields are lowercase (descriptor
    // convention, matching RHI descs); the caller provides correctly-formatted
    // pixel data and owns its lifetime.
    struct TextureData
    {
        const u8* pixels = nullptr;          // pixel data (not owned)
        u64 size = 0;                        // total bytes
        u32 width = 0;
        u32 height = 0;
        u32 depthOrArrayLayers = 1;          // depth (3D) or layer count
        u32 mipLevels = 1;                   // data must contain all mips if > 1
        rhi::TextureFormat format = rhi::TextureFormat::RGBA8Unorm;
        rhi::TextureDimension dimension = rhi::TextureDimension::Texture2D;
        u32 bytesPerRow = 0;                 // 0 = auto
        u32 rowsPerImage = 0;                // 0 = auto

        [[nodiscard]] static TextureData create2D(const u8* pixels, u64 size, u32 width, u32 height, rhi::TextureFormat format)
        {
            return TextureData{ pixels, size, width, height, 1, 1, format, rhi::TextureDimension::Texture2D, 0, 0 };
        }

        [[nodiscard]] static TextureData create2DWithMips(const u8* pixels, u64 size, u32 width, u32 height, u32 mipLevels, rhi::TextureFormat format)
        {
            return TextureData{ pixels, size, width, height, 1, mipLevels, format, rhi::TextureDimension::Texture2D, 0, 0 };
        }

        // Cubemap: 6 square faces packed as a 2D array of 6 layers.
        [[nodiscard]] static TextureData createCube(const u8* pixels, u64 size, u32 faceSize, rhi::TextureFormat format)
        {
            return TextureData{ pixels, size, faceSize, faceSize, 6, 1, format, rhi::TextureDimension::Texture2D, 0, 0 };
        }

        [[nodiscard]] static TextureData create2DArray(const u8* pixels, u64 size, u32 width, u32 height, u32 layers, rhi::TextureFormat format)
        {
            return TextureData{ pixels, size, width, height, layers, 1, format, rhi::TextureDimension::Texture2D, 0, 0 };
        }

        // Builds 2D texture data from an image. `colorSpace` selects the sRGB GPU
        // format (hardware sRGB->linear on sample) for color imagery, or a linear
        // format for data textures (normal maps, masks, HDR).
        [[nodiscard]] static TextureData fromImage(const image::Image& image, image::ImageColorSpace colorSpace)
        {
            const std::span<const u8> data = image.pixelData();
            return create2D(data.data(), static_cast<u64>(data.size()), image.width(), image.height(),
                            TextureFormatUtils::convert(image.format(), colorSpace));
        }

        // Bytes per pixel for an RHI format (uncompressed formats; default 4).
        [[nodiscard]] static u32 getBytesPerPixel(rhi::TextureFormat format)
        {
            switch (format)
            {
                case rhi::TextureFormat::R8Unorm: case rhi::TextureFormat::R8Snorm:
                case rhi::TextureFormat::R8Uint:  case rhi::TextureFormat::R8Sint:
                    return 1;
                case rhi::TextureFormat::R16Uint: case rhi::TextureFormat::R16Sint: case rhi::TextureFormat::R16Float:
                case rhi::TextureFormat::RG8Unorm: case rhi::TextureFormat::RG8Snorm:
                case rhi::TextureFormat::RG8Uint:  case rhi::TextureFormat::RG8Sint:
                case rhi::TextureFormat::Depth16Unorm:
                    return 2;
                case rhi::TextureFormat::R32Uint: case rhi::TextureFormat::R32Sint: case rhi::TextureFormat::R32Float:
                case rhi::TextureFormat::RG16Uint: case rhi::TextureFormat::RG16Sint: case rhi::TextureFormat::RG16Float:
                case rhi::TextureFormat::RGBA8Unorm: case rhi::TextureFormat::RGBA8UnormSrgb: case rhi::TextureFormat::RGBA8Snorm:
                case rhi::TextureFormat::RGBA8Uint:  case rhi::TextureFormat::RGBA8Sint:
                case rhi::TextureFormat::BGRA8Unorm: case rhi::TextureFormat::BGRA8UnormSrgb:
                case rhi::TextureFormat::Depth24Plus: case rhi::TextureFormat::Depth24PlusStencil8: case rhi::TextureFormat::Depth32Float:
                    return 4;
                case rhi::TextureFormat::RG32Uint: case rhi::TextureFormat::RG32Sint: case rhi::TextureFormat::RG32Float:
                case rhi::TextureFormat::RGBA16Uint: case rhi::TextureFormat::RGBA16Sint: case rhi::TextureFormat::RGBA16Float:
                    return 8;
                case rhi::TextureFormat::RGBA32Uint: case rhi::TextureFormat::RGBA32Sint: case rhi::TextureFormat::RGBA32Float:
                    return 16;
                case rhi::TextureFormat::Depth32FloatStencil8:
                    return 8;
                default:
                    return 4;
            }
        }

        // Expected byte size of a mip level.
        [[nodiscard]] u64 calculateMipSize(u32 mipLevel) const
        {
            const u32 mipWidth = std::max(1u, width >> mipLevel);
            const u32 mipHeight = std::max(1u, height >> mipLevel);
            const u32 bpp = getBytesPerPixel(format);
            return static_cast<u64>(mipWidth) * mipHeight * depthOrArrayLayers * bpp;
        }
    };
}
