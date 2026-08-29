/// Null RHI backend - stub implementations for all interfaces.
/// Useful for headless testing, CI, or when no GPU is available.

module;

#include <span>
#include <string_view>
#include <vector>

export module rhi.null;

import core.stdtypes;
import core.status;
import rhi;

using namespace draco;

export namespace draco::rhi::null {

// ---- Stub resource classes ----

class NullBuffer : public Buffer {
public:
    void* map()   override { return m_mapped; }
    void  unmap() override {}
    void  allocate(u64 size) { m_data.resize(static_cast<usize>(size)); m_mapped = m_data.data(); }
private:
    std::vector<u8> m_data;
    void* m_mapped = nullptr;
};

class NullTexture : public Texture {};
class NullTextureView : public TextureView {};
class NullSampler : public Sampler {};
class NullShaderModule : public ShaderModule {};
class NullSurface : public Surface {};
class NullCommandBuffer : public CommandBuffer {};

class NullFence : public Fence {
public:
    u64  completedValue() override { return m_value; }
    bool wait(u64 value, u64) override { m_value = value; return true; }
    void signal(u64 v) { m_value = v; }
private:
    u64 m_value = 0;
};

class NullQuerySet : public QuerySet {};

class NullBindGroupLayout : public BindGroupLayout {
public:
    std::span<const BindGroupLayoutEntry> entries() const override { return {}; }
};

class NullBindGroup : public BindGroup {
public:
    BindGroupLayout* layout() override { return nullptr; }
    void updateBindless(std::span<const BindlessUpdateEntry>) override {}
};

class NullPipelineLayout : public PipelineLayout {};

class NullPipelineCache : public PipelineCache {
public:
    u32    getDataSize() override { return 0; }
    Status getData(std::span<u8>) override { return ErrorCode::Ok; }
};

class NullRenderPipeline : public RenderPipeline {};
class NullComputePipeline : public ComputePipeline {};
class NullMeshPipeline : public MeshPipeline {};

class NullAccelStruct : public AccelStruct {
public:
    AccelStructType type()          const override { return AccelStructType::BottomLevel; }
    u64             deviceAddress() const override { return 0; }
};

class NullRayTracingPipeline : public RayTracingPipeline {};

// ---- Stub encoders ----

class NullRenderPassEncoder : public RenderPassEncoder, public MeshShaderPassExt {
public:
    MeshShaderPassExt* asMeshShaderExt() noexcept override { return this; }
    void setPipeline(RenderPipeline*) override {}
    void setBindGroup(u32, BindGroup*, std::span<const u32>) override {}
    void setPushConstants(ShaderStage, u32, u32, const void*) override {}
    void setVertexBuffer(u32, Buffer*, u64) override {}
    void setIndexBuffer(Buffer*, IndexFormat, u64) override {}
    void setViewport(f32, f32, f32, f32, f32, f32) override {}
    void setScissor(i32, i32, u32, u32) override {}
    void setBlendConstant(f32, f32, f32, f32) override {}
    void setStencilReference(u32) override {}
    void draw(u32, u32, u32, u32) override {}
    void drawIndexed(u32, u32, u32, i32, u32) override {}
    void drawIndirect(Buffer*, u64, u32, u32) override {}
    void drawIndexedIndirect(Buffer*, u64, u32, u32) override {}
    void executeBundles(std::span<RenderBundle* const>) override {}
    void writeTimestamp(QuerySet*, u32) override {}
    void beginOcclusionQuery(QuerySet*, u32) override {}
    void endOcclusionQuery(QuerySet*, u32) override {}
    void end() override {}
    void setMeshPipeline(MeshPipeline*) override {}
    void drawMeshTasks(u32, u32, u32) override {}
    void drawMeshTasksIndirect(Buffer*, u64, u32, u32) override {}
    void drawMeshTasksIndirectCount(Buffer*, u64, Buffer*, u64, u32, u32) override {}
};

class NullRenderBundle : public RenderBundle {};

class NullRenderBundleEncoder : public RenderBundleEncoder {
public:
    void setPipeline(RenderPipeline*) override {}
    void setBindGroup(u32, BindGroup*, std::span<const u32>) override {}
    void setPushConstants(ShaderStage, u32, u32, const void*) override {}
    void setVertexBuffer(u32, Buffer*, u64) override {}
    void setIndexBuffer(Buffer*, IndexFormat, u64) override {}
    void draw(u32, u32, u32, u32) override {}
    void drawIndexed(u32, u32, u32, i32, u32) override {}
    void drawIndirect(Buffer*, u64, u32, u32) override {}
    void drawIndexedIndirect(Buffer*, u64, u32, u32) override {}
    RenderBundle* finish() override { return &bundle; }
    NullRenderBundle bundle;
};

class NullComputePassEncoder : public ComputePassEncoder {
public:
    void setPipeline(ComputePipeline*) override {}
    void setBindGroup(u32, BindGroup*, std::span<const u32>) override {}
    void setPushConstants(ShaderStage, u32, u32, const void*) override {}
    void dispatch(u32, u32, u32) override {}
    void dispatchIndirect(Buffer*, u64) override {}
    void computeBarrier() override {}
    void writeTimestamp(QuerySet*, u32) override {}
    void end() override {}
};

class NullCommandEncoder : public CommandEncoder, public RayTracingEncoderExt {
public:
    RayTracingEncoderExt* asRayTracingExt() noexcept override { return this; }
    NullRenderPassEncoder rpe;
    NullComputePassEncoder cpe;
    NullRenderBundleEncoder rbe;
    NullCommandBuffer cb;

