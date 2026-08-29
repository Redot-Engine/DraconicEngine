/// Image - owns a CPU-side pixel buffer with manipulation methods.
/// Implements ImageData so it can be passed to anything accepting the base type.

module;

#include <span>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

export module image:image;

import core;
import :pixel_format;
import :image_data;

using namespace draco;

export namespace draco::image {

// Pixel access uses the engine's packed byte color, core::Color32 (the image
// library previously defined its own duplicate `Color` - unified away).

/// Image that owns a pixel buffer. Inherits ImageData for polymorphic use.
/// Supports pixel access, flips, format conversion, and procedural factories.
class Image : public ImageData {
public:
    Image() = default;

    Image(u32 w, u32 h, PixelFormat fmt, std::span<const u8> srcData = {})
        : m_width(w), m_height(h), m_format(fmt)
    {
        usize needed = dataSize();
        m_data.resize(needed);
        if (srcData.size() >= needed)
            std::memcpy(m_data.data(), srcData.data(), needed);
        else
            clear();
    }

    Image(const Image&) = default;
    Image(Image&&) noexcept = default;
    Image& operator=(const Image&) = default;
    Image& operator=(Image&&) noexcept = default;

    // ---- ImageData interface ----
    [[nodiscard]] u32 width()  const override { return m_width; }
    [[nodiscard]] u32 height() const override { return m_height; }
    [[nodiscard]] PixelFormat format() const override { return m_format; }
    [[nodiscard]] ImageColorSpace colorSpace() const override { return m_colorSpace; }
    [[nodiscard]] std::span<const u8> pixelData() const override { return { m_data.data(), m_data.size() }; }

    // ---- Mutable access ----
    [[nodiscard]] std::span<u8> pixelDataMut() { return { m_data.data(), m_data.size() }; }
    [[nodiscard]] u32 pixelCount() const { return m_width * m_height; }
    [[nodiscard]] usize dataSize() const { return static_cast<usize>(pixelCount()) * bytesPerPixel(m_format); }
    void setColorSpace(ImageColorSpace cs) { m_colorSpace = cs; }

    /// Replace dimensions, format, and pixel data in-place (hot-reload).
    void replaceData(u32 w, u32 h, PixelFormat fmt, std::span<const u8> src) {
        m_width = w; m_height = h; m_format = fmt;
        usize needed = dataSize();
        m_data.resize(needed);
        usize copy = std::min(needed, src.size());
        if (copy > 0) std::memcpy(m_data.data(), src.data(), copy);
        if (copy < needed) std::memset(m_data.data() + copy, 0, needed - copy);
    }

    // ---- Pixel access ----

    void clear() {
        if (draco::image::hasAlpha(m_format))
            fillColor(Color32::Transparent);
        else
            std::memset(m_data.data(), 0, m_data.size());
    }

    /// Clear the image to a specific color.
    void clear(Color32 color) { fillColor(color); }

    /// Bytes per pixel for a format (static helper mirroring Sedulous's API).
    [[nodiscard]] static i32 getBytesPerPixel(PixelFormat format) { return static_cast<i32>(bytesPerPixel(format)); }

    /// Whether this image's format carries an alpha channel.
    [[nodiscard]] bool hasAlpha() const { return draco::image::hasAlpha(m_format); }
    /// Number of channels in this image's pixel format.
    [[nodiscard]] i32 getChannelCount() const { return static_cast<i32>(draco::image::channelCount(m_format)); }

    void fillColor(Color32 c) {
        u32 bpp = bytesPerPixel(m_format);
        for (usize i = 0; i < m_data.size(); i += bpp) {
            switch (m_format) {
            case PixelFormat::R8:
                m_data[i] = static_cast<u8>((c.r + c.g + c.b) / 3);
                break;
            case PixelFormat::RG8:
                m_data[i] = c.r; m_data[i+1] = c.g;
                break;
            case PixelFormat::RGB8:
                m_data[i] = c.r; m_data[i+1] = c.g; m_data[i+2] = c.b;
                break;
            case PixelFormat::RGBA8:
                m_data[i] = c.r; m_data[i+1] = c.g; m_data[i+2] = c.b; m_data[i+3] = c.a;
                break;
            case PixelFormat::BGR8:
                m_data[i] = c.b; m_data[i+1] = c.g; m_data[i+2] = c.r;
                break;
            case PixelFormat::BGRA8:
                m_data[i] = c.b; m_data[i+1] = c.g; m_data[i+2] = c.r; m_data[i+3] = c.a;
                break;
            default: break;
            }
        }
    }

