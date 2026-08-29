/// DX12 implementation of Device.
/// Creates ID3D12Device, manages descriptor heaps, queues, command signatures,
/// and an internal blit pipeline for texture copy / mipmap generation.

module;

#include "DxIncludes.h"
#include <vector>
#include <span>
#include <string_view>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <unordered_map>

export module rhi.dx12:device;

import core.stdtypes;
import core.status;
import rhi;
import :conversions;
import :adapter;
import :surface;
import :descriptor_heap;
import :gpu_descriptor_heap;
import :buffer;
import :texture;
import :texture_view;
import :sampler;
import :shader_module;
import :fence;
import :query_set;
import :bind_group_layout;
import :bind_group;
import :pipeline_layout;
import :pipeline_cache;
import :render_pipeline;
import :compute_pipeline;
import :mesh_pipeline;
import :accel_struct;
import :ray_tracing_pipeline;
import :command_pool;
import :command_encoder;
import :render_pass_encoder;
import :compute_pass_encoder;
import :queue;
import :swap_chain;

using namespace draco;

export namespace draco::rhi::dx12 {

class DxDeviceImpl : public Device {
public:
    Status init(DxAdapterImpl* adapter, const DeviceDesc& desc) {
        m_adapter = adapter;

        // Create device at feature level 12.0.
        HRESULT hr = D3D12CreateDevice(
            adapter->handle(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_device));
        if (FAILED(hr)) {
            logErrorf("DxDevice: D3D12CreateDevice failed (0x%08X)", static_cast<unsigned>(hr));
            return ErrorCode::Unknown;
        }

        // Suppress noisy debug layer warnings.
        {
            ComPtr<ID3D12InfoQueue> infoQueue;
            if (SUCCEEDED(m_device->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
                D3D12_MESSAGE_ID suppressIds[] = {
                    D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
                    D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE,
                };
                D3D12_INFO_QUEUE_FILTER filter{};
                filter.DenyList.NumIDs  = static_cast<UINT>(std::size(suppressIds));
                filter.DenyList.pIDList = suppressIds;
                infoQueue->AddStorageFilterEntries(&filter);
                m_infoQueue = infoQueue;
            }
        }

        // --- Descriptor heap allocators (CPU-side, for staging) ---
        m_rtvHeap.init(m_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 256);
        m_dsvHeap.init(m_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 64);
        m_srvHeap.init(m_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 4096);
        m_samplerHeap.init(m_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 256);

        // --- GPU-visible descriptor heaps (shader-visible) ---
        m_gpuSrvHeap.init(m_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 65536, true);
        m_gpuSamplerHeap.init(m_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 2048, true);

        // --- CPU-visible descriptor heaps (non-shader-visible, bind groups write here) ---
        m_cpuSrvHeap.init(m_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 65536, false);
        m_cpuSamplerHeap.init(m_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 2048, false);

        // --- Create queues ---
        u32 graphicsCount = std::max(desc.graphicsQueueCount, 1u);
        for (u32 i = 0; i < graphicsCount; ++i) {
            auto* q = new DxQueueImpl();
            if (q->init(m_device.Get(), QueueType::Graphics, this) != ErrorCode::Ok) {
                delete q; break;
            }
            m_graphicsQueues.push_back(q);
        }
        for (u32 i = 0; i < desc.computeQueueCount; ++i) {
            auto* q = new DxQueueImpl();
            if (q->init(m_device.Get(), QueueType::Compute, this) != ErrorCode::Ok) {
                delete q; break;
            }
            m_computeQueues.push_back(q);
        }
        for (u32 i = 0; i < desc.transferQueueCount; ++i) {
            auto* q = new DxQueueImpl();
            if (q->init(m_device.Get(), QueueType::Transfer, this) != ErrorCode::Ok) {
                delete q; break;
            }
            m_transferQueues.push_back(q);
        }

        // --- Cached command signatures for indirect execution ---
        createIndirectCommandSignatures();

        // --- Internal blit pipeline ---
        createBlitPipeline();

        // --- Detect mesh shader & ray tracing support ---
        detectExtensionSupport();

        // --- Populate features ---
        type     = DeviceType::DX12;
        features = adapter->buildFeatures();

        // --- RT handle properties (DX12 constants) ---
        if (m_rtEnabled) {
            shaderGroupHandleSize      = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES; // 32
            shaderGroupHandleAlignment = D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT; // 32
            shaderGroupBaseAlignment   = D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT;  // 64
        }

        return ErrorCode::Ok;
    }

    // ==================================================================
    // Device interface -- Queues
    // ==================================================================

    Queue* getQueue(QueueType t, u32 index) override {
        switch (t) {
        case QueueType::Graphics: return index < m_graphicsQueues.size()  ? m_graphicsQueues[index]  : nullptr;
        case QueueType::Compute:  return index < m_computeQueues.size()   ? m_computeQueues[index]   : nullptr;
        case QueueType::Transfer: return index < m_transferQueues.size()  ? m_transferQueues[index]   : nullptr;
        }
        return nullptr;
    }

    u32 getQueueCount(QueueType t) override {
        switch (t) {
        case QueueType::Graphics: return static_cast<u32>(m_graphicsQueues.size());
        case QueueType::Compute:  return static_cast<u32>(m_computeQueues.size());
        case QueueType::Transfer: return static_cast<u32>(m_transferQueues.size());
        }
        return 0;
    }

    FormatSupport getFormatSupport(TextureFormat /*format*/) override {
        // DX12 supports D24_S8 on all hardware and most formats broadly.
        // A full implementation would call CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT).
        return FormatSupport::Texture | FormatSupport::ColorAttachment |
               FormatSupport::DepthStencil | FormatSupport::Buffer |
               FormatSupport::VertexBuffer | FormatSupport::BlendableColor |
               FormatSupport::LinearFilter;
    }

    // ==================================================================
    // Device interface -- Resource creation
    // ==================================================================

    Status createBuffer(const BufferDesc& d, Buffer*& out) override {
        auto* b = new DxBufferImpl();
        if (b->init(m_device.Get(), d) != ErrorCode::Ok) { delete b; out = nullptr; return ErrorCode::Unknown; }
        setDebugName(b->handle(), d.label);
        out = b;
        return ErrorCode::Ok;
    }

    Status createTexture(const TextureDesc& d, Texture*& out) override {
        auto* t = new DxTextureImpl();
        if (t->init(m_device.Get(), d) != ErrorCode::Ok) { delete t; out = nullptr; return ErrorCode::Unknown; }
        setDebugName(t->handle(), d.label);
        out = t;
        return ErrorCode::Ok;
    }

    Status createTextureView(Texture* tex, const TextureViewDesc& d, TextureView*& out) override {
        auto* dxTex = static_cast<DxTextureImpl*>(tex);
        if (!dxTex) {
            logError("DxDevice: cast to DxTextureImpl failed");
            out = nullptr;
            return ErrorCode::Unknown;
        }
        auto* v = new DxTextureViewImpl();
        if (v->init(m_device.Get(), dxTex, d, &m_srvHeap, &m_rtvHeap, &m_dsvHeap) != ErrorCode::Ok) {
            delete v; out = nullptr; return ErrorCode::Unknown;
        }
        out = v;
        return ErrorCode::Ok;
    }

    Status createSampler(const SamplerDesc& d, Sampler*& out) override {
        auto* s = new DxSamplerImpl();
        if (s->init(m_device.Get(), d, &m_samplerHeap) != ErrorCode::Ok) {
            delete s; out = nullptr; return ErrorCode::Unknown;
        }
        out = s;
        return ErrorCode::Ok;
    }

    Status createShaderModule(const ShaderModuleDesc& d, ShaderModule*& out) override {
        auto* m = new DxShaderModuleImpl();
        if (m->init(d) != ErrorCode::Ok) { delete m; out = nullptr; return ErrorCode::Unknown; }
        out = m;
        return ErrorCode::Ok;
    }

    // ==================================================================
    // Binding & Pipelines
    // ==================================================================

    Status createBindGroupLayout(const BindGroupLayoutDesc& d, BindGroupLayout*& out) override {
        auto* l = new DxBindGroupLayoutImpl();
        if (l->init(d) != ErrorCode::Ok) { delete l; out = nullptr; return ErrorCode::Unknown; }
        out = l;
        return ErrorCode::Ok;
    }

    Status createBindGroup(const BindGroupDesc& d, BindGroup*& out) override {
        auto* g = new DxBindGroupImpl();
        if (g->init(m_device.Get(), d, &m_cpuSrvHeap, &m_cpuSamplerHeap) != ErrorCode::Ok) {
            delete g; out = nullptr; return ErrorCode::Unknown;
        }
        out = g;
        return ErrorCode::Ok;
    }

    Status createPipelineLayout(const PipelineLayoutDesc& d, PipelineLayout*& out) override {
        auto* l = new DxPipelineLayoutImpl();
        if (l->init(m_device.Get(), d) != ErrorCode::Ok) {
            logError("DxDevice: createPipelineLayout failed");
            delete l; out = nullptr; return ErrorCode::Unknown;
        }
        setDebugName(l->handle(), d.label);
        out = l;
        return ErrorCode::Ok;
    }

    Status createPipelineCache(const PipelineCacheDesc& d, PipelineCache*& out) override {
        auto* c = new DxPipelineCacheImpl();
        if (c->init(m_device.Get(), d) != ErrorCode::Ok) { delete c; out = nullptr; return ErrorCode::Unknown; }
        if (c->handle()) setDebugName(c->handle(), d.label);
        out = c;
        return ErrorCode::Ok;
    }

    Status createRenderPipeline(const RenderPipelineDesc& d, RenderPipeline*& out) override {
        auto* p = new DxRenderPipelineImpl();
        if (p->init(m_device.Get(), d) != ErrorCode::Ok) { delete p; out = nullptr; return ErrorCode::Unknown; }
        setDebugName(p->handle(), d.label);
        out = p;
        return ErrorCode::Ok;
    }

    Status createComputePipeline(const ComputePipelineDesc& d, ComputePipeline*& out) override {
        auto* p = new DxComputePipelineImpl();
        if (p->init(m_device.Get(), d) != ErrorCode::Ok) { delete p; out = nullptr; return ErrorCode::Unknown; }
        setDebugName(p->handle(), d.label);
        out = p;
        return ErrorCode::Ok;
    }

    // ==================================================================
    // Mesh shader (folded in)
    // ==================================================================

    Status createMeshPipeline(const MeshPipelineDesc& d, MeshPipeline*& out) override {
        if (!m_meshEnabled) { out = nullptr; return ErrorCode::NotSupported; }
        auto* p = new DxMeshPipelineImpl();
        if (p->init(m_device.Get(), d) != ErrorCode::Ok) { delete p; out = nullptr; return ErrorCode::Unknown; }
        setDebugName(p->handle(), d.label);
        out = p;
        return ErrorCode::Ok;
    }

    void destroyMeshPipeline(MeshPipeline*& p) override {
        if (p) { static_cast<DxMeshPipelineImpl*>(p)->cleanup(); delete p; p = nullptr; }
    }

    // ==================================================================
    // Ray tracing (folded in)
    // ==================================================================

    Status createAccelStruct(const AccelStructDesc& d, AccelStruct*& out) override {
        if (!m_rtEnabled) { out = nullptr; return ErrorCode::NotSupported; }
        auto* a = new DxAccelStructImpl();
        if (a->init(m_device.Get(), d) != ErrorCode::Ok) { delete a; out = nullptr; return ErrorCode::Unknown; }
        out = a;
        return ErrorCode::Ok;
    }

    void destroyAccelStruct(AccelStruct*& a) override {
        if (a) { static_cast<DxAccelStructImpl*>(a)->cleanup(); delete a; a = nullptr; }
    }

    Status createRayTracingPipeline(const RayTracingPipelineDesc& d, RayTracingPipeline*& out) override {
        if (!m_rtEnabled) { out = nullptr; return ErrorCode::NotSupported; }
        auto* p = new DxRayTracingPipelineImpl();
        if (p->init(m_device.Get(), d) != ErrorCode::Ok) { delete p; out = nullptr; return ErrorCode::Unknown; }
        out = p;
        return ErrorCode::Ok;
    }

    void destroyRayTracingPipeline(RayTracingPipeline*& p) override {
        if (p) { static_cast<DxRayTracingPipelineImpl*>(p)->cleanup(); delete p; p = nullptr; }
    }

    Status getShaderGroupHandles(RayTracingPipeline* pipeline, u32 firstGroup,
                                  u32 groupCount, std::span<u8> outData) override {
        if (!m_rtEnabled) return ErrorCode::NotSupported;
        auto* dxPipeline = static_cast<DxRayTracingPipelineImpl*>(pipeline);
        if (!dxPipeline || !dxPipeline->properties()) {
            logError("DxDevice: pipeline or properties is null");
            return ErrorCode::Unknown;
        }

        constexpr u32 handleSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES; // 32
        if (outData.size() < static_cast<usize>(groupCount * handleSize)) {
            logError("DxDevice: output buffer too small for shader group handles");
            return ErrorCode::Unknown;
        }

        auto exportNames = dxPipeline->groupExportNames();
        for (u32 i = 0; i < groupCount; ++i) {
            u32 groupIdx = firstGroup + i;
            if (groupIdx >= exportNames.size()) {
                logError("DxDevice: shader group index out of range");
                return ErrorCode::Unknown;
            }
            const auto& exportName = exportNames[groupIdx];
            void* identifier = dxPipeline->properties()->GetShaderIdentifier(exportName.c_str());
            if (!identifier) {
                logError("DxDevice: GetShaderIdentifier returned null");
                return ErrorCode::Unknown;
            }
            std::memcpy(outData.data() + (i * handleSize), identifier, handleSize);
        }
        return ErrorCode::Ok;
    }

    // ==================================================================
    // Commands
    // ==================================================================

    Status createCommandPool(QueueType qt, CommandPool*& out) override {
        auto* p = new DxCommandPoolImpl();
        if (p->init(this, m_device.Get(), qt,
                    &m_cpuSrvHeap, &m_gpuSrvHeap,
                    &m_cpuSamplerHeap, &m_gpuSamplerHeap) != ErrorCode::Ok) {
            delete p; out = nullptr; return ErrorCode::Unknown;
        }
        out = p;
        return ErrorCode::Ok;
    }

    // ==================================================================
    // Synchronization
    // ==================================================================

    Status createFence(u64 initialValue, Fence*& out) override {
        auto* f = new DxFenceImpl();
        if (f->init(m_device.Get(), initialValue) != ErrorCode::Ok) { delete f; out = nullptr; return ErrorCode::Unknown; }
        out = f;
        return ErrorCode::Ok;
    }

    // ==================================================================
    // Queries
    // ==================================================================

    Status createQuerySet(const QuerySetDesc& d, QuerySet*& out) override {
        auto* q = new DxQuerySetImpl();
        if (q->init(m_device.Get(), d) != ErrorCode::Ok) { delete q; out = nullptr; return ErrorCode::Unknown; }
        setDebugName(q->handle(), d.label);
        out = q;
        return ErrorCode::Ok;
    }

    // ==================================================================
    // Presentation
    // ==================================================================

    Status createSwapChain(Surface* surface, const SwapChainDesc& d, SwapChain*& out) override {
        auto* dxSurface = static_cast<DxSurfaceImpl*>(surface);
        if (!dxSurface) {
            logError("DxDevice: cast to DxSurfaceImpl failed");
            out = nullptr;
            return ErrorCode::Unknown;
        }

        // Need a graphics queue for swap chain.
        if (m_graphicsQueues.empty()) { out = nullptr; return ErrorCode::Unknown; }

        auto* sc = new DxSwapChainImpl();
        if (sc->init(m_device.Get(), m_adapter->factory(),
                     m_graphicsQueues[0]->handle(),
                     dxSurface, d,
                     &m_srvHeap, &m_rtvHeap, &m_dsvHeap) != ErrorCode::Ok) {
            delete sc; out = nullptr; return ErrorCode::Unknown;
        }
        out = sc;
        return ErrorCode::Ok;
    }

    // ==================================================================
    // Resource destruction
    // ==================================================================

    void destroyBuffer(Buffer*& b)              override { if (b) { static_cast<DxBufferImpl*>(b)->cleanup(); delete b; b = nullptr; } }
    void destroyTexture(Texture*& t)            override { if (t) { static_cast<DxTextureImpl*>(t)->cleanup(); delete t; t = nullptr; } }
    void destroyTextureView(TextureView*& v)    override { if (v) { static_cast<DxTextureViewImpl*>(v)->cleanup(); delete v; v = nullptr; } }
    void destroySampler(Sampler*& s)            override { if (s) { static_cast<DxSamplerImpl*>(s)->cleanup(); delete s; s = nullptr; } }
    void destroyShaderModule(ShaderModule*& m)  override { if (m) { static_cast<DxShaderModuleImpl*>(m)->cleanup(); delete m; m = nullptr; } }
    void destroyBindGroupLayout(BindGroupLayout*& l) override { if (l) { delete l; l = nullptr; } }
    void destroyBindGroup(BindGroup*& g)        override { if (g) { static_cast<DxBindGroupImpl*>(g)->cleanup(); delete g; g = nullptr; } }
    void destroyPipelineLayout(PipelineLayout*& l) override { if (l) { static_cast<DxPipelineLayoutImpl*>(l)->cleanup(); delete l; l = nullptr; } }
    void destroyPipelineCache(PipelineCache*& c) override { if (c) { static_cast<DxPipelineCacheImpl*>(c)->cleanup(); delete c; c = nullptr; } }
    void destroyRenderPipeline(RenderPipeline*& p) override { if (p) { static_cast<DxRenderPipelineImpl*>(p)->cleanup(); delete p; p = nullptr; } }
    void destroyComputePipeline(ComputePipeline*& p) override { if (p) { static_cast<DxComputePipelineImpl*>(p)->cleanup(); delete p; p = nullptr; } }
    void destroyCommandPool(CommandPool*& p)    override { if (p) { static_cast<DxCommandPoolImpl*>(p)->cleanup(); delete p; p = nullptr; } }
    void destroyFence(Fence*& f)                override { if (f) { static_cast<DxFenceImpl*>(f)->cleanup(); delete f; f = nullptr; } }
    void destroyQuerySet(QuerySet*& q)          override { if (q) { static_cast<DxQuerySetImpl*>(q)->cleanup(); delete q; q = nullptr; } }
    void destroySwapChain(SwapChain*& sc)       override { if (sc) { static_cast<DxSwapChainImpl*>(sc)->cleanup(); delete sc; sc = nullptr; } }
    void destroySurface(Surface*& s)            override { if (s) { delete s; s = nullptr; } }

    // ==================================================================
    // Lifecycle
    // ==================================================================

    void waitIdle() override {
        for (auto* q : m_graphicsQueues) q->waitIdle();
        for (auto* q : m_computeQueues)  q->waitIdle();
        for (auto* q : m_transferQueues) q->waitIdle();
        drainDebugMessages();
    }

    void drainDebugMessages() {
        if (!m_infoQueue) return;
        UINT64 count = m_infoQueue->GetNumStoredMessages();
        for (UINT64 i = 0; i < count; ++i) {
            SIZE_T len = 0;
            m_infoQueue->GetMessage(i, nullptr, &len);
            if (len == 0) continue;
            auto* msg = static_cast<D3D12_MESSAGE*>(std::malloc(len));
            if (m_infoQueue->GetMessage(i, msg, &len) == S_OK) {
                if (msg->Severity <= D3D12_MESSAGE_SEVERITY_WARNING)
                    std::fprintf(stderr, "[DX12 %s] %.*s\n",
                        msg->Severity == D3D12_MESSAGE_SEVERITY_ERROR ? "ERROR" :
                        msg->Severity == D3D12_MESSAGE_SEVERITY_WARNING ? "WARN" : "CORRUPT",
                        static_cast<int>(msg->DescriptionByteLength), msg->pDescription);
            }
            std::free(msg);
        }
        m_infoQueue->ClearStoredMessages();
    }

    void destroy() override {
        waitIdle();

        // Queues.
        for (auto* q : m_graphicsQueues) { q->cleanup(); delete q; }
        for (auto* q : m_computeQueues)  { q->cleanup(); delete q; }
        for (auto* q : m_transferQueues) { q->cleanup(); delete q; }
        m_graphicsQueues.clear();
        m_computeQueues.clear();
        m_transferQueues.clear();

        // Blit pipeline.
        for (auto& [fmt, pso] : m_blitPsoCache)
            pso.Reset();
        m_blitPsoCache.clear();
        m_blitVsBlob.Reset();
        m_blitPsBlob.Reset();
        m_blitRootSignature.Reset();

        // Command signatures.
        m_drawSignature.Reset();
        m_drawIndexedSignature.Reset();
        m_dispatchSignature.Reset();
        m_dispatchMeshSignature.Reset();

        // Descriptor heaps.
        m_cpuSrvHeap.destroy();
        m_cpuSamplerHeap.destroy();
        m_gpuSrvHeap.destroy();
        m_gpuSamplerHeap.destroy();
        m_rtvHeap.destroy();
        m_dsvHeap.destroy();
        m_srvHeap.destroy();
        m_samplerHeap.destroy();

        // Report live objects in debug builds.
#ifdef _DEBUG
        {
            ComPtr<ID3D12DebugDevice> debugDevice;
            if (SUCCEEDED(m_device->QueryInterface(IID_PPV_ARGS(&debugDevice)))) {
                debugDevice->ReportLiveDeviceObjects(
                    static_cast<D3D12_RLDO_FLAGS>(D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL));
            }
        }
#endif

        m_device.Reset();
        delete this;
    }

    // ==================================================================
    // Internal accessors (encoders, swap chain, etc. need these)
    // ==================================================================

    [[nodiscard]] ID3D12Device* handle() const { return m_device.Get(); }
    [[nodiscard]] DxAdapterImpl* adapter() const { return m_adapter; }

    [[nodiscard]] DxDescriptorHeapAllocator* rtvHeap()     { return &m_rtvHeap; }
    [[nodiscard]] DxDescriptorHeapAllocator* dsvHeap()     { return &m_dsvHeap; }
    [[nodiscard]] DxDescriptorHeapAllocator* srvHeap()     { return &m_srvHeap; }
    [[nodiscard]] DxDescriptorHeapAllocator* samplerHeap() { return &m_samplerHeap; }

    [[nodiscard]] DxGpuDescriptorHeap* gpuSrvHeap()     { return &m_gpuSrvHeap; }
    [[nodiscard]] DxGpuDescriptorHeap* gpuSamplerHeap() { return &m_gpuSamplerHeap; }
    [[nodiscard]] DxGpuDescriptorHeap* cpuSrvHeap()     { return &m_cpuSrvHeap; }
    [[nodiscard]] DxGpuDescriptorHeap* cpuSamplerHeap() { return &m_cpuSamplerHeap; }

    [[nodiscard]] ID3D12CommandSignature* drawSignature()        const { return m_drawSignature.Get(); }
    [[nodiscard]] ID3D12CommandSignature* drawIndexedSignature()  const { return m_drawIndexedSignature.Get(); }
    [[nodiscard]] ID3D12CommandSignature* dispatchSignature()     const { return m_dispatchSignature.Get(); }
    [[nodiscard]] ID3D12CommandSignature* dispatchMeshSignature() const { return m_dispatchMeshSignature.Get(); }
    [[nodiscard]] ID3D12RootSignature*    blitRootSignature()    const { return m_blitRootSignature.Get(); }

    [[nodiscard]] bool meshEnabled() const { return m_meshEnabled; }
    [[nodiscard]] bool rtEnabled()   const { return m_rtEnabled; }

    /// Gets or creates a blit PSO for the given render target format.
    ID3D12PipelineState* getOrCreateBlitPSO(DXGI_FORMAT format) {
        if (!m_blitRootSignature) return nullptr;

        std::lock_guard lock(m_blitMutex);

        auto it = m_blitPsoCache.find(format);
        if (it != m_blitPsoCache.end()) return it->second.Get();

        D3D12_GRAPHICS_PIPELINE_STATE_DESC psd{};
        psd.pRootSignature = m_blitRootSignature.Get();
        psd.VS = m_blitVsBytecode;
        psd.PS = m_blitPsBytecode;
        psd.InputLayout = { nullptr, 0 };
        psd.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psd.RasterizerState.FillMode         = D3D12_FILL_MODE_SOLID;
        psd.RasterizerState.CullMode         = D3D12_CULL_MODE_NONE;
        psd.RasterizerState.DepthClipEnable  = FALSE;
        psd.BlendState.RenderTarget[0].BlendEnable          = FALSE;
        psd.BlendState.RenderTarget[0].RenderTargetWriteMask = 0x0F;
        psd.DepthStencilState.DepthEnable   = FALSE;
        psd.DepthStencilState.StencilEnable = FALSE;
        psd.DSVFormat        = DXGI_FORMAT_UNKNOWN;
        psd.NumRenderTargets = 1;
        psd.RTVFormats[0]    = format;
        psd.SampleDesc.Count = 1;
        psd.SampleMask       = UINT_MAX;

        ComPtr<ID3D12PipelineState> newPso;
        if (SUCCEEDED(m_device->CreateGraphicsPipelineState(&psd, IID_PPV_ARGS(&newPso)))) {
            auto* raw = newPso.Get();
            m_blitPsoCache[format] = std::move(newPso);
            return raw;
        }
        return nullptr;
    }

    /// Sets a debug name on a DX12 object (visible in PIX, VS Graphics Debugger, etc.).
    /// Works with any type that inherits from ID3D12Object (Resource, PSO, QueryHeap, etc.).
    template<typename T>
    static void setDebugName(T* obj, std::u8string_view name) {
        if (!obj || name.empty()) return;
        // Convert narrow to wide.
        std::wstring wide;
        wide.reserve(name.size());
        for (usize i = 0; i < name.size(); ++i)
            wide.push_back(static_cast<wchar_t>(name[i]));
        obj->SetName(wide.c_str());
    }

private:
    // ------------------------------------------------------------------
    // Indirect command signatures
    // ------------------------------------------------------------------

    void createIndirectCommandSignatures() {
        D3D12_INDIRECT_ARGUMENT_DESC argDesc{};
        D3D12_COMMAND_SIGNATURE_DESC sigDesc{};
        sigDesc.NumArgumentDescs = 1;
        sigDesc.pArgumentDescs   = &argDesc;
        sigDesc.NodeMask         = 0;

        // Draw.
        argDesc.Type        = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
        sigDesc.ByteStride  = 16; // sizeof(D3D12_DRAW_ARGUMENTS): 4 x uint32
        m_device->CreateCommandSignature(&sigDesc, nullptr, IID_PPV_ARGS(&m_drawSignature));

        // DrawIndexed.
        argDesc.Type        = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
        sigDesc.ByteStride  = 20; // sizeof(D3D12_DRAW_INDEXED_ARGUMENTS): 5 x uint32
        m_device->CreateCommandSignature(&sigDesc, nullptr, IID_PPV_ARGS(&m_drawIndexedSignature));

        // Dispatch.
        argDesc.Type        = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
        sigDesc.ByteStride  = 12; // sizeof(D3D12_DISPATCH_ARGUMENTS): 3 x uint32
        m_device->CreateCommandSignature(&sigDesc, nullptr, IID_PPV_ARGS(&m_dispatchSignature));
    }

    // ------------------------------------------------------------------
    // Extension support detection (mesh shader, ray tracing)
    // ------------------------------------------------------------------

    void detectExtensionSupport() {
        // Mesh shaders -- requires D3D12_OPTIONS7.
        D3D12_FEATURE_DATA_D3D12_OPTIONS7 options7{};
        HRESULT hr = m_device->CheckFeatureSupport(
            D3D12_FEATURE_D3D12_OPTIONS7, &options7, sizeof(options7));
        if (SUCCEEDED(hr) && options7.MeshShaderTier != D3D12_MESH_SHADER_TIER_NOT_SUPPORTED) {
            m_meshEnabled = true;

            // DispatchMesh command signature.
            D3D12_INDIRECT_ARGUMENT_DESC argDesc{};
            argDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_MESH;
            D3D12_COMMAND_SIGNATURE_DESC sigDesc{};
            sigDesc.ByteStride       = 12; // sizeof(D3D12_DISPATCH_MESH_ARGUMENTS): 3 x uint32
            sigDesc.NumArgumentDescs = 1;
            sigDesc.pArgumentDescs   = &argDesc;
            sigDesc.NodeMask         = 0;
            m_device->CreateCommandSignature(&sigDesc, nullptr, IID_PPV_ARGS(&m_dispatchMeshSignature));
        }

        // Ray tracing -- requires D3D12_OPTIONS5.
        D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5{};
        hr = m_device->CheckFeatureSupport(
            D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5));
        if (SUCCEEDED(hr) && options5.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED) {
            m_rtEnabled = true;
        }
    }

