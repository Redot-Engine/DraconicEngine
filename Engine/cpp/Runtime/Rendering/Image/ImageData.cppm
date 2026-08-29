/// Image data types: ImageColorSpace, OwnedImageData, ImageDataRef.

module;

#include <span>

#include <cstdint>
#include <cstring>
#include <vector>

export module image:image_data;

import core;
import :pixel_format;

using namespace draco;

export namespace draco::image {

/// Color space of the pixel data.
enum class ImageColorSpace : u32 {
    /// sRGB-encoded (photos, UI). GPU decodes sRGB→linear on sample.
    Srgb,
    /// Linear data (normal maps, masks, HDR). Sampled as-is.
    Linear,
};

/// Abstract interface for image data. Implemented by OwnedImageData (owning)
/// and ImageDataRef (non-owning). Allows renderers and other consumers to
/// accept either type through a common base.
class ImageData {
public:
    virtual ~ImageData() = default;
    [[nodiscard]] virtual u32 width()  const = 0;
    [[nodiscard]] virtual u32 height() const = 0;
    [[nodiscard]] virtual PixelFormat format() const = 0;
    [[nodiscard]] virtual std::span<const u8> pixelData() const = 0;
    [[nodiscard]] virtual ImageColorSpace colorSpace() const = 0;
};

/// Owns a CPU-side pixel buffer.
class OwnedImageData : public ImageData {
public:
    OwnedImageData() = default;

    /// Creates from a copy of the provided data.
    OwnedImageData(u32 w, u32 h, PixelFormat fmt, std::span<const u8> data,
                   ImageColorSpace cs = ImageColorSpace::Srgb)
        : m_width(w), m_height(h), m_format(fmt), m_colorSpace(cs)
    {
        m_data.resize(data.size());
        if (data.size() > 0) { std::memcpy(m_data.data(), data.data(), data.size()); }
    }

    /// Takes ownership of data by move.
    OwnedImageData(u32 w, u32 h, PixelFormat fmt, std::vector<u8>&& data,
                   ImageColorSpace cs = ImageColorSpace::Srgb)
        : m_width(w), m_height(h), m_format(fmt), m_colorSpace(cs),
          m_data(static_cast<std::vector<u8>&&>(data)) {}

    [[nodiscard]] u32 width()  const override { return m_width; }
    [[nodiscard]] u32 height() const override { return m_height; }
    [[nodiscard]] PixelFormat format() const override { return m_format; }
    [[nodiscard]] ImageColorSpace colorSpace() const override { return m_colorSpace; }
    [[nodiscard]] std::span<const u8> pixelData() const override { return { m_data.data(), m_data.size() }; }
    [[nodiscard]] std::span<u8> pixelDataMut() { return { m_data.data(), m_data.size() }; }
    [[nodiscard]] u32 dataSize() const { return m_width * m_height * bytesPerPixel(m_format); }

private:
    u32 m_width = 0, m_height = 0;
    PixelFormat m_format = PixelFormat::RGBA8;
    ImageColorSpace m_colorSpace = ImageColorSpace::Srgb;
    std::vector<u8> m_data;
};

/// References external pixel data (non-owning).
/// Caller must ensure data outlives this reference.
class ImageDataRef : public ImageData {
public:
    ImageDataRef() = default;

    /// No pixel data (for GPU-managed textures).
    ImageDataRef(u32 w, u32 h, PixelFormat fmt = PixelFormat::RGBA8,
                 ImageColorSpace cs = ImageColorSpace::Srgb)
        : m_width(w), m_height(h), m_format(fmt), m_colorSpace(cs) {}

    /// Points to external pixel data.
    ImageDataRef(u32 w, u32 h, PixelFormat fmt, const u8* data, usize length,
                 ImageColorSpace cs = ImageColorSpace::Srgb)
        : m_width(w), m_height(h), m_format(fmt), m_colorSpace(cs),
          m_ptr(data), m_length(length) {}

    [[nodiscard]] u32 width()  const override { return m_width; }
    [[nodiscard]] u32 height() const override { return m_height; }
    [[nodiscard]] PixelFormat format() const override { return m_format; }
    [[nodiscard]] ImageColorSpace colorSpace() const override { return m_colorSpace; }
    [[nodiscard]] std::span<const u8> pixelData() const override {
        return m_ptr ? std::span<const u8>(m_ptr, m_length) : std::span<const u8>();
    }

private:
    u32 m_width = 0, m_height = 0;
    PixelFormat m_format = PixelFormat::RGBA8;
    ImageColorSpace m_colorSpace = ImageColorSpace::Srgb;
    const u8* m_ptr = nullptr;
    usize m_length = 0;
};

} // namespace draco::image
