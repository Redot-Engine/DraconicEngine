// Draconic::Texture - :format_utils partition
//
// Maps an image PixelFormat to the RHI TextureFormat, honoring the source

module;

export module texture:format_utils;

import core;
import rhi;
import image;

using namespace draco;

export namespace draco::texture
{
    namespace rhi = draco::rhi;
    namespace image = draco::image;

    class TextureFormatUtils
    {
    public:
        // Image PixelFormat -> RHI TextureFormat. When the data is sRGB and the
        // format has an sRGB variant (8-bit RGB/RGBA/BGR/BGRA), the sRGB GPU
        // format is returned so hardware decodes sRGB->linear on sample. Float
        // and 1/2-channel formats pass through. 3-channel maps to RGBA (GPUs
        // don't support 3-channel).
        [[nodiscard]] static rhi::TextureFormat convert(image::PixelFormat format, image::ImageColorSpace colorSpace)
        {
            if (colorSpace == image::ImageColorSpace::Srgb)
            {
                switch (format)
                {
                    case image::PixelFormat::RGB8:
                    case image::PixelFormat::RGBA8: return rhi::TextureFormat::RGBA8UnormSrgb;
                    case image::PixelFormat::BGR8:
                    case image::PixelFormat::BGRA8: return rhi::TextureFormat::BGRA8UnormSrgb;
                    default: break;
                }
            }

            switch (format)
            {
                case image::PixelFormat::R8:      return rhi::TextureFormat::R8Unorm;
                case image::PixelFormat::RG8:     return rhi::TextureFormat::RG8Unorm;
                case image::PixelFormat::RGB8:    return rhi::TextureFormat::RGBA8Unorm;
                case image::PixelFormat::RGBA8:   return rhi::TextureFormat::RGBA8Unorm;
                case image::PixelFormat::BGR8:    return rhi::TextureFormat::BGRA8Unorm;
                case image::PixelFormat::BGRA8:   return rhi::TextureFormat::BGRA8Unorm;
                case image::PixelFormat::R16F:    return rhi::TextureFormat::R16Float;
                case image::PixelFormat::RG16F:   return rhi::TextureFormat::RG16Float;
                case image::PixelFormat::RGB16F:  return rhi::TextureFormat::RGBA16Float;
                case image::PixelFormat::RGBA16F: return rhi::TextureFormat::RGBA16Float;
                case image::PixelFormat::R32F:    return rhi::TextureFormat::R32Float;
                case image::PixelFormat::RG32F:   return rhi::TextureFormat::RG32Float;
                case image::PixelFormat::RGB32F:  return rhi::TextureFormat::RGBA32Float;
                case image::PixelFormat::RGBA32F: return rhi::TextureFormat::RGBA32Float;
                default:                        return rhi::TextureFormat::RGBA8Unorm;
            }
        }
    };
}
