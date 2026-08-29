/// Abstract command recording interfaces: CommandPool, CommandEncoder,
/// RenderPassEncoder, ComputePassEncoder, TransferBatch.

module;

#include <span>
#include <string_view>

export module rhi:commands;

import core.stdtypes;
import core.status;
import :forward;
import :enums;
import :types;
import :descriptors;
import :resources;

using namespace draco;

export namespace draco::rhi {

// ---- Render Command Encoder (shared draw-recording surface) ----

/// The draw-recording commands common to a render pass and a render bundle. A `Renderer` that
/// takes a `RenderCommandEncoder*` records identically whether it targets a live pass (inline)
/// or an off-thread `RenderBundleEncoder` - which is what makes parallel command recording
/// fall out (split a draw list into N bundles recorded on N threads, then executeBundles).
/// This is exactly the subset valid inside a WebGPU render bundle: no pass-level dynamic state
/// (viewport / scissor / blend constant / stencil ref are inherited from the pass), no queries.
class RenderCommandEncoder {
public:
    virtual ~RenderCommandEncoder() = default;

    /// Bind a graphics (rasterization) pipeline.
    virtual void setPipeline(RenderPipeline* pipeline) = 0;
    /// Bind a resource group at the given index.
    virtual void setBindGroup(u32 index, BindGroup* group, std::span<const u32> dynamicOffsets = {}) = 0;
    /// Upload push constant data.
    virtual void setPushConstants(ShaderStage stages, u32 offset, u32 size, const void* data) = 0;

    /// Bind a vertex buffer to a slot.
    virtual void setVertexBuffer(u32 slot, Buffer* buffer, u64 offset = 0) = 0;
    /// Bind an index buffer.
    virtual void setIndexBuffer(Buffer* buffer, IndexFormat format, u64 offset = 0) = 0;

    /// Issue a non-indexed draw call.
    virtual void draw(u32 vertexCount, u32 instanceCount = 1, u32 firstVertex = 0, u32 firstInstance = 0) = 0;
    /// Issue an indexed draw call.
    virtual void drawIndexed(u32 indexCount, u32 instanceCount = 1, u32 firstIndex = 0, i32 baseVertex = 0, u32 firstInstance = 0) = 0;
    /// Issue an indirect draw call.
    virtual void drawIndirect(Buffer* buffer, u64 offset, u32 drawCount = 1, u32 stride = 0) = 0;
    /// Issue an indexed indirect draw call.
    virtual void drawIndexedIndirect(Buffer* buffer, u64 offset, u32 drawCount = 1, u32 stride = 0) = 0;
};

// ---- Render Pass Encoder ----

/// Records draw commands within a render pass: the shared recording surface plus the pass-level
/// dynamic state, queries, bundle execution, and pass control.
class RenderPassEncoder : public RenderCommandEncoder {
public:
    /// Cross-query for the mesh-shader extension (replacement for a sideways
    /// dynamic_cast). Encoders that support it return `this`.
    [[nodiscard]] virtual MeshShaderPassExt* asMeshShaderExt() noexcept { return nullptr; }

    /// Set the viewport rectangle and depth range.
    virtual void setViewport(f32 x, f32 y, f32 w, f32 h, f32 minDepth = 0.0f, f32 maxDepth = 1.0f) = 0;
    /// Set the scissor rectangle.
    virtual void setScissor(i32 x, i32 y, u32 w, u32 h) = 0;
    /// Set the blend constant color.
    virtual void setBlendConstant(f32 r, f32 g, f32 b, f32 a) = 0;
    /// Set the stencil reference value.
    virtual void setStencilReference(u32 reference) = 0;

    /// Execute pre-recorded render bundles in order (replays their draws into this pass). The
    /// pass must have been begun with RenderPassContents::SecondaryCommandBuffers.
    virtual void executeBundles(std::span<RenderBundle* const> bundles) = 0;

    /// Write a timestamp query.
    virtual void writeTimestamp(QuerySet* querySet, u32 index) = 0;
    /// Begin an occlusion query.
    virtual void beginOcclusionQuery(QuerySet* querySet, u32 index) = 0;
    /// End an occlusion query.
    virtual void endOcclusionQuery(QuerySet* querySet, u32 index) = 0;

    /// End the render pass.
    virtual void end() = 0;
};

// ---- Render Bundle ----

/// An immutable, pre-recorded sequence of draw commands, replayable into any compatible render
/// pass (matching attachment formats) via RenderPassEncoder::executeBundles. Valid until its
/// owning command pool is reset. Recorded off the main thread for parallel command recording.
class RenderBundle {
public:
    virtual ~RenderBundle() = default;
};

/// Records draws into a render bundle (the shared recording surface only - no pass-level state).
class RenderBundleEncoder : public RenderCommandEncoder {
public:
    /// Finish recording and return the immutable bundle (owned by the command pool).
    [[nodiscard]] virtual RenderBundle* finish() = 0;
};

// ---- Compute Pass Encoder ----

/// Records compute dispatch commands within a compute pass.
class ComputePassEncoder {
public:
    virtual ~ComputePassEncoder() = default;

    virtual void setPipeline(ComputePipeline* pipeline) = 0;
    virtual void setBindGroup(u32 index, BindGroup* group, std::span<const u32> dynamicOffsets = {}) = 0;
    virtual void setPushConstants(ShaderStage stages, u32 offset, u32 size, const void* data) = 0;

