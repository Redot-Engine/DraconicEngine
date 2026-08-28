/// Concrete image loading and saving via stb_image / stb_image_write.
/// No abstract loader/writer - direct stb dependency.
/// Works with draco::image::Image directly.

module;

#include <span>
#include <string_view>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "stb_image.h"
#include "stb_image_write.h"

export module image.io;

import core;
import image;

using namespace draco;

export namespace draco::image::io {

/// File format for saving.
enum class ImageFileFormat : u32 { PNG, JPG, BMP };

/// Load an image from a file path. Returns RGBA8 for LDR, RGBA32F for HDR.
[[nodiscard]] inline Status loadImage(std::u8string_view path, Image& out) {
    const std::string cPath(reinterpret_cast<const char*>(path.data()), path.size());
    int x = 0, y = 0, channels = 0;
    constexpr int desired = 4;

    bool isHDR = stbi_is_hdr(cPath.c_str()) != 0;
    void* data = nullptr;
    if (isHDR)
        data = stbi_loadf(cPath.c_str(), &x, &y, &channels, desired);
    else
        data = stbi_load(cPath.c_str(), &x, &y, &channels, desired);

    if (!data) return ErrorCode::Unknown;

    usize dataSize = isHDR
        ? static_cast<usize>(x) * y * desired * sizeof(float)
        : static_cast<usize>(x) * y * desired;

    PixelFormat fmt = isHDR ? PixelFormat::RGBA32F : PixelFormat::RGBA8;
    out = Image(static_cast<u32>(x), static_cast<u32>(y), fmt,
                std::span<const u8>(static_cast<const u8*>(data), dataSize));
    out.setColorSpace(isHDR ? ImageColorSpace::Linear : ImageColorSpace::Srgb);
    stbi_image_free(data);
    return ErrorCode::Ok;
}

/// Load an image from a memory buffer.
[[nodiscard]] inline Status loadImageFromMemory(std::span<const u8> buffer, Image& out) {
    int x = 0, y = 0, channels = 0;
    constexpr int desired = 4;

    bool isHDR = stbi_is_hdr_from_memory(buffer.data(), static_cast<int>(buffer.size())) != 0;
    void* data = nullptr;
    if (isHDR)
        data = stbi_loadf_from_memory(buffer.data(), static_cast<int>(buffer.size()),
                                      &x, &y, &channels, desired);
    else
        data = stbi_load_from_memory(buffer.data(), static_cast<int>(buffer.size()),
                                     &x, &y, &channels, desired);

    if (!data) return ErrorCode::Unknown;

    usize dataSize = isHDR
        ? static_cast<usize>(x) * y * desired * sizeof(float)
        : static_cast<usize>(x) * y * desired;

    PixelFormat fmt = isHDR ? PixelFormat::RGBA32F : PixelFormat::RGBA8;
    out = Image(static_cast<u32>(x), static_cast<u32>(y), fmt,
                std::span<const u8>(static_cast<const u8*>(data), dataSize));
    out.setColorSpace(isHDR ? ImageColorSpace::Linear : ImageColorSpace::Srgb);
    stbi_image_free(data);
    return ErrorCode::Ok;
}

/// Save an image to a file. Only supports 8-bit formats (R8, RG8, RGB8, RGBA8).
[[nodiscard]] inline Status saveImage(const Image& image, std::u8string_view path,
                                      ImageFileFormat format, i32 jpgQuality = 90) {
    if (image.width() == 0 || image.height() == 0) return ErrorCode::Unknown;

    switch (image.format()) {
    case PixelFormat::R8: case PixelFormat::RG8:
    case PixelFormat::RGB8: case PixelFormat::RGBA8:
        break;
    default: return ErrorCode::Unknown;
    }

    const std::string cPath(reinterpret_cast<const char*>(path.data()), path.size());
    int w = static_cast<int>(image.width());
    int h = static_cast<int>(image.height());
    int ch = static_cast<int>(channelCount(image.format()));
    const void* data = image.pixelData().data();

    int ok = 0;
    switch (format) {
    case ImageFileFormat::PNG: ok = stbi_write_png(cPath.c_str(), w, h, ch, data, w * ch); break;
    case ImageFileFormat::JPG: ok = stbi_write_jpg(cPath.c_str(), w, h, ch, data, jpgQuality); break;
    case ImageFileFormat::BMP: ok = stbi_write_bmp(cPath.c_str(), w, h, ch, data); break;
    }
    return ok != 0 ? ErrorCode::Ok : ErrorCode::Unknown;
}

} // namespace draco::image::io