    RenderPassEncoder*  beginRenderPass(const RenderPassDesc&) override { return &rpe; }
    ComputePassEncoder* beginComputePass(std::u8string_view) override { return &cpe; }
    RenderBundleEncoder* createRenderBundleEncoder(const RenderBundleDesc&) override { return &rbe; }
    void barrier(const BarrierGroup&) override {}
    void copyBufferToBuffer(Buffer*, u64, Buffer*, u64, u64) override {}
    void copyBufferToTexture(Buffer*, Texture*, const BufferTextureCopyRegion&) override {}
    void copyTextureToBuffer(Texture*, Buffer*, const BufferTextureCopyRegion&) override {}
    void copyTextureToTexture(Texture*, Texture*, const TextureCopyRegion&) override {}
    void blit(Texture*, Texture*) override {}
    void generateMipmaps(Texture*) override {}
    void resolveTexture(Texture*, Texture*) override {}
    void resetQuerySet(QuerySet*, u32, u32) override {}
    void writeTimestamp(QuerySet*, u32) override {}
    void resolveQuerySet(QuerySet*, u32, u32, Buffer*, u64) override {}
    void beginDebugLabel(std::u8string_view, f32, f32, f32, f32) override {}
    void endDebugLabel() override {}
    void insertDebugLabel(std::u8string_view, f32, f32, f32, f32) override {}
    CommandBuffer* finish() override { return &cb; }

    // RayTracingEncoderExt
    void buildBottomLevelAccelStruct(AccelStruct*, Buffer*, u64, std::span<const AccelStructGeometryTriangles>, std::span<const AccelStructGeometryAABBs>) override {}
    void buildTopLevelAccelStruct(AccelStruct*, Buffer*, u64, Buffer*, u64, u32) override {}
    void setRayTracingPipeline(RayTracingPipeline*) override {}
    void setBindGroup(u32, BindGroup*, std::span<const u32>) override {}
    void setPushConstants(ShaderStage, u32, u32, const void*) override {}
    void traceRays(Buffer*, u64, u64, Buffer*, u64, u64, Buffer*, u64, u64, u32, u32, u32) override {}
};

class NullCommandPool : public CommandPool {
public:
    NullCommandEncoder enc;
    Status createEncoder(CommandEncoder*& out) override { out = &enc; return ErrorCode::Ok; }
    void   destroyEncoder(CommandEncoder*&) override {}
    void   reset() override {}
};

class NullTransferBatch : public TransferBatch {
public:
    void   writeBuffer(Buffer*, u64, std::span<const u8>) override {}
    void   writeTexture(Texture*, std::span<const u8>, const TextureDataLayout&, Extent3D, u32, u32) override {}
    Status submit() override { return ErrorCode::Ok; }
    Status submitAsync(Fence*, u64) override { return ErrorCode::Ok; }
    void   reset() override {}
    void   destroy() override {}
};

class NullSwapChain : public SwapChain {
public:
    NullTexture tex;
    NullTextureView view;
    TextureFormat m_format  = TextureFormat::BGRA8UnormSrgb;
    u32           m_width   = 0;
    u32           m_height  = 0;
    u32           m_count   = 2;
    u32           m_imgIdx  = 0;

