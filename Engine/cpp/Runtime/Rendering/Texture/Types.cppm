// Draconic::Texture - :types partition
//
// Logical texture descriptors: shape, filter, wrap. Used by TextureResource to
// say how pixel data should be interpreted and sampled. Ported from
// Sedulous.Textures/TextureTypes.bf.

module;

export module texture:types;

import core;

using namespace draco;

export namespace draco::texture
{
    // The logical shape of a texture asset.
    enum class TextureShape : u8
    {
        Texture2D,      // standard 2D image
        Texture2DArray, // multiple same-size layers
        Texture3D,      // volume
        Cubemap,        // 6 square faces (+X,-X,+Y,-Y,+Z,-Z)
        CubemapArray,   // array of cubemaps
    };

    enum class TextureFilter : u8
    {
        Nearest,
        Linear,
        MipmapNearest,
        MipmapLinear,
    };

    enum class TextureWrap : u8
    {
        Repeat,
        ClampToEdge,
        ClampToBorder,
        MirroredRepeat,
    };
}