    [[nodiscard]] Color32 getPixel(u32 x, u32 y) const {
        if (x >= m_width || y >= m_height) return Color32::Black;
        usize off = pixelOffset(x, y);
        switch (m_format) {
        case PixelFormat::R8:    { u8 g = m_data[off]; return {g,g,g,255}; }
        case PixelFormat::RGB8:  return {m_data[off], m_data[off+1], m_data[off+2], 255};
        case PixelFormat::RGBA8: return {m_data[off], m_data[off+1], m_data[off+2], m_data[off+3]};
        case PixelFormat::BGR8:  return {m_data[off+2], m_data[off+1], m_data[off], 255};
        case PixelFormat::BGRA8: return {m_data[off+2], m_data[off+1], m_data[off], m_data[off+3]};
        default: return Color32::Black;
        }
    }

    void setPixel(u32 x, u32 y, Color32 c) {
        if (x >= m_width || y >= m_height) return;
        usize off = pixelOffset(x, y);
        switch (m_format) {
        case PixelFormat::R8:    m_data[off] = static_cast<u8>((c.r+c.g+c.b)/3); break;
        case PixelFormat::RGB8:  m_data[off]=c.r; m_data[off+1]=c.g; m_data[off+2]=c.b; break;
        case PixelFormat::RGBA8: m_data[off]=c.r; m_data[off+1]=c.g; m_data[off+2]=c.b; m_data[off+3]=c.a; break;
        case PixelFormat::BGR8:  m_data[off]=c.b; m_data[off+1]=c.g; m_data[off+2]=c.r; break;
        case PixelFormat::BGRA8: m_data[off]=c.b; m_data[off+1]=c.g; m_data[off+2]=c.r; m_data[off+3]=c.a; break;
        default: break;
        }
    }

    // ---- Flips ----

    void flipVertical() {
        u32 rowSize = m_width * bytesPerPixel(m_format);
        std::vector<u8> tmp(rowSize);
        for (u32 y = 0; y < m_height / 2; ++y) {
            u8* top = m_data.data() + y * rowSize;
            u8* bot = m_data.data() + (m_height - 1 - y) * rowSize;
            std::memcpy(tmp.data(), top, rowSize);
            std::memcpy(top, bot, rowSize);
            std::memcpy(bot, tmp.data(), rowSize);
        }
    }

    void flipHorizontal() {
        u32 bpp = bytesPerPixel(m_format);
        std::vector<u8> tmp(bpp);
        for (u32 y = 0; y < m_height; ++y) {
            for (u32 x = 0; x < m_width / 2; ++x) {
                u8* left  = m_data.data() + pixelOffset(x, y);
                u8* right = m_data.data() + pixelOffset(m_width - 1 - x, y);
                std::memcpy(tmp.data(), left, bpp);
                std::memcpy(left, right, bpp);
                std::memcpy(right, tmp.data(), bpp);
            }
        }
    }

    // ---- Format conversion ----

    [[nodiscard]] Image convertFormat(PixelFormat newFmt) const {
        if (newFmt == m_format) return *this;
        Image out(m_width, m_height, newFmt);
        for (u32 y = 0; y < m_height; ++y)
            for (u32 x = 0; x < m_width; ++x)
                out.setPixel(x, y, getPixel(x, y));
        return out;
    }

    // ---- Factories ----

    static Image createSolidColor(u32 w, u32 h, Color32 c, PixelFormat fmt = PixelFormat::RGBA8) {
        Image img(w, h, fmt);
        img.fillColor(c);
        return img;
    }

    static Image createCheckerboard(u32 size = 256, Color32 c1 = Color32::White, Color32 c2 = Color32::Black,
                                     u32 checkSize = 32, PixelFormat fmt = PixelFormat::RGBA8) {
        Image img(size, size, fmt);
        for (u32 y = 0; y < size; ++y)
            for (u32 x = 0; x < size; ++x)
                img.setPixel(x, y, ((x/checkSize + y/checkSize) % 2 == 0) ? c1 : c2);
        return img;
    }