    TextureFormat format()            const override { return m_format; }
    u32           width()             const override { return m_width; }
    u32           height()            const override { return m_height; }
    u32           bufferCount()       const override { return m_count; }
    u32           currentImageIndex() const override { return m_imgIdx; }
    Status        acquireNextImage()        override { m_imgIdx = (m_imgIdx + 1) % m_count; return ErrorCode::Ok; }
    Texture*      currentTexture()          override { return &tex; }
    TextureView*  currentTextureView()      override { return &view; }
    Status        present(Queue*)           override { return ErrorCode::Ok; }
    Status        resize(u32 w, u32 h)      override { m_width = w; m_height = h; return ErrorCode::Ok; }
};

class NullQueue : public Queue {
public:
    NullTransferBatch tb;
    void submit(std::span<CommandBuffer* const>) override {}
    void submit(std::span<CommandBuffer* const>, Fence* f, u64 v) override { if (auto* nf = static_cast<NullFence*>(f)) nf->signal(v); }
    void submit(std::span<CommandBuffer* const>, std::span<Fence* const>, std::span<const u64>, Fence* f, u64 v) override { if (auto* nf = static_cast<NullFence*>(f)) nf->signal(v); }
    void waitIdle() override {}
    Status createTransferBatch(TransferBatch*& out) override { out = &tb; return ErrorCode::Ok; }
    void destroyTransferBatch(TransferBatch*&) override {}
    f32 timestampPeriod() const override { return 1.0f; }
};

// ---- Null Device ----

class NullDevice : public Device {
public:
    NullQueue gfxQueue, compQueue, xferQueue;

    NullDevice() {
        type = DeviceType::Null;
        gfxQueue.queueType  = QueueType::Graphics;
        compQueue.queueType = QueueType::Compute;
        xferQueue.queueType = QueueType::Transfer;
    }

    Queue* getQueue(QueueType t, u32) override {
        switch (t) {
        case QueueType::Graphics: return &gfxQueue;
        case QueueType::Compute:  return &compQueue;
        case QueueType::Transfer: return &xferQueue;
        } return nullptr;
    }
    u32 getQueueCount(QueueType) override { return 1; }
    FormatSupport getFormatSupport(TextureFormat) override { return FormatSupport::Texture | FormatSupport::ColorAttachment | FormatSupport::DepthStencil; }