    // ------------------------------------------------------------------
    // Internal blit pipeline (fullscreen triangle VS + texture sample PS)
    // ------------------------------------------------------------------

    void createBlitPipeline() {
        // TODO: Blit pipeline requires D3DCompile from d3dcompiler.lib.
        // Add d3dcompiler to target_link_libraries and uncomment the code below
        // once d3dcompiler linkage is available in this project.
        //
        // The blit pipeline is used for Blit and GenerateMipmaps operations.
        const char vsSource[] = R"(
            struct VSOutput {
                float4 Position : SV_Position;
                float2 UV : TEXCOORD0;
            };
            VSOutput main(uint vertexId : SV_VertexID) {
                VSOutput output;
                output.UV = float2((vertexId << 1) & 2, vertexId & 2);
                output.Position = float4(output.UV * float2(2, -2) + float2(-1, 1), 0, 1);
                return output;
            }
        )";

        const char psSource[] = R"(
            Texture2D srcTexture : register(t0);
            SamplerState srcSampler : register(s0);
            float4 main(float4 pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target {
                return srcTexture.Sample(srcSampler, uv);
            }
        )";

        ComPtr<ID3DBlob> errorBlob;

        // Compile VS.
        HRESULT hr = D3DCompile(vsSource, sizeof(vsSource) - 1, nullptr, nullptr, nullptr,
                                "main", "vs_5_0", 0, 0, &m_blitVsBlob, &errorBlob);
        if (FAILED(hr)) {
            if (errorBlob) logErrorf("DxDevice: blit VS compile error: %s",
                                      static_cast<const char*>(errorBlob->GetBufferPointer()));
            return;
        }
        errorBlob.Reset();

