/// DX12 implementation of PipelineCache via ID3D12PipelineLibrary.

module;

#include "DxIncludes.h"
#include <span>

export module rhi.dx12:pipeline_cache;

import core.stdtypes;
import core.status;
import rhi;

using namespace draco;

export namespace draco::rhi::dx12 {

class DxPipelineCacheImpl : public PipelineCache {
public:
    Status init(ID3D12Device* device, const PipelineCacheDesc& d) {
        ComPtr<ID3D12Device1> device1;
        if (FAILED(device->QueryInterface(IID_PPV_ARGS(&device1)))) return ErrorCode::Unknown;

        HRESULT hr;
        if (d.initialData.size() > 0)
            hr = device1->CreatePipelineLibrary(d.initialData.data(), d.initialData.size(), IID_PPV_ARGS(&m_library));
        else
            hr = device1->CreatePipelineLibrary(nullptr, 0, IID_PPV_ARGS(&m_library));

        return SUCCEEDED(hr) ? ErrorCode::Ok : ErrorCode::Unknown;
    }

    u32 getDataSize() override {
        if (!m_library) return 0;
        return static_cast<u32>(m_library->GetSerializedSize());
    }

    Status getData(std::span<u8> outData) override {
        if (!m_library) return ErrorCode::Unknown;
        auto size = m_library->GetSerializedSize();
        if (outData.size() < size) return ErrorCode::Unknown;
        return SUCCEEDED(m_library->Serialize(outData.data(), size)) ? ErrorCode::Ok : ErrorCode::Unknown;
    }

    void cleanup() { m_library.Reset(); }

    [[nodiscard]] ID3D12PipelineLibrary* handle() const { return m_library.Get(); }

private:
    ComPtr<ID3D12PipelineLibrary> m_library;
};

} // namespace draco::rhi::dx12
