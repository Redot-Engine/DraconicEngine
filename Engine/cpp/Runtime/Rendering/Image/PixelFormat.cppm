/// Pixel format enum for CPU-side image data.

export module image:pixel_format;

import core;

using namespace draco;

export namespace draco::image {

enum class PixelFormat : u32 {
    R8, RG8, RGB8, RGBA8,
    R16F, RG16F, RGB16F, RGBA16F,
    R32F, RG32F, RGB32F, RGBA32F,
    BGR8, BGRA8,
};

[[nodiscard]] constexpr u32 bytesPerPixel(PixelFormat f) {
    switch (f) {
    case PixelFormat::R8:      return 1;
    case PixelFormat::RG8:     return 2;
    case PixelFormat::RGB8:    return 3;
    case PixelFormat::BGR8:    return 3;
    case PixelFormat::RGBA8:   return 4;
    case PixelFormat::BGRA8:   return 4;
    case PixelFormat::R16F:    return 2;
    case PixelFormat::RG16F:   return 4;
    case PixelFormat::RGB16F:  return 6;
    case PixelFormat::RGBA16F: return 8;
    case PixelFormat::R32F:    return 4;
    case PixelFormat::RG32F:   return 8;
    case PixelFormat::RGB32F:  return 12;
    case PixelFormat::RGBA32F: return 16;
    }
    return 4;
}

[[nodiscard]] constexpr u32 channelCount(PixelFormat f) {
    switch (f) {
    case PixelFormat::R8:   case PixelFormat::R16F:  case PixelFormat::R32F:  return 1;
    case PixelFormat::RG8:  case PixelFormat::RG16F: case PixelFormat::RG32F: return 2;
    case PixelFormat::RGB8: case PixelFormat::BGR8:
    case PixelFormat::RGB16F: case PixelFormat::RGB32F: return 3;
    case PixelFormat::RGBA8: case PixelFormat::BGRA8:
    case PixelFormat::RGBA16F: case PixelFormat::RGBA32F: return 4;
    }
    return 0;
}

[[nodiscard]] constexpr bool hasAlpha(PixelFormat f) {
    return f == PixelFormat::RGBA8 || f == PixelFormat::BGRA8 ||
           f == PixelFormat::RGBA16F || f == PixelFormat::RGBA32F;
}

} // namespace draco::image
