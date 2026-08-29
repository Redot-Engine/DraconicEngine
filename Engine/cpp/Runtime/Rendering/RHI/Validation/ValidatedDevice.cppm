/// Validation wrapper for Device. Tracks all live resources for leak
/// detection, validates create/destroy parameters.

module;

#include <span>
#include <vector>

export module rhi.validation:validated_device;

import core.stdtypes;
import core.status;
import rhi;
import :validated_fence;
import :validated_swap_chain;
import :validated_command_pool;
import :validated_queue;

using namespace draco;

export namespace draco::rhi::validation {

class ValidatedDevice : public Device {
public:
    explicit ValidatedDevice(Device* inner) : m_inner(inner) {
        type     = inner->type;
        features = inner->features;
        shaderGroupHandleSize      = inner->shaderGroupHandleSize;
        shaderGroupHandleAlignment = inner->shaderGroupHandleAlignment;
        shaderGroupBaseAlignment   = inner->shaderGroupBaseAlignment;
    }

    // ---- Queues ----
    Queue* getQueue(QueueType t, u32 index) override {
        // Wrap on first access.
        Queue* raw = m_inner->getQueue(t, index);
        if (!raw) return nullptr;
        for (auto& w : m_queueWrappers) if (w.raw == raw) return w.validated;
        auto* vq = new ValidatedQueue(raw);
        m_queueWrappers.push_back({ raw, vq });
        return vq;
    }
    u32 getQueueCount(QueueType t) override { return m_inner->getQueueCount(t); }
    FormatSupport getFormatSupport(TextureFormat f) override { return m_inner->getFormatSupport(f); }