    static Image createGradient(u32 w, u32 h, Color32 top, Color32 bottom, PixelFormat fmt = PixelFormat::RGBA8) {
        Image img(w, h, fmt);
        for (u32 y = 0; y < h; ++y) {
            f32 t = static_cast<f32>(y) / static_cast<f32>(h > 1 ? h - 1 : 1);
            Color32 c{
                static_cast<u8>(top.r + t * (bottom.r - top.r)),
                static_cast<u8>(top.g + t * (bottom.g - top.g)),
                static_cast<u8>(top.b + t * (bottom.b - top.b)),
                static_cast<u8>(top.a + t * (bottom.a - top.a)),
            };
            for (u32 x = 0; x < w; ++x) img.setPixel(x, y, c);
        }
        return img;
    }

    // ---- Normal-map factories ----
    // (n*0.5+0.5)*255; the neutral up-normal (0,0,1) -> (128,128,255).

    static Image createFlatNormalMap(u32 width = 256, u32 height = 256, PixelFormat format = PixelFormat::RGBA8) {
        Image image(width, height, format);
        image.fillColor(Color32(128, 128, 255, 255)); // (0,0,1)
        return image;
    }

    static Image createWaveNormalMap(u32 width = 256, u32 height = 256,
                                     f32 waveFrequencyX = 8.0f, f32 waveFrequencyY = 6.0f,
                                     f32 amplitude = 0.3f, PixelFormat format = PixelFormat::RGBA8) {
        Image image(width, height, format);
        for (u32 y = 0; y < height; ++y) {
            for (u32 x = 0; x < width; ++x) {
                const f32 fx = static_cast<f32>(x) / static_cast<f32>(width);
                const f32 fy = static_cast<f32>(y) / static_cast<f32>(height);
                const f32 heightValue = math::sin(fx * math::PI * waveFrequencyX) * amplitude + math::sin(fy * math::PI * waveFrequencyY) * amplitude * 0.7f;
                const f32 heightRight = math::sin((fx + 1.0f / static_cast<f32>(width)) * math::PI * waveFrequencyX) * amplitude + math::sin(fy * math::PI * waveFrequencyY) * amplitude * 0.7f;
                const f32 heightDown = math::sin(fx * math::PI * waveFrequencyX) * amplitude + math::sin((fy + 1.0f / static_cast<f32>(height)) * math::PI * waveFrequencyY) * amplitude * 0.7f;
                const f32 dx = heightRight - heightValue;
                const f32 dy = heightDown - heightValue;
                image.setPixel(x, y, encodeNormal(math::normalize(math::Vector3{ -dx * 20.0f, -dy * 20.0f, 1.0f })));
            }
        }
        return image;
    }

    static Image createBrickNormalMap(u32 width = 256, u32 height = 256, u32 bricksX = 8, u32 bricksY = 4,
                                      f32 mortarDepth = 0.3f, PixelFormat format = PixelFormat::RGBA8) {
        Image image(width, height, format);
        const u32 brickWidth = width / bricksX;
        const u32 brickHeight = height / bricksY;
        const u32 mortarWidth = std::max(brickWidth / 16u, 2u);

        for (u32 y = 0; y < height; ++y) {
            for (u32 x = 0; x < width; ++x) {
                const u32 brickY = y / brickHeight;
                u32 adjustedX = x;
                if (brickY % 2 == 1)
                    adjustedX = (x + brickWidth / 2) % width;

                const u32 localX = adjustedX % brickWidth;
                const u32 localY = y % brickHeight;

                const bool isHorizontalMortar = localY < mortarWidth || localY >= (brickHeight - mortarWidth);
                const bool isVerticalMortar = localX < mortarWidth || localX >= (brickWidth - mortarWidth);
                const bool isMortar = isHorizontalMortar || isVerticalMortar;

                math::Vector3 normal;
                if (isMortar) {
                    normal = math::Vector3{ 0.0f, 0.0f, 1.0f - mortarDepth * 2.0f };
                } else {
                    const f32 brickVariation = math::sin(static_cast<f32>(localX) * 0.2f) * math::sin(static_cast<f32>(localY) * 0.15f) * 0.1f;
                    normal = math::Vector3{ 0.0f, 0.0f, 1.0f + brickVariation };
                }
                image.setPixel(x, y, encodeNormal(math::normalize(normal)));
            }
        }
        return image;
    }

