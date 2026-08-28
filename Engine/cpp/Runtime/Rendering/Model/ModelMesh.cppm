/// A mesh within a model containing vertex and index data.

module;
#include <span>
#include <string_view>

#include <cstring>
#include <limits>
#include <string>
#include <vector>

export module model:model_mesh;

import core;
import :vertex_format;
import :mesh_part;

using namespace draco;

export namespace draco::model {

/// Primitive topology.
enum class PrimitiveTopology : u32 {
    Triangles,
    TriangleStrip,
    Lines,
    LineStrip,
    Points,
};

/// A mesh within a model containing vertex/index buffers, parts, and bounds.
class ModelMesh {
public:
    ModelMesh() = default;
    ~ModelMesh() = default;

    // -- Name --

    [[nodiscard]] std::u8string_view name() const { return std::u8string_view(m_name.data(), m_name.size()); }
    void setName(std::u8string_view n) { m_name = std::u8string(n); }

    // -- Property accessors --

    [[nodiscard]] i32 vertexCount()     const { return m_vertexCount; }
    [[nodiscard]] i32 vertexStride()    const { return m_vertexStride; }
    [[nodiscard]] i32 indexCount()      const { return m_indexCount; }
    [[nodiscard]] bool use32BitIndices() const { return m_use32BitIndices; }
    [[nodiscard]] PrimitiveTopology topology() const { return m_topology; }
    [[nodiscard]] math::AABB bounds() const { return m_bounds; }
    [[nodiscard]] bool hasNormals()  const { return m_hasNormals; }
    [[nodiscard]] bool hasTangents() const { return m_hasTangents; }

    void setTopology(PrimitiveTopology t)  { m_topology = t; }
    void setHasNormals(bool v)  { m_hasNormals = v; }
    void setHasTangents(bool v) { m_hasTangents = v; }
    void setBounds(math::AABB b) { m_bounds = b; }

    // -- Parts --

    [[nodiscard]] std::span<const ModelMeshPart> parts() const {
        return std::span<const ModelMeshPart>(m_parts.data(), m_parts.size());
    }
    [[nodiscard]] std::span<ModelMeshPart> parts() {
        return std::span<ModelMeshPart>(m_parts.data(), m_parts.size());
    }
    void addPart(ModelMeshPart part) { m_parts.push_back(part); }

    // -- Vertex elements --

    [[nodiscard]] std::span<const VertexElement> vertexElements() const {
        return std::span<const VertexElement>(m_vertexElements.data(), m_vertexElements.size());
    }
    [[nodiscard]] std::span<VertexElement> vertexElements() {
        return std::span<VertexElement>(m_vertexElements.data(), m_vertexElements.size());
    }
    void addVertexElement(VertexElement elem) { m_vertexElements.push_back(elem); }

    // -- Vertex data --

    /// Allocate vertex buffer.
    void allocateVertices(i32 count, i32 stride) {
        m_vertexCount  = count;
        m_vertexStride = stride;
        m_vertexData.resize(static_cast<usize>(count) * stride, 0);
    }

    /// Get raw vertex data pointer (nullptr if empty).
    [[nodiscard]] const u8* getVertexData() const {
        return m_vertexData.empty() ? nullptr : m_vertexData.data();
    }

    [[nodiscard]] u8* getVertexData() {
        return m_vertexData.empty() ? nullptr : m_vertexData.data();
    }

    /// Vertex data size in bytes.
    [[nodiscard]] i32 getVertexDataSize() const { return m_vertexCount * m_vertexStride; }

    /// Copy typed data into the vertex buffer.
    template <typename T>
    void setVertexData(const T* data, usize count) {
        usize bytes = count * sizeof(T);
        if (m_vertexData.empty() || bytes > m_vertexData.size()) return;
        std::memcpy(m_vertexData.data(), data, bytes);
    }

    // -- Index data --

    /// Allocate index buffer.
    void allocateIndices(i32 count, bool use32Bit) {
        m_indexCount      = count;
        m_use32BitIndices = use32Bit;
        i32 indexSize = use32Bit ? 4 : 2;
        m_indexData.resize(static_cast<usize>(count) * indexSize, 0);
    }

    /// Get raw index data pointer (nullptr if empty).
    [[nodiscard]] const u8* getIndexData() const {
        return m_indexData.empty() ? nullptr : m_indexData.data();
    }

    [[nodiscard]] u8* getIndexData() {
        return m_indexData.empty() ? nullptr : m_indexData.data();
    }

    /// Index data size in bytes.
    [[nodiscard]] i32 getIndexDataSize() const {
        return m_indexCount * (m_use32BitIndices ? 4 : 2);
    }

    /// Copy u16 index data.
    void setIndexData(const u16* indices, usize count) {
        usize bytes = count * 2;
        if (m_indexData.empty() || m_use32BitIndices || bytes > m_indexData.size()) return;
        std::memcpy(m_indexData.data(), indices, bytes);
    }

    /// Copy u32 index data.
    void setIndexData(const u32* indices, usize count) {
        usize bytes = count * 4;
        if (m_indexData.empty() || !m_use32BitIndices || bytes > m_indexData.size()) return;
        std::memcpy(m_indexData.data(), indices, bytes);
    }

    // -- Skinning helpers --

