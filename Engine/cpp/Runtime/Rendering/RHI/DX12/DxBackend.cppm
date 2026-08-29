/// DX12 implementation of Backend.
/// Creates DXGI factory, enumerates adapters, creates surfaces.

module;

#include "DxIncludes.h"
#include <vector>
#include <span>

#include <cstdio>

export module rhi.dx12:backend;

import core.stdtypes;
import core.status;
import rhi;
import :surface;
import :adapter;

using namespace draco;

export namespace draco::rhi::dx12 {

/// Configuration for DX12 backend creation.
struct DxBackendDesc {
    bool enableValidation = false;
};

/// DX12 implementation of Backend.
class DxBackendImpl : public Backend {
public:
    ~DxBackendImpl() override { destroyImpl(); }

    // ---- Backend interface ----

    std::span<Adapter* const> enumerateAdapters() override {
        return std::span<Adapter* const>(m_adapterPtrs.data(), m_adapterPtrs.size());
    }

    Status createSurface(void* windowHandle, void* /*displayHandle*/, Surface*& out) override {
        out = nullptr;
        if (!windowHandle) {
            logError("DxBackend: window handle is null");
            return ErrorCode::InvalidArgument;
        }
        out = new DxSurfaceImpl(reinterpret_cast<HWND>(windowHandle));
        return ErrorCode::Ok;
    }

    void destroy() override {
        destroyImpl();
        delete this;
    }

    // ---- Internal ----
    [[nodiscard]] IDXGIFactory4* factory() const { return m_factory.Get(); }
    [[nodiscard]] bool validationEnabled() const { return m_validationEnabled; }

private:
    friend Status createDxBackend(const DxBackendDesc& desc, Backend*& out);

    Status init(bool enableValidation) {
        m_validationEnabled = enableValidation;

        // Enable debug layer before device creation.
        if (m_validationEnabled) {
            ComPtr<ID3D12Debug> debugController;
            if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
                debugController->EnableDebugLayer();
            }
        }

        // Create DXGI factory.
        UINT factoryFlags = m_validationEnabled ? DXGI_CREATE_FACTORY_DEBUG : 0;
        HRESULT hr = CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&m_factory));
        if (FAILED(hr)) {
            logErrorf("DxBackend: CreateDXGIFactory2 failed (0x%08X)", static_cast<unsigned>(hr));
            return ErrorCode::Unknown;
        }

        enumerateAdaptersInternal();
        isInitialized = true;
        return ErrorCode::Ok;
    }

    void enumerateAdaptersInternal() {
        ComPtr<IDXGIAdapter1> adapter;
        for (UINT i = 0; m_factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
            DXGI_ADAPTER_DESC1 desc{};
            adapter->GetDesc1(&desc);

            // Skip software adapters.
            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
                adapter.Reset();
                continue;
            }

            // Check D3D12 feature level 12.0 support.
            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, __uuidof(ID3D12Device), nullptr))) {
                auto* a = new DxAdapterImpl(adapter.Detach(), m_factory.Get());
                m_adapters.push_back(a);
                m_adapterPtrs.push_back(a);
            }

            adapter.Reset();
        }

        // Expose adapters best-GPU-first; callers take [0]. See Backend::enumerateAdapters.
        sortAdaptersByPreference(m_adapterPtrs);
    }

    void destroyImpl() {
        for (auto* a : m_adapters) delete a;
        m_adapters.clear();
        m_adapterPtrs.clear();
        m_factory.Reset();
    }

    ComPtr<IDXGIFactory4>         m_factory;
    bool                          m_validationEnabled = false;
    std::vector<DxAdapterImpl*>         m_adapters;
    std::vector<Adapter*>               m_adapterPtrs;
};

/// Creates a DX12 backend. Caller owns the returned pointer - dispose via destroy().
[[nodiscard]] Status createDxBackend(const DxBackendDesc& desc, Backend*& out) {
    out = nullptr;
    auto* b = new DxBackendImpl();
    Status r = b->init(desc.enableValidation);
    if (r != ErrorCode::Ok) {
        delete b;
        return r;
    }
    out = b;
    return ErrorCode::Ok;
}

} // namespace draco::rhi::dx12
