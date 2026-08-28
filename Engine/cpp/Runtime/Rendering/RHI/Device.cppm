/// Abstract Backend, Adapter, and Device interfaces.
///
/// Backend is the entry point - it enumerates GPU adapters and creates
/// presentation surfaces. Adapter represents a physical GPU. Device is
/// the central factory for all GPU resources.
///
/// Mesh shader and ray tracing creation methods are on Device directly
/// (virtual no-ops returning ErrorCode::NotSupported when not supported).
/// Callers check device->features.meshShaders / .rayTracing before use.

module;

#include <span>
#include <vector>

export module rhi:device;

import core.stdtypes;
import core.status;
import :enums;
import :texture_format;
import :types;
import :descriptors;
import :ext_descriptors;
import :resources;
import :commands;
import :queue;
import :swapchain;

using namespace draco;

export namespace draco::rhi {

// ---- Backend ----

/// RHI backend entry point (Vulkan, DX12, etc.).
class Backend {
public:
    virtual ~Backend() = default;

    bool isInitialized = false;

    /// Returns all available GPU adapters in preference order, most
    /// preferred first (discrete > integrated > unknown > CPU). Backends
    /// guarantee this ordering via sortAdaptersByPreference(), so callers
    /// that just want "the best available GPU" can take element [0].
    [[nodiscard]] virtual std::span<Adapter* const> enumerateAdapters() = 0;

    /// Creates a presentation surface from a native window handle.
    /// On Win32: windowHandle = HWND, displayHandle = nullptr.
    /// On X11: windowHandle = XID (as void*), displayHandle = Display*.
    /// On Wayland: windowHandle = wl_surface*, displayHandle = wl_display*.
    virtual Status createSurface(void* windowHandle, void* displayHandle, Surface*& out) = 0;

    Status createSurface(void* windowHandle, Surface*& out) {
        return createSurface(windowHandle, nullptr, out);
    }

    /// Destroy the backend and all objects it owns.
    virtual void destroy() = 0;
};

// ---- Adapter ----

/// Represents a physical GPU. Query capabilities and create a logical device.
class Adapter {
public:
    virtual ~Adapter() = default;

    /// Populate adapter info (name, vendor, features, limits).
    virtual void getInfo(AdapterInfo& out) = 0;

    /// Convenience: returns a copy of the adapter info.
    [[nodiscard]] AdapterInfo info() { AdapterInfo i; getInfo(i); return i; }

    /// Create a logical device from this adapter.
    virtual Status createDevice(const DeviceDesc& desc, Device*& out) = 0;
};

/// Selection preference for an adapter type - lower is more preferred.
/// Defines the single source of truth for "best GPU first" ordering.
[[nodiscard]] inline int adapterPreferenceRank(AdapterType type) {
    switch (type) {
        case AdapterType::DiscreteGpu:   return 0;
        case AdapterType::IntegratedGpu: return 1;
        case AdapterType::Unknown:       return 2;
        case AdapterType::Cpu:           return 3;
    }
    return 4;
}

/// Reorder adapters so the most preferred GPU is first (see
/// adapterPreferenceRank). Backends call this after enumeration so that
/// enumerateAdapters()[0] is the recommended default. The sort is stable,
/// preserving the driver's native order among adapters of equal type.
inline void sortAdaptersByPreference(std::vector<Adapter*>& adapters) {
    // Stable insertion sort by preference rank (adapter counts are tiny).
    for (usize i = 1; i < adapters.size(); ++i) {
        Adapter* key = adapters[i];
        const int keyRank = adapterPreferenceRank(key->info().type);
        usize j = i;
        while (j > 0 && adapterPreferenceRank(adapters[j - 1]->info().type) > keyRank) {
            adapters[j] = adapters[j - 1];
            --j;
        }
        adapters[j] = key;
    }
}

// ---- Device ----

/// Central factory for GPU resources, pipelines, and command infrastructure.
class Device {
public:
    virtual ~Device() = default;

    DeviceType     type     = DeviceType::Null;
    DeviceFeatures features{};

    // ---- Queries ----
    [[nodiscard]] virtual Queue* getQueue(QueueType type, u32 index = 0) = 0;
    [[nodiscard]] virtual u32    getQueueCount(QueueType type) = 0;
    /// Query hardware format support for a given texture format.
    [[nodiscard]] virtual FormatSupport getFormatSupport(TextureFormat format) = 0;

