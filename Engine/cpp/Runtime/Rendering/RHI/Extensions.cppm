/// Extension base classes for multiple inheritance.
///
/// VK backend encoders inherit these alongside their core base classes:
///   class VkRenderPassEncoder : public RenderPassEncoder, public MeshShaderPassExt { ... };
///   class VkCommandEncoder : public CommandEncoder, public RayTracingEncoderExt { ... };
///
/// Callers probe for support via the asMeshShaderExt() / asRayTracingExt()
/// cross-query methods (which return `this` on supporting encoders, else null).

module;

#include <span>

export module rhi:extensions;

import core.stdtypes;
import :enums;
import :types;
import :descriptors;
import :ext_descriptors;
import :resources;

using namespace draco;

export namespace draco::rhi {

/// Mesh shader render pass extension. Implemented by backend
/// RenderPassEncoder subclasses that support mesh shaders.
class MeshShaderPassExt {
public:
    virtual ~MeshShaderPassExt() = default;

    /// Bind a mesh shader pipeline.
    virtual void setMeshPipeline(MeshPipeline* pipeline) = 0;
    /// Dispatch mesh shader work groups.
    virtual void drawMeshTasks(u32 groupCountX, u32 groupCountY = 1, u32 groupCountZ = 1) = 0;
    /// Dispatch mesh shader work groups via an indirect buffer.
    virtual void drawMeshTasksIndirect(Buffer* buffer, u64 offset, u32 drawCount = 1, u32 stride = 0) = 0;
    /// Dispatch mesh shader work groups with an indirect count buffer.
    virtual void drawMeshTasksIndirectCount(Buffer* buffer, u64 offset,
                                            Buffer* countBuffer, u64 countOffset,
                                            u32 maxDrawCount, u32 stride) = 0;
};

/// Ray tracing command encoder extension. Implemented by backend
/// CommandEncoder subclasses that support ray tracing.
class RayTracingEncoderExt {
public:
    virtual ~RayTracingEncoderExt() = default;

    /// Build a bottom-level acceleration structure from triangle and/or AABB geometry.
    virtual void buildBottomLevelAccelStruct(AccelStruct* dst, Buffer* scratchBuffer, u64 scratchOffset,
                                             std::span<const AccelStructGeometryTriangles> triangles,
                                             std::span<const AccelStructGeometryAABBs> aabbs) = 0;

    /// Build a top-level acceleration structure from an instance buffer.
    virtual void buildTopLevelAccelStruct(AccelStruct* dst, Buffer* scratchBuffer, u64 scratchOffset,
                                          Buffer* instanceBuffer, u64 instanceOffset, u32 instanceCount) = 0;

    /// Bind a ray tracing pipeline.
    virtual void setRayTracingPipeline(RayTracingPipeline* pipeline) = 0;

    /// Bind a resource group for the ray tracing pipeline.
    virtual void setBindGroup(u32 index, BindGroup* group, std::span<const u32> dynamicOffsets = {}) = 0;

    /// Upload push constants for the ray tracing pipeline.
    virtual void setPushConstants(ShaderStage stages, u32 offset, u32 size, const void* data) = 0;

    /// Dispatch rays using shader binding tables.
    virtual void traceRays(Buffer* raygenSBT, u64 raygenOffset, u64 raygenStride,
                           Buffer* missSBT, u64 missOffset, u64 missStride,
                           Buffer* hitSBT, u64 hitOffset, u64 hitStride,
                           u32 width, u32 height, u32 depth = 1) = 0;
};

} // namespace draco::rhi