        // Compile PS.
        hr = D3DCompile(psSource, sizeof(psSource) - 1, nullptr, nullptr, nullptr,
                        "main", "ps_5_0", 0, 0, &m_blitPsBlob, &errorBlob);
        if (FAILED(hr)) {
            if (errorBlob) logErrorf("DxDevice: blit PS compile error: %s",
                                      static_cast<const char*>(errorBlob->GetBufferPointer()));
            m_blitVsBlob.Reset();
            return;
        }
        errorBlob.Reset();

        m_blitVsBytecode = { m_blitVsBlob->GetBufferPointer(), m_blitVsBlob->GetBufferSize() };
        m_blitPsBytecode = { m_blitPsBlob->GetBufferPointer(), m_blitPsBlob->GetBufferSize() };

        // Root signature: 1 SRV descriptor table (t0) + 1 static linear sampler (s0).
        D3D12_DESCRIPTOR_RANGE srvRange{};
        srvRange.RangeType                         = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        srvRange.NumDescriptors                    = 1;
        srvRange.BaseShaderRegister                = 0;
        srvRange.RegisterSpace                     = 0;
        srvRange.OffsetInDescriptorsFromTableStart = 0;

        D3D12_ROOT_PARAMETER rootParam{};
        rootParam.ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParam.ShaderVisibility                    = D3D12_SHADER_VISIBILITY_PIXEL;
        rootParam.DescriptorTable.NumDescriptorRanges = 1;
        rootParam.DescriptorTable.pDescriptorRanges   = &srvRange;