    static Image createCircularBumpNormalMap(u32 width = 256, u32 height = 256, f32 bumpHeight = 0.5f,
                                             f32 falloff = 2.0f, PixelFormat format = PixelFormat::RGBA8) {
        Image image(width, height, format);
        const f32 centerX = static_cast<f32>(width) * 0.5f;
        const f32 centerY = static_cast<f32>(height) * 0.5f;
        const f32 maxRadius = static_cast<f32>(std::min(width, height)) * 0.4f;

        for (u32 y = 0; y < height; ++y) {
            for (u32 x = 0; x < width; ++x) {
                const f32 dx = static_cast<f32>(x) - centerX;
                const f32 dy = static_cast<f32>(y) - centerY;
                const f32 distance = sqrt(dx * dx + dy * dy);

                math::Vector3 normal;
                if (distance < maxRadius && distance > 0.001f) {
                    const f32 normalizedDist = distance / maxRadius;
                    const f32 heightDerivative = -falloff * pow(1.0f - normalizedDist, falloff - 1.0f) * bumpHeight * 3.0f / maxRadius;
                    const f32 nx = (dx / distance) * heightDerivative;
                    const f32 ny = (dy / distance) * heightDerivative;
                    normal = math::normalize(math::Vector3{ nx, ny, 1.0f });
                } else {
                    normal = math::Vector3{ 0.0f, 0.0f, 1.0f };
                }
                image.setPixel(x, y, encodeNormal(normal));
            }
        }
        return image;
    }

    static Image createNoiseNormalMap(u32 width = 256, u32 height = 256, f32 scale = 0.1f,
                                      f32 amplitude = 0.2f, i32 seed = 12345, PixelFormat format = PixelFormat::RGBA8) {
        Image image(width, height, format);

        std::vector<f32> heightMap;
        heightMap.resize(static_cast<usize>(width) * height);

        for (u32 y = 0; y < height; ++y) {
            for (u32 x = 0; x < width; ++x) {
                f32 noise = 0.0f;
                f32 freq = scale;
                f32 amp = amplitude;
                for (i32 octave = 0; octave < 4; ++octave) {
                    const f32 fx = static_cast<f32>(x) * freq;
                    const f32 fy = static_cast<f32>(y) * freq;
                    const u32 ix = static_cast<u32>(fx);
                    const u32 iy = static_cast<u32>(fy);
                    const f32 fracX = fx - static_cast<f32>(ix);
                    const f32 fracY = fy - static_cast<f32>(iy);

                    const f32 a = hashToFloat(seed + static_cast<i32>(ix) + static_cast<i32>(iy) * 1000 + octave * 10000);
                    const f32 b = hashToFloat(seed + static_cast<i32>(ix + 1) + static_cast<i32>(iy) * 1000 + octave * 10000);
                    const f32 c = hashToFloat(seed + static_cast<i32>(ix) + static_cast<i32>(iy + 1) * 1000 + octave * 10000);
                    const f32 d = hashToFloat(seed + static_cast<i32>(ix + 1) + static_cast<i32>(iy + 1) * 1000 + octave * 10000);

                    const f32 smoothX = fracX * fracX * (3.0f - 2.0f * fracX);
                    const f32 smoothY = fracY * fracY * (3.0f - 2.0f * fracY);

                    const f32 i1 = math::lerp(a, b, smoothX);
                    const f32 i2 = math::lerp(c, d, smoothX);
                    const f32 value = math::lerp(i1, i2, smoothY);

                    noise += value * amp;
                    freq *= 2.0f;
                    amp *= 0.5f;
                }
                heightMap[static_cast<usize>(y) * width + x] = noise;
            }
        }

        for (u32 y = 0; y < height; ++y) {
            for (u32 x = 0; x < width; ++x) {
                // Edge-clamped neighbours (avoids the u32 underflow latent in the original).
                const u32 xl = (x > 0) ? x - 1 : 0;
                const u32 xr = (x + 1 < width) ? x + 1 : width - 1;
                const u32 yu = (y > 0) ? y - 1 : 0;
                const u32 yd = (y + 1 < height) ? y + 1 : height - 1;
                const f32 heightL = heightMap[static_cast<usize>(y) * width + xl];
                const f32 heightR = heightMap[static_cast<usize>(y) * width + xr];
                const f32 heightU = heightMap[static_cast<usize>(yu) * width + x];
                const f32 heightD = heightMap[static_cast<usize>(yd) * width + x];

                const f32 dx = heightR - heightL;
                const f32 dy = heightD - heightU;
                image.setPixel(x, y, encodeNormal(math::normalize(math::Vector3{ -dx * 8.0f, -dy * 8.0f, 1.0f })));
            }
        }
        return image;
    }

