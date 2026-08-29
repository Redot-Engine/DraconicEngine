/// Texture format enum and query helpers.

export module rhi:texture_format;

import core.stdtypes;

using namespace draco;

export namespace draco::rhi {

/// Pixel formats for textures and render targets. Naming follows
/// WebGPU / Vulkan conventions: component layout + bit-depth + type.
enum class TextureFormat : u32 {
    Undefined = 0,

    // 8-bit per channel.
    R8Unorm, R8Snorm, R8Uint, R8Sint,

    // 16-bit per channel.
    R16Uint, R16Sint, R16Float,
    RG8Unorm, RG8Snorm, RG8Uint, RG8Sint,

    // 32-bit per channel.
    R32Uint, R32Sint, R32Float,
    RG16Uint, RG16Sint, RG16Float,
    RGBA8Unorm, RGBA8UnormSrgb, RGBA8Snorm, RGBA8Uint, RGBA8Sint,
    BGRA8Unorm, BGRA8UnormSrgb,
    RGB10A2Unorm, RGB10A2Uint,
    RG11B10Float, RGB9E5Float,

    // 64-bit per channel.
    RG32Uint, RG32Sint, RG32Float,
    RGBA16Uint, RGBA16Sint, RGBA16Float, RGBA16Unorm, RGBA16Snorm,

    // 128-bit per channel.
    RGBA32Uint, RGBA32Sint, RGBA32Float,

    // Depth / stencil.
    Depth16Unorm, Depth24Plus, Depth24PlusStencil8,
    Depth32Float, Depth32FloatStencil8, Stencil8,

    // BC compressed.
    BC1RGBAUnorm, BC1RGBAUnormSrgb,
    BC2RGBAUnorm, BC2RGBAUnormSrgb,
    BC3RGBAUnorm, BC3RGBAUnormSrgb,
    BC4RUnorm,    BC4RSnorm,
    BC5RGUnorm,   BC5RGSnorm,
    BC6HRGBUfloat, BC6HRGBFloat,
    BC7RGBAUnorm, BC7RGBAUnormSrgb,

    // ASTC compressed.
    ASTC4x4Unorm,  ASTC4x4UnormSrgb,
    ASTC5x5Unorm,  ASTC5x5UnormSrgb,
    ASTC6x6Unorm,  ASTC6x6UnormSrgb,
    ASTC8x8Unorm,  ASTC8x8UnormSrgb,
};

/// True for Depth16Unorm, Depth24Plus, Depth32Float and their stencil variants.
[[nodiscard]] constexpr bool isDepthFormat(TextureFormat f) {
    return f >= TextureFormat::Depth16Unorm && f <= TextureFormat::Depth32FloatStencil8;
}

/// True for any depth or stencil format (includes Stencil8).
[[nodiscard]] constexpr bool isDepthStencil(TextureFormat f) {
    return f >= TextureFormat::Depth16Unorm && f <= TextureFormat::Stencil8;
}

/// True if the format has a depth component.
[[nodiscard]] constexpr bool hasDepth(TextureFormat f) {
    switch (f) {
    case TextureFormat::Depth16Unorm:
    case TextureFormat::Depth24Plus:
    case TextureFormat::Depth24PlusStencil8:
    case TextureFormat::Depth32Float:
    case TextureFormat::Depth32FloatStencil8:
        return true;
    default: return false;
    }
}

/// True if the format has a stencil component.
[[nodiscard]] constexpr bool hasStencil(TextureFormat f) {
    switch (f) {
    case TextureFormat::Depth24PlusStencil8:
    case TextureFormat::Depth32FloatStencil8:
    case TextureFormat::Stencil8:
        return true;
    default: return false;
    }
}

/// True for BC or ASTC compressed formats.
[[nodiscard]] constexpr bool isCompressed(TextureFormat f) {
    return f >= TextureFormat::BC1RGBAUnorm && f <= TextureFormat::ASTC8x8UnormSrgb;
}

/// True for sRGB variants.
[[nodiscard]] constexpr bool isSrgb(TextureFormat f) {
    switch (f) {
    case TextureFormat::RGBA8UnormSrgb:
    case TextureFormat::BGRA8UnormSrgb:
    case TextureFormat::BC1RGBAUnormSrgb:
    case TextureFormat::BC2RGBAUnormSrgb:
    case TextureFormat::BC3RGBAUnormSrgb:
    case TextureFormat::BC7RGBAUnormSrgb:
    case TextureFormat::ASTC4x4UnormSrgb:
    case TextureFormat::ASTC5x5UnormSrgb:
    case TextureFormat::ASTC6x6UnormSrgb:
    case TextureFormat::ASTC8x8UnormSrgb:
        return true;
    default: return false;
    }
}

/// Returns bytes per pixel for uncompressed formats; 0 for compressed or unknown.
[[nodiscard]] constexpr u32 bytesPerPixel(TextureFormat f) {
    switch (f) {
    case TextureFormat::R8Unorm:
    case TextureFormat::Stencil8:
        return 1;
    case TextureFormat::R16Uint:
    case TextureFormat::R16Sint:
    case TextureFormat::R16Float:
    case TextureFormat::RG8Unorm:
    case TextureFormat::Depth16Unorm:
        return 2;
    case TextureFormat::RGBA8Unorm:
    case TextureFormat::RGBA8UnormSrgb:
    case TextureFormat::BGRA8Unorm:
    case TextureFormat::BGRA8UnormSrgb:
    case TextureFormat::RG16Float:
    case TextureFormat::R32Float:
    case TextureFormat::R32Uint:
    case TextureFormat::R32Sint:
    case TextureFormat::RGB10A2Unorm:
    case TextureFormat::RG11B10Float:
    case TextureFormat::Depth24Plus:
    case TextureFormat::Depth24PlusStencil8:
    case TextureFormat::Depth32Float:
        return 4;
    case TextureFormat::Depth32FloatStencil8:
        return 8;
    case TextureFormat::RG32Float:
    case TextureFormat::RG32Uint:
    case TextureFormat::RGBA16Float:
    case TextureFormat::RGBA16Uint:
    case TextureFormat::RGBA16Sint:
        return 8;
    case TextureFormat::RGBA32Float:
    case TextureFormat::RGBA32Uint:
    case TextureFormat::RGBA32Sint:
        return 16;
    default: return 0; // Compressed or unknown.
    }
}

} // namespace draco::rhi
