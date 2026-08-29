/// DX12 implementation of Adapter.
/// Wraps IDXGIAdapter1, queries device features, creates DxDevice.

module;

#include "DxIncludes.h"

#include <cstring>
#include <string>

export module rhi.dx12:adapter;

import core.stdtypes;
import core.status;
import rhi;

using namespace draco;

export namespace draco::rhi::dx12 {

class DxDeviceImpl; // forward

/// Convert a null-terminated UTF-16 string (as DXGI reports device names) to UTF-8.
inline std::u8string wideToUtf8(const wchar_t* w) {
    if (!w) return {};
    const int len = ::WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 1) return {};
    std::u8string out(static_cast<usize>(len - 1), u8'\0');
    ::WideCharToMultiByte(CP_UTF8, 0, w, -1, reinterpret_cast<char*>(out.data()), len, nullptr, nullptr);
    return out;
}

class DxAdapterImpl : public Adapter {
public:
    DxAdapterImpl(IDXGIAdapter1* adapter, IDXGIFactory4* factory)
        : m_adapter(adapter), m_factory(factory)
    {
        m_adapter->GetDesc1(&m_desc);
    }

    ~DxAdapterImpl() override {
        if (m_adapter) { m_adapter->Release(); m_adapter = nullptr; }
    }

    // ---- Adapter interface ----

    void getInfo(AdapterInfo& out) override {
        // DXGI Description is a WCHAR[] (UTF-16) - transcode to UTF-8.
        out.name = wideToUtf8(m_desc.Description);
        out.vendorId = m_desc.VendorId;
        out.deviceId = m_desc.DeviceId;
        out.type = (m_desc.DedicatedVideoMemory > 0) ? AdapterType::DiscreteGpu : AdapterType::IntegratedGpu;
        out.supportedFeatures = buildFeatures();
    }

    DeviceFeatures buildFeatures() {
        // Create a temporary device to query features.
        ComPtr<ID3D12Device> tempDevice;
        HRESULT hr = D3D12CreateDevice(m_adapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&tempDevice));
        if (FAILED(hr) || !tempDevice) return {};

        DeviceFeatures f{};

        // Check feature support.
        D3D12_FEATURE_DATA_D3D12_OPTIONS options{};
        if (SUCCEEDED(tempDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &options, sizeof(options)))) {
            f.bindlessDescriptors = true; // DX12 always supports descriptor indexing
            f.timestampQueries = true;
            f.multiDrawIndirect = true;
            f.depthClamp = true;
            f.fillModeWireframe = true;
            f.textureCompressionBC = true;
            f.textureCompressionASTC = false;
            f.independentBlend = true;
            f.multiViewport = true;
            f.pipelineStatisticsQueries = true;
        }

        // Check mesh shader support.
        D3D12_FEATURE_DATA_D3D12_OPTIONS7 options7{};
        if (SUCCEEDED(tempDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7, &options7, sizeof(options7)))) {
            f.meshShaders = (options7.MeshShaderTier != D3D12_MESH_SHADER_TIER_NOT_SUPPORTED);
        }

        // Check ray tracing support.
        D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5{};
        if (SUCCEEDED(tempDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5)))) {
            f.rayTracing = (options5.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED);
        }

        // Conservative limits for D3D12 feature level 12.0.
        f.maxBindGroups = 32;
        f.maxBindingsPerGroup = 1000000;
        f.maxPushConstantSize = 128;
        f.maxTextureDimension2D = 16384;
        f.maxTextureArrayLayers = 2048;
        f.maxComputeWorkgroupSizeX = 1024;
        f.maxComputeWorkgroupSizeY = 1024;
        f.maxComputeWorkgroupSizeZ = 64;
        f.maxComputeWorkgroupsPerDimension = 65535;
        f.maxBufferSize = static_cast<u64>(m_desc.DedicatedVideoMemory);
        f.minUniformBufferOffsetAlignment = 256;
        f.minStorageBufferOffsetAlignment = 16;
        f.timestampPeriodNs = 1; // DX12 timestamps in ticks, period queried at runtime

        return f;
    }

    Status createDevice(const DeviceDesc& desc, Device*& out) override;

    // ---- Internal ----
    [[nodiscard]] IDXGIAdapter1*    handle()  const { return m_adapter; }
    [[nodiscard]] IDXGIFactory4*    factory() const { return m_factory; }
    [[nodiscard]] DXGI_ADAPTER_DESC1 adapterDesc() const { return m_desc; }

private:
    IDXGIAdapter1*     m_adapter = nullptr; // owned, released in destructor
    IDXGIFactory4*     m_factory = nullptr; // not owned (Backend owns it)
    DXGI_ADAPTER_DESC1 m_desc{};
};

} // namespace draco::rhi::dx12