    static Image createTestPatternNormalMap(u32 width = 256, u32 height = 256, PixelFormat format = PixelFormat::RGBA8) {
        Image image(width, height, format);
        for (u32 y = 0; y < height; ++y) {
            for (u32 x = 0; x < width; ++x) {
                const f32 fx = static_cast<f32>(x) / static_cast<f32>(width);
                const f32 fy = static_cast<f32>(y) / static_cast<f32>(height);

                math::Vector3 normal;
                if (fx < 0.5f && fy < 0.5f) {
                    normal = math::Vector3{ 0.0f, 0.0f, 1.0f }; // Top-left: flat.
                } else if (fx >= 0.5f && fy < 0.5f) {
                    const f32 bump = math::sin(fx * math::PI * 16.0f) * 0.5f; // Top-right: X bumps.
                    normal = math::normalize(math::Vector3{ bump, 0.0f, 1.0f });
                } else if (fx < 0.5f && fy >= 0.5f) {
                    const f32 bump = math::sin(fy * math::PI * 16.0f) * 0.5f; // Bottom-left: Y bumps.
                    normal = math::normalize(math::Vector3{ 0.0f, bump, 1.0f });
                } else {
                    const f32 centerX = 0.75f, centerY = 0.75f; // Bottom-right: circular.
                    const f32 dx = fx - centerX;
                    const f32 dy = fy - centerY;
                    const f32 dist = sqrt(dx * dx + dy * dy);
                    if (dist < 0.2f) {
                        const f32 angle = atan2(dy, dx);
                        normal = math::normalize(math::Vector3{ math::cos(angle) * 0.3f, math::sin(angle) * 0.3f, 1.0f });
                    } else {
                        normal = math::Vector3{ 0.0f, 0.0f, 1.0f };
                    }
                }
                image.setPixel(x, y, encodeNormal(normal));
            }
        }
        return image;
    }

    /// Build a normal from neighbouring heights (heightmap -> normal conversion).
    [[nodiscard]] static math::Vector3 calculateNormalFromHeight(f32 heightL, f32 heightR, f32 heightU, f32 heightD, f32 scale = 1.0f) {
        const f32 dx = (heightR - heightL) * scale;
        const f32 dy = (heightD - heightU) * scale;
        return math::normalize(math::Vector3{ -dx, -dy, 1.0f });
    }

private:
    /// Encode a unit normal into a packed RGB color ((n*0.5+0.5)*255), alpha 255.
    [[nodiscard]] static Color32 encodeNormal(math::Vector3 n) {
        return Color32(static_cast<u8>((n.x * 0.5f + 0.5f) * 255.0f),
                       static_cast<u8>((n.y * 0.5f + 0.5f) * 255.0f),
                       static_cast<u8>((n.z * 0.5f + 0.5f) * 255.0f), 255);
    }

    /// LCG-style integer hash -> f32 in [-1, 1] (deterministic; for value noise).
    [[nodiscard]] static f32 hashToFloat(i32 value) {
        u32 hash = static_cast<u32>(value);
        hash = hash * 1103515245u + 12345u;
        hash = (hash >> 16) ^ hash;
        hash = hash * 0x85ebca6bu;
        hash = (hash >> 13) ^ hash;
        return (static_cast<f32>(hash & 0x7FFFFFFFu) / static_cast<f32>(0x7FFFFFFF)) * 2.0f - 1.0f;
    }

    [[nodiscard]] usize pixelOffset(u32 x, u32 y) const {
        return static_cast<usize>(y * m_width + x) * bytesPerPixel(m_format);
    }

    u32 m_width = 0, m_height = 0;
    PixelFormat m_format = PixelFormat::RGBA8;
    ImageColorSpace m_colorSpace = ImageColorSpace::Srgb;
    std::vector<u8> m_data;
};

} // namespace draco::image