    // ---- Create methods (with validation + tracking) ----
#define V_CREATE(Type, method, desc_t) \
    Status method(const desc_t& d, Type*& out) override { \
        if (m_destroyed) { logError("[Validation] " #method ": device destroyed"); out = nullptr; return ErrorCode::Unknown; } \
        Status r = m_inner->method(d, out); \
        if (r == ErrorCode::Ok && out) m_live##Type##s.push_back(out); \
        return r; \
    }

    V_CREATE(Buffer, createBuffer, BufferDesc)
    V_CREATE(Texture, createTexture, TextureDesc)
    V_CREATE(Sampler, createSampler, SamplerDesc)
    V_CREATE(ShaderModule, createShaderModule, ShaderModuleDesc)
    V_CREATE(BindGroupLayout, createBindGroupLayout, BindGroupLayoutDesc)
    V_CREATE(BindGroup, createBindGroup, BindGroupDesc)
    V_CREATE(PipelineLayout, createPipelineLayout, PipelineLayoutDesc)
    V_CREATE(PipelineCache, createPipelineCache, PipelineCacheDesc)
    V_CREATE(RenderPipeline, createRenderPipeline, RenderPipelineDesc)
    V_CREATE(ComputePipeline, createComputePipeline, ComputePipelineDesc)
    V_CREATE(QuerySet, createQuerySet, QuerySetDesc)
#undef V_CREATE

    Status createTextureView(Texture* tex, const TextureViewDesc& d, TextureView*& out) override {
        if (m_destroyed) { logError("[Validation] createTextureView: device destroyed"); out = nullptr; return ErrorCode::Unknown; }
        if (!tex) { logError("[Validation] createTextureView: texture is null"); out = nullptr; return ErrorCode::Unknown; }
        Status r = m_inner->createTextureView(tex, d, out);
        if (r == ErrorCode::Ok && out) m_liveTextureViews.push_back(out);
        return r;
    }

    Status createCommandPool(QueueType qt, CommandPool*& out) override {
        if (m_destroyed) { logError("[Validation] createCommandPool: device destroyed"); out = nullptr; return ErrorCode::Unknown; }
        CommandPool* innerPool = nullptr;
        Status r = m_inner->createCommandPool(qt, innerPool);
        if (r != ErrorCode::Ok || !innerPool) { out = nullptr; return r; }
        out = new ValidatedCommandPool(innerPool);
        m_liveCommandPools.push_back(out);
        return ErrorCode::Ok;
    }

    Status createFence(u64 initialValue, Fence*& out) override {
        if (m_destroyed) { logError("[Validation] createFence: device destroyed"); out = nullptr; return ErrorCode::Unknown; }
        Fence* innerFence = nullptr;
        Status r = m_inner->createFence(initialValue, innerFence);
        if (r != ErrorCode::Ok || !innerFence) { out = nullptr; return r; }
        out = new ValidatedFence(innerFence);
        m_liveFences.push_back(out);
        return ErrorCode::Ok;
    }

    Status createSwapChain(Surface* surface, const SwapChainDesc& d, SwapChain*& out) override {
        if (m_destroyed) { logError("[Validation] createSwapChain: device destroyed"); out = nullptr; return ErrorCode::Unknown; }
        SwapChain* innerSc = nullptr;
        Status r = m_inner->createSwapChain(surface, d, innerSc);
        if (r != ErrorCode::Ok || !innerSc) { out = nullptr; return r; }
        out = new ValidatedSwapChain(innerSc);
        m_liveSwapChains.push_back(out);
        return ErrorCode::Ok;
    }

    // ---- Mesh/RT (forwarded, validated for destroyed state) ----
    Status createMeshPipeline(const MeshPipelineDesc& d, MeshPipeline*& out) override {
        if (m_destroyed) { out = nullptr; return ErrorCode::Unknown; }
        Status r = m_inner->createMeshPipeline(d, out);
        if (r == ErrorCode::Ok && out) m_liveMeshPipelines.push_back(out);
        return r;
    }
    void destroyMeshPipeline(MeshPipeline*& p) override { removeAndDestroy(m_liveMeshPipelines, p, [&](auto*& x){ m_inner->destroyMeshPipeline(x); }); }

    Status createAccelStruct(const AccelStructDesc& d, AccelStruct*& out) override {
        if (m_destroyed) { out = nullptr; return ErrorCode::Unknown; }
        Status r = m_inner->createAccelStruct(d, out);
        if (r == ErrorCode::Ok && out) m_liveAccelStructs.push_back(out);
        return r;
    }
    void destroyAccelStruct(AccelStruct*& a) override { removeAndDestroy(m_liveAccelStructs, a, [&](auto*& x){ m_inner->destroyAccelStruct(x); }); }

    Status createRayTracingPipeline(const RayTracingPipelineDesc& d, RayTracingPipeline*& out) override {
        if (m_destroyed) { out = nullptr; return ErrorCode::Unknown; }
        Status r = m_inner->createRayTracingPipeline(d, out);
        if (r == ErrorCode::Ok && out) m_liveRtPipelines.push_back(out);
        return r;
    }
    void destroyRayTracingPipeline(RayTracingPipeline*& p) override { removeAndDestroy(m_liveRtPipelines, p, [&](auto*& x){ m_inner->destroyRayTracingPipeline(x); }); }

    Status getShaderGroupHandles(RayTracingPipeline* p, u32 first, u32 count, std::span<u8> out) override {
        return m_inner->getShaderGroupHandles(p, first, count, out);
    }

    // ---- Destroy methods (with tracking removal) ----
#define V_DESTROY(Type, method, list) \
    void method(Type*& x) override { removeAndDestroy(list, x, [&](auto*& p){ m_inner->method(p); }); }

    V_DESTROY(Buffer, destroyBuffer, m_liveBuffers)
    V_DESTROY(Texture, destroyTexture, m_liveTextures)
    V_DESTROY(TextureView, destroyTextureView, m_liveTextureViews)
    V_DESTROY(Sampler, destroySampler, m_liveSamplers)
    V_DESTROY(ShaderModule, destroyShaderModule, m_liveShaderModules)
    V_DESTROY(BindGroupLayout, destroyBindGroupLayout, m_liveBindGroupLayouts)
    V_DESTROY(BindGroup, destroyBindGroup, m_liveBindGroups)
    V_DESTROY(PipelineLayout, destroyPipelineLayout, m_livePipelineLayouts)
    V_DESTROY(PipelineCache, destroyPipelineCache, m_livePipelineCaches)
    V_DESTROY(RenderPipeline, destroyRenderPipeline, m_liveRenderPipelines)
    V_DESTROY(ComputePipeline, destroyComputePipeline, m_liveComputePipelines)
    V_DESTROY(QuerySet, destroyQuerySet, m_liveQuerySets)
#undef V_DESTROY

    void destroyCommandPool(CommandPool*& pool) override {
        if (!pool) return;
        removeFromList(m_liveCommandPools, pool);
        auto* vp = static_cast<ValidatedCommandPool*>(pool);
        if (vp) { CommandPool* innerPool = vp->inner(); m_inner->destroyCommandPool(innerPool); delete vp; }
        else m_inner->destroyCommandPool(pool);
        pool = nullptr;
    }

    void destroyFence(Fence*& fence) override {
        if (!fence) return;
        removeFromList(m_liveFences, fence);
        auto* vf = static_cast<ValidatedFence*>(fence);
        if (vf) { Fence* innerFence = vf->inner(); m_inner->destroyFence(innerFence); delete vf; }
        else m_inner->destroyFence(fence);
        fence = nullptr;
    }

    void destroySwapChain(SwapChain*& sc) override {
        if (!sc) return;
        removeFromList(m_liveSwapChains, sc);
        auto* vs = static_cast<ValidatedSwapChain*>(sc);
        if (vs) { SwapChain* innerSc = vs->inner(); m_inner->destroySwapChain(innerSc); delete vs; }
        else m_inner->destroySwapChain(sc);
        sc = nullptr;
    }

    void destroySurface(Surface*& s) override { m_inner->destroySurface(s); }

    void waitIdle() override { m_inner->waitIdle(); }

    void destroy() override {
        if (m_destroyed) { logError("[Validation] Device::destroy: already destroyed"); return; }
        m_destroyed = true;
        reportLeaks();
        for (auto& w : m_queueWrappers) delete w.validated;
        m_queueWrappers.clear();
        m_inner->destroy();
        delete this;
    }

private:
    template <typename T>
    void removeFromList(std::vector<T*>& list, T* item) {
        for (usize i = 0; i < list.size(); ++i) {
            if (list[i] == item) { list.erase(list.begin() + i); return; }
        }
    }

    template <typename T, typename Fn>
    void removeAndDestroy(std::vector<T*>& list, T*& item, Fn destroyFn) {
        if (!item) return;
        removeFromList(list, item);
        destroyFn(item);
        item = nullptr;
    }

    void reportLeaks() {
        auto report = [](const char* name, usize count) {
            if (count > 0) logWarningf("[Validation] Device destroyed with %zu live %s(s)", count, name);
        };
        report("Buffer", m_liveBuffers.size());
        report("Texture", m_liveTextures.size());
        report("TextureView", m_liveTextureViews.size());
        report("Sampler", m_liveSamplers.size());
        report("ShaderModule", m_liveShaderModules.size());
        report("BindGroupLayout", m_liveBindGroupLayouts.size());
        report("BindGroup", m_liveBindGroups.size());
        report("PipelineLayout", m_livePipelineLayouts.size());
        report("PipelineCache", m_livePipelineCaches.size());
        report("RenderPipeline", m_liveRenderPipelines.size());
        report("ComputePipeline", m_liveComputePipelines.size());
        report("MeshPipeline", m_liveMeshPipelines.size());
        report("AccelStruct", m_liveAccelStructs.size());
        report("RayTracingPipeline", m_liveRtPipelines.size());
        report("CommandPool", m_liveCommandPools.size());
        report("Fence", m_liveFences.size());
        report("SwapChain", m_liveSwapChains.size());
        report("QuerySet", m_liveQuerySets.size());
    }

    Device* m_inner;
    bool    m_destroyed = false;

    struct QueueWrap { Queue* raw; ValidatedQueue* validated; };
    std::vector<QueueWrap> m_queueWrappers;

    std::vector<Buffer*>            m_liveBuffers;
    std::vector<Texture*>           m_liveTextures;
    std::vector<TextureView*>       m_liveTextureViews;
    std::vector<Sampler*>           m_liveSamplers;
    std::vector<ShaderModule*>      m_liveShaderModules;
    std::vector<BindGroupLayout*>   m_liveBindGroupLayouts;
    std::vector<BindGroup*>         m_liveBindGroups;
    std::vector<PipelineLayout*>    m_livePipelineLayouts;
    std::vector<PipelineCache*>     m_livePipelineCaches;
    std::vector<RenderPipeline*>    m_liveRenderPipelines;
    std::vector<ComputePipeline*>   m_liveComputePipelines;
    std::vector<MeshPipeline*>      m_liveMeshPipelines;
    std::vector<AccelStruct*>       m_liveAccelStructs;
    std::vector<RayTracingPipeline*>m_liveRtPipelines;
    std::vector<CommandPool*>       m_liveCommandPools;
    std::vector<Fence*>             m_liveFences;
    std::vector<SwapChain*>         m_liveSwapChains;
    std::vector<QuerySet*>          m_liveQuerySets;
};

} // namespace draco::rhi::validation