        D3D12_STATIC_SAMPLER_DESC staticSampler{};
        staticSampler.Filter           = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        staticSampler.AddressU         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        staticSampler.AddressV         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        staticSampler.AddressW         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        staticSampler.MaxAnisotropy    = 1;
        staticSampler.ComparisonFunc   = D3D12_COMPARISON_FUNC_NEVER;
        staticSampler.MinLOD           = 0;
        staticSampler.MaxLOD           = D3D12_FLOAT32_MAX;
        staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_ROOT_SIGNATURE_DESC rsDesc{};
        rsDesc.NumParameters     = 1;
        rsDesc.pParameters       = &rootParam;
        rsDesc.NumStaticSamplers = 1;
        rsDesc.pStaticSamplers   = &staticSampler;

        ComPtr<ID3DBlob> signatureBlob;
        hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1,
                                          &signatureBlob, &errorBlob);
        if (FAILED(hr)) {
            if (errorBlob) logErrorf("DxDevice: blit root sig serialize error: %s",
                                      static_cast<const char*>(errorBlob->GetBufferPointer()));
            return;
        }

        m_device->CreateRootSignature(0, signatureBlob->GetBufferPointer(),
                                     signatureBlob->GetBufferSize(),
                                     IID_PPV_ARGS(&m_blitRootSignature));
    }

    // ------------------------------------------------------------------
    // Member data
    // ------------------------------------------------------------------

    ComPtr<ID3D12Device> m_device;
    ComPtr<ID3D12InfoQueue> m_infoQueue;
    DxAdapterImpl*       m_adapter = nullptr;

    // Queues.
    std::vector<DxQueueImpl*> m_graphicsQueues;
    std::vector<DxQueueImpl*> m_computeQueues;
    std::vector<DxQueueImpl*> m_transferQueues;

    // Descriptor heap allocators (CPU-side for staging).
    DxDescriptorHeapAllocator m_rtvHeap;
    DxDescriptorHeapAllocator m_dsvHeap;
    DxDescriptorHeapAllocator m_srvHeap;
    DxDescriptorHeapAllocator m_samplerHeap;

    // GPU-visible descriptor heaps (shader-visible, for command buffer binding).
    DxGpuDescriptorHeap m_gpuSrvHeap;
    DxGpuDescriptorHeap m_gpuSamplerHeap;

    // CPU-visible descriptor heaps (non-shader-visible, bind groups write here).
    DxGpuDescriptorHeap m_cpuSrvHeap;
    DxGpuDescriptorHeap m_cpuSamplerHeap;

    // Cached command signatures for indirect execution.
    ComPtr<ID3D12CommandSignature> m_drawSignature;
    ComPtr<ID3D12CommandSignature> m_drawIndexedSignature;
    ComPtr<ID3D12CommandSignature> m_dispatchSignature;
    ComPtr<ID3D12CommandSignature> m_dispatchMeshSignature;

    // Internal blit pipeline.
    ComPtr<ID3D12RootSignature>    m_blitRootSignature;
    D3D12_SHADER_BYTECODE          m_blitVsBytecode{};
    D3D12_SHADER_BYTECODE          m_blitPsBytecode{};
    ComPtr<ID3DBlob>               m_blitVsBlob;
    ComPtr<ID3DBlob>               m_blitPsBlob;
    std::unordered_map<DXGI_FORMAT, ComPtr<ID3D12PipelineState>> m_blitPsoCache;
    std::mutex                     m_blitMutex;

    // Extension flags.
    bool m_meshEnabled = false;
    bool m_rtEnabled   = false;
};