    // ---- Resource creation ----
    virtual Status createBuffer(const BufferDesc& desc, Buffer*& out) = 0;
    virtual Status createTexture(const TextureDesc& desc, Texture*& out) = 0;
    virtual Status createTextureView(Texture* texture, const TextureViewDesc& desc, TextureView*& out) = 0;
    virtual Status createSampler(const SamplerDesc& desc, Sampler*& out) = 0;
    virtual Status createShaderModule(const ShaderModuleDesc& desc, ShaderModule*& out) = 0;
    virtual Status createBindGroupLayout(const BindGroupLayoutDesc& desc, BindGroupLayout*& out) = 0;
    virtual Status createBindGroup(const BindGroupDesc& desc, BindGroup*& out) = 0;
    virtual Status createPipelineLayout(const PipelineLayoutDesc& desc, PipelineLayout*& out) = 0;
    virtual Status createPipelineCache(const PipelineCacheDesc& desc, PipelineCache*& out) = 0;
    virtual Status createRenderPipeline(const RenderPipelineDesc& desc, RenderPipeline*& out) = 0;
    virtual Status createComputePipeline(const ComputePipelineDesc& desc, ComputePipeline*& out) = 0;
    virtual Status createCommandPool(QueueType queueType, CommandPool*& out) = 0;
    virtual Status createFence(u64 initialValue, Fence*& out) = 0;
    virtual Status createQuerySet(const QuerySetDesc& desc, QuerySet*& out) = 0;
    virtual Status createSwapChain(Surface* surface, const SwapChainDesc& desc, SwapChain*& out) = 0;

    // ---- Resource destruction ----
    virtual void destroyBuffer(Buffer*& buf) = 0;
    virtual void destroyTexture(Texture*& tex) = 0;
    virtual void destroyTextureView(TextureView*& view) = 0;
    virtual void destroySampler(Sampler*& sampler) = 0;
    virtual void destroyShaderModule(ShaderModule*& shaderModule) = 0;
    virtual void destroyBindGroupLayout(BindGroupLayout*& layout) = 0;
    virtual void destroyBindGroup(BindGroup*& group) = 0;
    virtual void destroyPipelineLayout(PipelineLayout*& layout) = 0;
    virtual void destroyPipelineCache(PipelineCache*& cache) = 0;
    virtual void destroyRenderPipeline(RenderPipeline*& pipeline) = 0;
    virtual void destroyComputePipeline(ComputePipeline*& pipeline) = 0;
    virtual void destroyCommandPool(CommandPool*& pool) = 0;
    virtual void destroyFence(Fence*& fence) = 0;
    virtual void destroyQuerySet(QuerySet*& querySet) = 0;
    virtual void destroySwapChain(SwapChain*& swapChain) = 0;
    virtual void destroySurface(Surface*& surface) = 0;

    // ---- Mesh shader extension (folded into Device) ----
    /// Create a mesh shader pipeline. Returns Unsupported if mesh shaders
    /// are not enabled on this device.
    virtual Status createMeshPipeline(const MeshPipelineDesc& desc, MeshPipeline*& out) {
        (void)desc; out = nullptr; return ErrorCode::NotSupported;
    }
    virtual void destroyMeshPipeline(MeshPipeline*& pipeline) { (void)pipeline; }

    // ---- Ray tracing extension (folded into Device) ----
    /// Shader binding table handle properties. Populated by the backend
    /// during device creation when ray tracing is enabled.
    u32 shaderGroupHandleSize      = 0;
    u32 shaderGroupHandleAlignment = 0;
    u32 shaderGroupBaseAlignment   = 0;

    /// Create an acceleration structure. Returns Unsupported if ray tracing
    /// is not enabled on this device.
    virtual Status createAccelStruct(const AccelStructDesc& desc, AccelStruct*& out) {
        (void)desc; out = nullptr; return ErrorCode::NotSupported;
    }
    virtual void destroyAccelStruct(AccelStruct*& accelStruct) { (void)accelStruct; }

    virtual Status createRayTracingPipeline(const RayTracingPipelineDesc& desc, RayTracingPipeline*& out) {
        (void)desc; out = nullptr; return ErrorCode::NotSupported;
    }
    virtual void destroyRayTracingPipeline(RayTracingPipeline*& pipeline) { (void)pipeline; }

    /// Retrieve shader group handles for building shader binding tables.
    virtual Status getShaderGroupHandles(RayTracingPipeline* pipeline, u32 firstGroup,
                                          u32 groupCount, std::span<u8> outData) {
        (void)pipeline; (void)firstGroup; (void)groupCount; (void)outData;
        return ErrorCode::NotSupported;
    }

    // ---- Lifecycle ----
    virtual void waitIdle() = 0;
    virtual void destroy() = 0;
};

} // namespace draco::rhi