    /// Dispatch compute work groups.
    virtual void dispatch(u32 x, u32 y = 1, u32 z = 1) = 0;
    /// Dispatch compute work groups via an indirect buffer.
    virtual void dispatchIndirect(Buffer* buffer, u64 offset) = 0;

    /// Insert a compute-to-compute memory barrier.
    virtual void computeBarrier() = 0;

    virtual void writeTimestamp(QuerySet* querySet, u32 index) = 0;
    virtual void end() = 0;
};

// ---- Command Encoder ----

/// Records GPU commands: render/compute passes, barriers, copies, queries.
class CommandEncoder {
public:
    virtual ~CommandEncoder() = default;

    /// Cross-query for the ray-tracing extension (replacement for a sideways
    /// dynamic_cast). Encoders that support it return `this`.
    [[nodiscard]] virtual RayTracingEncoderExt* asRayTracingExt() noexcept { return nullptr; }

    /// Begin a render pass. Returns the encoder for recording draw commands.
    [[nodiscard]] virtual RenderPassEncoder* beginRenderPass(const RenderPassDesc& desc) = 0;
    /// Begin a compute pass.
    [[nodiscard]] virtual ComputePassEncoder* beginComputePass(std::u8string_view label = {}) = 0;
    /// Begin recording a render bundle (a reusable, off-thread-recordable draw sequence
    /// replayable into passes matching `desc`'s attachment signature). Returns null if the
    /// backend does not support bundles.
    [[nodiscard]] virtual RenderBundleEncoder* createRenderBundleEncoder(const RenderBundleDesc& desc) = 0;

    /// Insert resource barriers.
    virtual void barrier(const BarrierGroup& group) = 0;

    /// Convenience: transition a single texture between resource states.
    void transitionTexture(Texture* tex, ResourceState oldState, ResourceState newState) {
        TextureBarrier tb{}; tb.texture = tex; tb.oldState = oldState; tb.newState = newState;
        BarrierGroup g{}; g.textureBarriers = std::span<const TextureBarrier>(&tb, 1);
        barrier(g);
    }

    /// Convenience: transition a single buffer between resource states.
    void transitionBuffer(Buffer* buf, ResourceState oldState, ResourceState newState) {
        BufferBarrier bb{}; bb.buffer = buf; bb.oldState = oldState; bb.newState = newState;
        BarrierGroup g{}; g.bufferBarriers = std::span<const BufferBarrier>(&bb, 1);
        barrier(g);
    }

    /// Copy operations.
    virtual void copyBufferToBuffer(Buffer* src, u64 srcOffset, Buffer* dst, u64 dstOffset, u64 size) = 0;
    virtual void copyBufferToTexture(Buffer* src, Texture* dst, const BufferTextureCopyRegion& region) = 0;
    virtual void copyTextureToBuffer(Texture* src, Buffer* dst, const BufferTextureCopyRegion& region) = 0;
    virtual void copyTextureToTexture(Texture* src, Texture* dst, const TextureCopyRegion& region) = 0;

    /// Blit (scaled copy) from one texture to another.
    virtual void blit(Texture* src, Texture* dst) = 0;
    /// Generate mipmaps for a texture.
    virtual void generateMipmaps(Texture* texture) = 0;
    /// Resolve a multisampled texture to a single-sampled texture.
    virtual void resolveTexture(Texture* src, Texture* dst) = 0;

    /// Query operations.
    virtual void resetQuerySet(QuerySet* querySet, u32 first, u32 count) = 0;
    virtual void writeTimestamp(QuerySet* querySet, u32 index) = 0;
    virtual void resolveQuerySet(QuerySet* querySet, u32 first, u32 count, Buffer* dst, u64 dstOffset) = 0;

    /// Debug labels.
    virtual void beginDebugLabel(std::u8string_view label, f32 r = 0, f32 g = 0, f32 b = 0, f32 a = 1) = 0;
    virtual void endDebugLabel() = 0;
    virtual void insertDebugLabel(std::u8string_view label, f32 r = 0, f32 g = 0, f32 b = 0, f32 a = 1) = 0;

    /// Finish recording and return an immutable command buffer.
    [[nodiscard]] virtual CommandBuffer* finish() = 0;
};

// ---- Command Pool ----

/// Manages command buffer memory for a single queue type.
/// One pool per thread per queue type.
class CommandPool {
public:
    virtual ~CommandPool() = default;

    /// Create a new command encoder for recording.
    virtual Status createEncoder(CommandEncoder*& out) = 0;
    /// Destroy a command encoder.
    virtual void destroyEncoder(CommandEncoder*& encoder) = 0;
    /// Reset all command buffers allocated from this pool.
    virtual void reset() = 0;
};

// ---- Transfer Batch ----

/// Batches staging upload operations (CPU->GPU buffer/texture writes).
class TransferBatch {
public:
    virtual ~TransferBatch() = default;

    /// Stage a buffer write.
    virtual void writeBuffer(Buffer* dst, u64 dstOffset, std::span<const u8> data) = 0;
    /// Stage a texture write.
    virtual void writeTexture(Texture* dst, std::span<const u8> data,
                              const TextureDataLayout& layout, Extent3D extent,
                              u32 mipLevel = 0, u32 arrayLayer = 0) = 0;

    /// Submit all staged writes synchronously.
    virtual Status submit() = 0;
    /// Submit all staged writes, signaling a fence on completion.
    virtual Status submitAsync(Fence* fence, u64 signalValue) = 0;

    /// Reset the batch for reuse.
    virtual void reset() = 0;
    /// Destroy the batch.
    virtual void destroy() = 0;
};

} // namespace draco::rhi