// ==================================================================
// Adapter::CreateDevice implementation
// ==================================================================

Status DxAdapterImpl::createDevice(const DeviceDesc& desc, Device*& out) {
    auto* dev = new DxDeviceImpl();
    if (dev->init(this, desc) != ErrorCode::Ok) {
        delete dev; out = nullptr; return ErrorCode::Unknown;
    }
    out = dev;
    return ErrorCode::Ok;
}

// ---- CommandEncoder out-of-line: blitSubresource (needs DxDeviceImpl) ----

void DxCommandEncoderImpl::blitSubresource(DxTextureImpl* srcTex, u32 srcMip,
    DxTextureImpl* dstTex, u32 dstMip, u32 dstWidth, u32 dstHeight, DXGI_FORMAT dxgiFormat) {

    auto* blitRootSig = m_device->blitRootSignature();
    if (!blitRootSig) return;
    auto* blitPso = m_device->getOrCreateBlitPSO(dxgiFormat);
    if (!blitPso) return;

    // Allocate temp RTV for destination mip.
    auto rtvHandle = m_device->rtvHeap()->allocate();

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
    rtvDesc.Format = dxgiFormat;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Texture2D.MipSlice = dstMip;
    m_device->handle()->CreateRenderTargetView(dstTex->handle(), &rtvDesc, rtvHandle);

    // Allocate temp SRV in CPU heap, write, then stage-copy to GPU heap.
    i32 tempSrvOff = m_device->cpuSrvHeap()->allocate(1);
    if (tempSrvOff < 0) { m_device->rtvHeap()->free(rtvHandle); return; }

    auto tempCpuHandle = m_device->cpuSrvHeap()->getCpuHandle(static_cast<u32>(tempSrvOff));

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = dxgiFormat;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MostDetailedMip = srcMip;
    srvDesc.Texture2D.MipLevels = 1;
    m_device->handle()->CreateShaderResourceView(srcTex->handle(), &srvDesc, tempCpuHandle);

    // Copy from CPU heap into GPU staging, then free CPU temp slot.
    i32 stagedOff = m_pool->srvStaging()->copyFrom(static_cast<u32>(tempSrvOff), 1);
    m_device->cpuSrvHeap()->free(static_cast<u32>(tempSrvOff), 1);
    if (stagedOff < 0) { m_device->rtvHeap()->free(rtvHandle); return; }

    auto srvGpuHandle = m_device->gpuSrvHeap()->getGpuHandle(static_cast<u32>(stagedOff));

    ensureDescriptorHeaps();

    // Set blit pipeline.
    m_cmdList->SetGraphicsRootSignature(blitRootSig);
    m_cmdList->SetPipelineState(blitPso);
    m_cmdList->SetGraphicsRootDescriptorTable(0, srvGpuHandle);
    m_cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    D3D12_VIEWPORT vp{}; vp.Width = static_cast<FLOAT>(dstWidth); vp.Height = static_cast<FLOAT>(dstHeight); vp.MaxDepth = 1.0f;
    m_cmdList->RSSetViewports(1, &vp);
    D3D12_RECT sc{}; sc.right = static_cast<LONG>(dstWidth); sc.bottom = static_cast<LONG>(dstHeight);
    m_cmdList->RSSetScissorRects(1, &sc);

    m_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_cmdList->DrawInstanced(3, 1, 0, 0);

    m_device->rtvHeap()->free(rtvHandle);
}

