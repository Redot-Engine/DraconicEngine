module;

#include <algorithm>
#include <cmath>

export module core.color;

import core.stdtypes;

export namespace draco
{
    // =======================================================================
    // Color - linear RGBA, float components (typically 0..1).
    // =======================================================================
    struct Color
    {
        f32 r = 0.0f;
        f32 g = 0.0f;
        f32 b = 0.0f;
        f32 a = 1.0f;

        constexpr Color() noexcept = default;
        constexpr Color(f32 inR, f32 inG, f32 inB, f32 inA = 1.0f) noexcept : r(inR), g(inG), b(inB), a(inA) {}

        // Packs to 0xRRGGBBAA (components clamped to 0..1).
        [[nodiscard]] u32 toRGBA8() const noexcept
        {
            const auto byteOf = [](f32 c) -> u32
            {
                return static_cast<u32>(std::clamp(c, 0.0f, 1.0f) * 255.0f + 0.5f);
            };
            return (byteOf(r) << 24) | (byteOf(g) << 16) | (byteOf(b) << 8) | byteOf(a);
        }

        [[nodiscard]] static Color fromRGBA8(u32 packed) noexcept
        {
            return Color{ static_cast<f32>((packed >> 24) & 0xFFu) / 255.0f,
                          static_cast<f32>((packed >> 16) & 0xFFu) / 255.0f,
                          static_cast<f32>((packed >> 8) & 0xFFu) / 255.0f,
                          static_cast<f32>(packed & 0xFFu) / 255.0f };
        }

        static const Color White;
        static const Color Black;
        static const Color Red;
        static const Color Green;
        static const Color Blue;
        static const Color Transparent;
    };

    inline constexpr Color Color::White{ 1.0f, 1.0f, 1.0f, 1.0f };
    inline constexpr Color Color::Black{ 0.0f, 0.0f, 0.0f, 1.0f };
    inline constexpr Color Color::Red{ 1.0f, 0.0f, 0.0f, 1.0f };
    inline constexpr Color Color::Green{ 0.0f, 1.0f, 0.0f, 1.0f };
    inline constexpr Color Color::Blue{ 0.0f, 0.0f, 1.0f, 1.0f };
    inline constexpr Color Color::Transparent{ 0.0f, 0.0f, 0.0f, 0.0f };

    [[nodiscard]] constexpr Color operator*(Color c, f32 s) noexcept { return { c.r * s, c.g * s, c.b * s, c.a * s }; }
    [[nodiscard]] constexpr Color operator+(Color a, Color b) noexcept { return { a.r + b.r, a.g + b.g, a.b + b.b, a.a + b.a }; }
    [[nodiscard]] constexpr bool operator==(Color a, Color b) noexcept
    {
        return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
    }

    [[nodiscard]] constexpr Color lerp(Color a, Color b, f32 t) noexcept
    {
        return { std::lerp(a.r, b.r, t), std::lerp(a.g, b.g, t), std::lerp(a.b, b.b, t), std::lerp(a.a, b.a, t) };
    }

    [[nodiscard]] inline bool nearlyEqual(Color a, Color b, f32 epsilon = 1e-5f) noexcept
    {
        return std::abs(a.r - b.r) <= epsilon && std::abs(a.g - b.g) <= epsilon
            && std::abs(a.b - b.b) <= epsilon && std::abs(a.a - b.a) <= epsilon;
    }

    // =======================================================================
    // Color32 - packed 8-bit RGBA, for compact storage: image pixels, vertex
    // colors, asset interchange. The byte-color counterpart of the float Color
    // (which stays the linear/HDR runtime currency). Conversions are plain
    // 0..255 <-> 0..1 with NO gamma; sRGB<->linear is handled explicitly at the
    // texture/format edge (ImageColorSpace, *UnormSrgb formats).
    // =======================================================================
    struct Color32
    {
        u8 r = 0;
        u8 g = 0;
        u8 b = 0;
        u8 a = 255;

        constexpr Color32() noexcept = default;
        constexpr Color32(u8 inR, u8 inG, u8 inB, u8 inA = 255) noexcept : r(inR), g(inG), b(inB), a(inA) {}

        // Packs to 0xRRGGBBAA.
        [[nodiscard]] constexpr u32 toRGBA8() const noexcept
        {
            return (static_cast<u32>(r) << 24) | (static_cast<u32>(g) << 16)
                 | (static_cast<u32>(b) << 8) | static_cast<u32>(a);
        }

        [[nodiscard]] static constexpr Color32 fromRGBA8(u32 packed) noexcept
        {
            return Color32{ static_cast<u8>((packed >> 24) & 0xFFu),
                            static_cast<u8>((packed >> 16) & 0xFFu),
                            static_cast<u8>((packed >> 8) & 0xFFu),
                            static_cast<u8>(packed & 0xFFu) };
        }

        static const Color32 White;
        static const Color32 Black;
        static const Color32 Red;
        static const Color32 Green;
        static const Color32 Blue;
        static const Color32 Transparent;
    };

    inline constexpr Color32 Color32::White{ 255, 255, 255, 255 };
    inline constexpr Color32 Color32::Black{ 0, 0, 0, 255 };
    inline constexpr Color32 Color32::Red{ 255, 0, 0, 255 };
    inline constexpr Color32 Color32::Green{ 0, 255, 0, 255 };
    inline constexpr Color32 Color32::Blue{ 0, 0, 255, 255 };
    inline constexpr Color32 Color32::Transparent{ 0, 0, 0, 0 };

    [[nodiscard]] constexpr bool operator==(Color32 a, Color32 b) noexcept
    {
        return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
    }

    // Float Color -> packed bytes (components clamped to 0..1, rounded).
    [[nodiscard]] constexpr Color32 toColor32(Color c) noexcept
    {
        const auto byteOf = [](f32 v) -> u8
        {
            return static_cast<u8>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f);
        };
        return Color32{ byteOf(c.r), byteOf(c.g), byteOf(c.b), byteOf(c.a) };
    }

    // Packed bytes -> float Color (exact 0..255 -> 0..1; round-trips toColor32).
    [[nodiscard]] constexpr Color toColor(Color32 c) noexcept
    {
        return Color{ static_cast<f32>(c.r) / 255.0f, static_cast<f32>(c.g) / 255.0f,
                      static_cast<f32>(c.b) / 255.0f, static_cast<f32>(c.a) / 255.0f };
    }

    // sRGB <-> linear transfer functions (the standard IEC 61966-2-1 EOTF). For
    // decoding sRGB-authored colors to linear before linear-space blending/shading.
    [[nodiscard]] inline f32 srgbToLinear(f32 c) noexcept
    {
        return (c <= 0.04045f) ? (c / 12.92f) : std::pow((c + 0.055f) / 1.055f, 2.4f);
    }

    [[nodiscard]] inline f32 linearToSrgb(f32 c) noexcept
    {
        return (c <= 0.0031308f) ? (c * 12.92f) : (1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f);
    }

    // Color32 (sRGB-authored) -> linear float Color (RGB through the sRGB EOTF;
    // alpha is linear). For uploading UI/SVG colors to a linear/sRGB pipeline.
    [[nodiscard]] inline Color toLinear(Color32 c) noexcept
    {
        return Color{ srgbToLinear(static_cast<f32>(c.r) / 255.0f),
                      srgbToLinear(static_cast<f32>(c.g) / 255.0f),
                      srgbToLinear(static_cast<f32>(c.b) / 255.0f),
                      static_cast<f32>(c.a) / 255.0f };
    }
}
