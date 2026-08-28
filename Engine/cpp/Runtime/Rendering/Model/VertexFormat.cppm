/// Vertex element semantics, formats, and descriptors.

export module model:vertex_format;

import core;

using namespace draco;

export namespace draco::model {

/// Vertex element semantic types.
enum class VertexSemantic : u32 {
    Position,
    Normal,
    TexCoord,
    Color,
    Tangent,
    Joints,
    Weights,
};

/// Vertex element data formats.
enum class VertexElementFormat : u32 {
    Float,
    Float2,
    Float3,
    Float4,
    Byte4,
    UShort2,
    UShort4,
};

/// Describes a single element in a vertex layout.
struct VertexElement {
    VertexSemantic semantic = VertexSemantic::Position;
    VertexElementFormat format = VertexElementFormat::Float3;
    i32 offset = 0;
    i32 semanticIndex = 0; // For multiple UV channels, etc.

    constexpr VertexElement() = default;
    constexpr VertexElement(VertexSemantic sem, VertexElementFormat fmt, i32 off, i32 semIdx = 0)
        : semantic(sem), format(fmt), offset(off), semanticIndex(semIdx) {}

    /// Size of this element in bytes.
    [[nodiscard]] constexpr i32 size() const {
        switch (format) {
        case VertexElementFormat::Float:    return 4;
        case VertexElementFormat::Float2:   return 8;
        case VertexElementFormat::Float3:   return 12;
        case VertexElementFormat::Float4:   return 16;
        case VertexElementFormat::Byte4:    return 4;
        case VertexElementFormat::UShort2:  return 4;
        case VertexElementFormat::UShort4:  return 8;
        }
        return 0;
    }
};

} // namespace draco::model