// ---- CommandPool out-of-line methods (need DxCommandEncoderImpl + context structs) ----

Status DxCommandPoolImpl::createEncoder(CommandEncoder*& out) {
    out = nullptr;

    ComPtr<ID3D12Device> d3dDev;
    m_allocator->GetDevice(IID_PPV_ARGS(&d3dDev));
    if (!d3dDev) return ErrorCode::Unknown;

    ID3D12GraphicsCommandList* cmdList = nullptr;
    HRESULT hr = d3dDev->CreateCommandList(0, m_type,
        m_allocator.Get(), nullptr, IID_PPV_ARGS(&cmdList));
    if (FAILED(hr)) return ErrorCode::Unknown;

    DxRenderPassContext rpeCtx{};
    rpeCtx.cmdList         = cmdList;
    rpeCtx.srvStaging      = &m_srvStaging;
    rpeCtx.samplerStaging  = &m_samplerStaging;
    rpeCtx.gpuSrvHeap      = m_device->gpuSrvHeap();
    rpeCtx.gpuSamplerHeap  = m_device->gpuSamplerHeap();
    rpeCtx.drawSig         = m_device->drawSignature();
    rpeCtx.drawIndexedSig  = m_device->drawIndexedSignature();
    rpeCtx.dispatchMeshSig = m_device->dispatchMeshSignature();

    DxComputePassContext cpeCtx{};
    cpeCtx.cmdList         = cmdList;
    cpeCtx.srvStaging      = &m_srvStaging;
    cpeCtx.samplerStaging  = &m_samplerStaging;
    cpeCtx.gpuSrvHeap      = m_device->gpuSrvHeap();
    cpeCtx.gpuSamplerHeap  = m_device->gpuSamplerHeap();
    cpeCtx.dispatchSig     = m_device->dispatchSignature();

    auto* enc = new DxCommandEncoderImpl(m_device, cmdList, this, rpeCtx, cpeCtx);
    out = enc;
    return ErrorCode::Ok;
}

void DxCommandPoolImpl::destroyEncoder(CommandEncoder*& encoder) {
    if (auto* dx = static_cast<DxCommandEncoderImpl*>(encoder)) delete dx;
    encoder = nullptr;
}

} // namespace draco::rhi::dx12
