/// Descriptor structs for mesh shader and ray tracing extensions.

module;

#include <optional>
#include <span>
#include <string_view>

export module rhi:ext_descriptors;

import core.stdtypes;
import :enums;
import :texture_format;
import :types;
import :forward;
import :descriptors;

using namespace draco;

export namespace draco::rhi {

// ---- Mesh shader pipeline ----

/// Descriptor for creating a mesh shader pipeline.
struct MeshPipelineDesc {
    PipelineLayout*   layout = nullptr;
    std::optional<ProgrammableStage> task;       ///< Optional task (amplification) shader.
    ProgrammableStage mesh;                       ///< Required mesh shader.
    std::optional<FragmentState> fragment;
    std::span<const ColorTargetState> colorTargets;
    PrimitiveState    primitive;
    std::optional<DepthStencilState> depthStencil;
    MultisampleState  multisample;
    PipelineCache*    cache = nullptr;
    std::u8string_view label;
};

// ---- Ray tracing ----

/// Descriptor for creating an acceleration structure.
struct AccelStructDesc {
    AccelStructType      type  = AccelStructType::BottomLevel;
    AccelStructBuildFlags flags = AccelStructBuildFlags::PreferFastTrace;
    std::u8string_view   label;
};

/// Triangle geometry for BLAS construction.
struct AccelStructGeometryTriangles {
    Buffer*      vertexBuffer = nullptr;
    u64          vertexOffset = 0;
    u32          vertexCount  = 0;
    u32          vertexStride = 0;
    VertexFormat vertexFormat = VertexFormat::Float32x3;
    Buffer*      indexBuffer  = nullptr;
    u64          indexOffset  = 0;
    u32          indexCount   = 0;
    IndexFormat  indexFormat  = IndexFormat::UInt32;
    Buffer*      transformBuffer = nullptr;
    u64          transformOffset = 0;
    GeometryFlags flags = GeometryFlags::Opaque;
};

/// AABB geometry for procedural BLAS construction.
struct AccelStructGeometryAABBs {
    Buffer*       aabbBuffer = nullptr;
    u64           offset     = 0;
    u32           count      = 0;
    u32           stride     = 24;   ///< sizeof(VkAabbPositionsKHR)
    GeometryFlags flags      = GeometryFlags::Opaque;
};

/// Shader group definition for a ray tracing pipeline.
struct RayTracingShaderGroup {
    enum class Type : u32 { General, TrianglesHitGroup, ProceduralHitGroup };

    Type type                    = Type::General;
    u32  generalShaderIndex      = ~0u;
    u32  closestHitShaderIndex   = ~0u;
    u32  anyHitShaderIndex       = ~0u;
    u32  intersectionShaderIndex = ~0u;
};

/// Descriptor for creating a ray tracing pipeline.
struct RayTracingPipelineDesc {
    PipelineLayout*                    layout = nullptr;
    std::span<const ProgrammableStage>      stages;
    std::span<const RayTracingShaderGroup>  groups;
    u32          maxRecursionDepth = 1;
    u32          maxPayloadSize   = 0;
    u32          maxAttributeSize = 0;
    PipelineCache* cache          = nullptr;
    std::u8string_view label;
};

} // namespace draco::rhi