    Status createBuffer(const BufferDesc& d, Buffer*& out) override {
        auto* b = new NullBuffer(); b->desc = d; b->allocate(d.size); out = b; return ErrorCode::Ok;
    }
    Status createTexture(const TextureDesc& d, Texture*& out) override { auto* t = new NullTexture(); t->desc = d; out = t; return ErrorCode::Ok; }
    Status createTextureView(Texture* tex, const TextureViewDesc& d, TextureView*& out) override { auto* v = new NullTextureView(); v->desc = d; v->texture = tex; out = v; return ErrorCode::Ok; }
    Status createSampler(const SamplerDesc& d, Sampler*& out) override { auto* s = new NullSampler(); s->desc = d; out = s; return ErrorCode::Ok; }
    Status createShaderModule(const ShaderModuleDesc&, ShaderModule*& out) override { out = new NullShaderModule(); return ErrorCode::Ok; }
    Status createBindGroupLayout(const BindGroupLayoutDesc&, BindGroupLayout*& out) override { out = new NullBindGroupLayout(); return ErrorCode::Ok; }
    Status createBindGroup(const BindGroupDesc&, BindGroup*& out) override { out = new NullBindGroup(); return ErrorCode::Ok; }
    Status createPipelineLayout(const PipelineLayoutDesc&, PipelineLayout*& out) override { out = new NullPipelineLayout(); return ErrorCode::Ok; }
    Status createPipelineCache(const PipelineCacheDesc&, PipelineCache*& out) override { out = new NullPipelineCache(); return ErrorCode::Ok; }
    Status createRenderPipeline(const RenderPipelineDesc&, RenderPipeline*& out) override { out = new NullRenderPipeline(); return ErrorCode::Ok; }
    Status createComputePipeline(const ComputePipelineDesc&, ComputePipeline*& out) override { out = new NullComputePipeline(); return ErrorCode::Ok; }
    Status createCommandPool(QueueType, CommandPool*& out) override { out = new NullCommandPool(); return ErrorCode::Ok; }
    Status createFence(u64, Fence*& out) override { out = new NullFence(); return ErrorCode::Ok; }
    Status createQuerySet(const QuerySetDesc& d, QuerySet*& out) override { auto* q = new NullQuerySet(); q->type = d.type; q->count = d.count; out = q; return ErrorCode::Ok; }
    Status createSwapChain(Surface*, const SwapChainDesc& d, SwapChain*& out) override {
        auto* sc = new NullSwapChain(); sc->m_format = d.format; sc->m_width = d.width; sc->m_height = d.height; sc->m_count = d.bufferCount; out = sc; return ErrorCode::Ok;
    }

    void destroyBuffer(Buffer*& x)            override { delete x; x = nullptr; }
    void destroyTexture(Texture*& x)          override { delete x; x = nullptr; }
    void destroyTextureView(TextureView*& x)  override { delete x; x = nullptr; }
    void destroySampler(Sampler*& x)          override { delete x; x = nullptr; }
    void destroyShaderModule(ShaderModule*& x)override { delete x; x = nullptr; }
    void destroyBindGroupLayout(BindGroupLayout*& x) override { delete x; x = nullptr; }
    void destroyBindGroup(BindGroup*& x)      override { delete x; x = nullptr; }
    void destroyPipelineLayout(PipelineLayout*& x) override { delete x; x = nullptr; }
    void destroyPipelineCache(PipelineCache*& x) override { delete x; x = nullptr; }
    void destroyRenderPipeline(RenderPipeline*& x) override { delete x; x = nullptr; }
    void destroyComputePipeline(ComputePipeline*& x) override { delete x; x = nullptr; }
    void destroyCommandPool(CommandPool*& x)  override { delete x; x = nullptr; }
    void destroyFence(Fence*& x)              override { delete x; x = nullptr; }
    void destroyQuerySet(QuerySet*& x)        override { delete x; x = nullptr; }
    void destroySwapChain(SwapChain*& x)      override { delete x; x = nullptr; }
    void destroySurface(Surface*& x)          override { delete x; x = nullptr; }

    void waitIdle() override {}
    void destroy() override { delete this; }
};

// ---- Null Adapter ----

class NullAdapter : public Adapter {
public:
    void getInfo(AdapterInfo& out) override {
        out.name     = u8"Null Device";
        out.vendorId = 0;
        out.deviceId = 0;
        out.type     = AdapterType::Cpu;
    }
    Status createDevice(const DeviceDesc&, Device*& out) override {
        out = new NullDevice();
        return ErrorCode::Ok;
    }
};

// ---- Null Backend ----

class NullBackend : public Backend {
public:
    NullAdapter adapter;
    Adapter*    adapterPtr = &adapter;

    std::span<Adapter* const> enumerateAdapters() override {
        return std::span<Adapter* const>(&adapterPtr, 1);
    }

    Status createSurface(void*, void*, Surface*& out) override {
        out = new NullSurface();
        return ErrorCode::Ok;
    }

    void destroy() override { delete this; }
};

/// Creates a null backend for headless / GPU-less testing.
Status createNullBackend(Backend*& out) {
    auto* b = new NullBackend();
    b->isInitialized = true;
    out = b;
    return ErrorCode::Ok;
}

} // namespace draco::rhi::null