    /// Converts a non-skinned mesh into a skinned one by adding uniform bone weighting.
    /// All vertices are assigned to a single joint with weight 1.0.
    /// Expands vertex buffer by 24 bytes/vertex (UShort4 joints + Float4 weights).
    void addUniformSkinning(i32 jointIndex) {
        // Skip if mesh already has joint data.
        for (auto& elem : m_vertexElements)
            if (elem.semantic == VertexSemantic::Joints) return;

        if (m_vertexData.empty() || m_vertexCount == 0) return;

        i32 oldStride = m_vertexStride;
        i32 newStride = oldStride + 24; // +8 (UShort4) +16 (Float4)
        std::vector<u8> newData(static_cast<usize>(m_vertexCount) * newStride, 0);

        for (i32 i = 0; i < m_vertexCount; ++i) {
            i32 srcOff = i * oldStride;
            i32 dstOff = i * newStride;

            // Copy existing vertex data.
            std::memcpy(&newData[dstOff], &m_vertexData[srcOff], oldStride);

            // Write joints: uint16[4] = (jointIndex, 0, 0, 0).
            auto* joints = reinterpret_cast<u16*>(&newData[dstOff + oldStride]);
            joints[0] = static_cast<u16>(jointIndex);
            joints[1] = 0;
            joints[2] = 0;
            joints[3] = 0;

            // Write weights: float[4] = (1, 0, 0, 0).
            auto* weights = reinterpret_cast<f32*>(&newData[dstOff + oldStride + 8]);
            weights[0] = 1.0f;
            weights[1] = 0.0f;
            weights[2] = 0.0f;
            weights[3] = 0.0f;
        }

        m_vertexData  = static_cast<std::vector<u8>&&>(newData);
        m_vertexStride = newStride;

        m_vertexElements.push_back(VertexElement(VertexSemantic::Joints, VertexElementFormat::UShort4, oldStride));
        m_vertexElements.push_back(VertexElement(VertexSemantic::Weights, VertexElementFormat::Float4, oldStride + 8));
    }

    /// Scales vertex positions by a non-uniform scale vector.
    void scalePositions(math::Vector3 scale) {
        i32 posOffset = -1;
        for (auto& elem : m_vertexElements) {
            if (elem.semantic == VertexSemantic::Position) {
                posOffset = elem.offset;
                break;
            }
        }
        if (posOffset < 0 || m_vertexData.empty()) return;

        for (i32 i = 0; i < m_vertexCount; ++i) {
            i32 off = i * m_vertexStride + posOffset;
            auto* pos = reinterpret_cast<math::Vector3*>(&m_vertexData[off]);
            pos->x *= scale.x;
            pos->y *= scale.y;
            pos->z *= scale.z;
        }
    }

    /// Remaps vertex joint indices using the provided mapping array.
    /// remap[oldJointIndex] = newJointIndex.
    void remapJointIndices(const i32* remap, usize remapCount) {
        i32 jointsOffset = -1;
        for (auto& elem : m_vertexElements) {
            if (elem.semantic == VertexSemantic::Joints) {
                jointsOffset = elem.offset;
                break;
            }
        }
        if (jointsOffset < 0 || m_vertexData.empty()) return;

        for (i32 i = 0; i < m_vertexCount; ++i) {
            i32 off = i * m_vertexStride + jointsOffset;
            auto* joints = reinterpret_cast<u16*>(&m_vertexData[off]);
            for (int j = 0; j < 4; ++j) {
                i32 oldIdx = static_cast<i32>(joints[j]);
                if (oldIdx >= 0 && static_cast<usize>(oldIdx) < remapCount)
                    joints[j] = static_cast<u16>(remap[oldIdx]);
            }
        }
    }

    /// Calculate bounds from position data.
    void calculateBounds() {
        if (m_vertexData.empty() || m_vertexCount == 0) {
            m_bounds = math::AABB{};
            return;
        }

        i32 posOffset = -1;
        for (auto& elem : m_vertexElements) {
            if (elem.semantic == VertexSemantic::Position) {
                posOffset = elem.offset;
                break;
            }
        }
        if (posOffset < 0) {
            m_bounds = math::AABB{};
            return;
        }

        math::Vector3 bmin(std::numeric_limits<f32>::max());
        math::Vector3 bmax(std::numeric_limits<f32>::lowest());

        for (i32 i = 0; i < m_vertexCount; ++i) {
            i32 off = i * m_vertexStride + posOffset;
            math::Vector3 pos{};
            std::memcpy(&pos, &m_vertexData[off], sizeof(math::Vector3));
            bmin = min(bmin, pos);
            bmax = max(bmax, pos);
        }

        m_bounds = math::AABB{bmin, bmax};
    }

private:
    std::u8string m_name;
    std::vector<u8> m_vertexData;
    std::vector<u8> m_indexData;
    std::vector<ModelMeshPart> m_parts;
    std::vector<VertexElement> m_vertexElements;

    i32 m_vertexCount  = 0;
    i32 m_vertexStride = 0;
    i32 m_indexCount   = 0;
    bool m_use32BitIndices = false;
    PrimitiveTopology m_topology = PrimitiveTopology::Triangles;
    math::AABB m_bounds;

    bool m_hasNormals  = false;
    bool m_hasTangents = false;
};

} // namespace draco::model
