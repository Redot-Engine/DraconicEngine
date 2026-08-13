/// Texture-related enums and the ModelTexture class.

module;
#include <string_view>

#include <cstring>
#include <string>

export module model:model_texture;

import core;

using namespace draco;

export namespace draco::model {

/// Texture wrapping mode.
enum class TextureWrap : u32 {
    Repeat,
    ClampToEdge,
    MirroredRepeat,
};

/// Texture minification filter.
enum class TextureMinFilter : u32 {
    Nearest,
    Linear,
    NearestMipmapNearest,
    LinearMipmapNearest,
    NearestMipmapLinear,
    LinearMipmapLinear,
};

/// Texture magnification filter.
enum class TextureMagFilter : u32 {
    Nearest,
    Linear,
};

/// Texture sampler settings.
struct TextureSampler {
    TextureWrap wrapS = TextureWrap::Repeat;
    TextureWrap wrapT = TextureWrap::Repeat;
    TextureMinFilter minFilter = TextureMinFilter::LinearMipmapLinear;
    TextureMagFilter magFilter = TextureMagFilter::Linear;
};

/// Pixel format for decoded texture data.
enum class TexturePixelFormat : u32 {
    Unknown,
    R8,     // 1 byte per pixel (grayscale)
    RG8,    // 2 bytes per pixel
    RGB8,   // 3 bytes per pixel
    RGBA8,  // 4 bytes per pixel
    BGR8,   // 3 bytes per pixel (BGR order)
    BGRA8,  // 4 bytes per pixel (BGRA order)
};

/// A texture reference in a model.
class ModelTexture {
public:
    ModelTexture() = default;
    ~ModelTexture() {
        delete[] m_data;
    }

    // Non-copyable, movable.
    ModelTexture(const ModelTexture&) = delete;
    ModelTexture& operator=(const ModelTexture&) = delete;
    ModelTexture(ModelTexture&& other) noexcept
        : mimeType(static_cast<std::u8string&&>(other.mimeType)),
          samplerIndex(other.samplerIndex),
          width(other.width), height(other.height),
          pixelFormat(other.pixelFormat),
          m_name(static_cast<std::u8string&&>(other.m_name)),
          m_uri(static_cast<std::u8string&&>(other.m_uri)),
          m_data(other.m_data), m_dataSize(other.m_dataSize) {
        other.m_data = nullptr;
        other.m_dataSize = 0;
    }
    ModelTexture& operator=(ModelTexture&& other) noexcept {
        if (this != &other) {
            delete[] m_data;
            m_name = static_cast<std::u8string&&>(other.m_name);
            m_uri = static_cast<std::u8string&&>(other.m_uri);
            m_data = other.m_data;
            m_dataSize = other.m_dataSize;
            mimeType = static_cast<std::u8string&&>(other.mimeType);
            samplerIndex = other.samplerIndex;
            width = other.width;
            height = other.height;
            pixelFormat = other.pixelFormat;
            other.m_data = nullptr;
            other.m_dataSize = 0;
        }
        return *this;
    }

    // -- Accessors --

    [[nodiscard]] std::u8string_view name() const { return std::u8string_view(m_name.data(), m_name.size()); }
    [[nodiscard]] std::u8string_view uri()  const { return std::u8string_view(m_uri.data(), m_uri.size()); }

    void setName(std::u8string_view n) { m_name = std::u8string(n); }
    void setUri(std::u8string_view u)  { m_uri  = std::u8string(u); }

    /// Set embedded image data (copies).
    void setData(const u8* ptr, usize length) {
        delete[] m_data;
        if (ptr && length > 0) {
            m_data = new u8[length];
            std::memcpy(m_data, ptr, length);
            m_dataSize = static_cast<i32>(length);
        } else {
            m_data = nullptr;
            m_dataSize = 0;
        }
    }

    /// Set embedded image data (takes ownership of raw pointer).
    void setData(u8* ptr, i32 size) {
        delete[] m_data;
        m_data = ptr;
        m_dataSize = size;
    }

    /// Get embedded image data pointer (nullptr if empty).
    [[nodiscard]] const u8* getData() const { return m_data; }

    /// Get embedded image data size.
    [[nodiscard]] i32 getDataSize() const { return m_dataSize; }

    /// Whether texture has embedded data.
    [[nodiscard]] bool hasEmbeddedData() const { return m_data != nullptr && m_dataSize > 0; }

    // -- Public fields --

    /// Image data format (e.g., "image/png", "image/jpeg").
    std::u8string mimeType;

    /// Sampler index (-1 for default sampler).
    i32 samplerIndex = -1;

    /// Width in pixels (0 if not yet loaded).
    i32 width = 0;

    /// Height in pixels (0 if not yet loaded).
    i32 height = 0;

    /// Pixel format of decoded data.
    TexturePixelFormat pixelFormat = TexturePixelFormat::Unknown;

private:
    std::u8string m_name;
    std::u8string m_uri;
    u8* m_data = nullptr;
    i32 m_dataSize = 0;
};

} // namespace draco::model
