/// DX12 implementation of PipelineLayout.
/// Creates ID3D12RootSignature from bind group layouts + push constant ranges.

module;

#include "DxIncludes.h"
#include <vector>
#include <span>

export module rhi.dx12:pipeline_layout;

import core.stdtypes;
import core.status;
import rhi;
import :conversions;
import :bind_group_layout;

using namespace draco;

export namespace draco::rhi::dx12 {

struct DynamicRootEntry {
    u32 groupIndex;
    u32 dynamicIndex;
    i32 rootParamIndex;
    D3D12_ROOT_PARAMETER_TYPE paramType;
};

class DxPipelineLayoutImpl : public PipelineLayout {
public:
    Status init(ID3D12Device* device, const PipelineLayoutDesc& d) {
        m_numBindGroups = static_cast<u32>(d.bindGroupLayouts.size());

        std::vector<D3D12_ROOT_PARAMETER> rootParams;
        // Storage for descriptor ranges (must outlive SerializeRootSignature).
        std::vector<std::vector<D3D12_DESCRIPTOR_RANGE>> rangeStorage;

        m_rootParamMap.resize(d.bindGroupLayouts.size() * 2, -1);

        for (usize gi = 0; gi < d.bindGroupLayouts.size(); ++gi) {
            auto* layout = static_cast<DxBindGroupLayoutImpl*>(d.bindGroupLayouts[gi]);
            if (!layout) return ErrorCode::Unknown;

            std::vector<D3D12_DESCRIPTOR_RANGE> csvRanges, sampRanges;
            u32 dynIdx = 0;

            auto ranges = layout->ranges();
            for (usize ri = 0; ri < ranges.size(); ++ri) {
                const auto& r = ranges[ri];

                if (r.hasDynamicOffset) {
                    D3D12_ROOT_PARAMETER_TYPE pt;
                    switch (r.type) {
                    case BindingType::UniformBuffer:          pt = D3D12_ROOT_PARAMETER_TYPE_CBV; break;
                    case BindingType::StorageBufferReadOnly:  pt = D3D12_ROOT_PARAMETER_TYPE_SRV; break;
                    case BindingType::StorageBufferReadWrite: pt = D3D12_ROOT_PARAMETER_TYPE_UAV; break;
                    default: continue;
                    }
                    D3D12_ROOT_PARAMETER p{}; p.ParameterType = pt;
                    p.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
                    p.Descriptor.ShaderRegister = r.binding;
                    p.Descriptor.RegisterSpace  = static_cast<UINT>(gi);

                    m_dynamicRootEntries.push_back({ static_cast<u32>(gi), dynIdx,
                        static_cast<i32>(rootParams.size()), pt });
                    rootParams.push_back(p);
                    ++dynIdx;
                    continue;
                }

                D3D12_DESCRIPTOR_RANGE dr{};
                dr.RangeType          = toDescriptorRangeType(r.type);
                dr.NumDescriptors     = r.count;
                dr.BaseShaderRegister = r.binding;
                dr.RegisterSpace      = static_cast<UINT>(gi);
                dr.OffsetInDescriptorsFromTableStart = r.heapOffset;

                if (r.isSampler) sampRanges.push_back(dr);
                else             csvRanges.push_back(dr);
            }

            if (!csvRanges.empty()) {
                rangeStorage.push_back(static_cast<std::vector<D3D12_DESCRIPTOR_RANGE>&&>(csvRanges));
                auto& stored = rangeStorage.back();
                D3D12_ROOT_PARAMETER p{}; p.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
                p.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
                p.DescriptorTable.NumDescriptorRanges = static_cast<UINT>(stored.size());
                p.DescriptorTable.pDescriptorRanges   = stored.data();
                m_rootParamMap[gi * 2] = static_cast<i32>(rootParams.size());
                rootParams.push_back(p);
            }
            if (!sampRanges.empty()) {
                rangeStorage.push_back(static_cast<std::vector<D3D12_DESCRIPTOR_RANGE>&&>(sampRanges));
                auto& stored = rangeStorage.back();
                D3D12_ROOT_PARAMETER p{}; p.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
                p.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
                p.DescriptorTable.NumDescriptorRanges = static_cast<UINT>(stored.size());
                p.DescriptorTable.pDescriptorRanges   = stored.data();
                m_rootParamMap[gi * 2 + 1] = static_cast<i32>(rootParams.size());
                rootParams.push_back(p);
            }
        }

        // Push constants → root 32-bit constants.
        for (usize i = 0; i < d.pushConstantRanges.size(); ++i) {
            const auto& pc = d.pushConstantRanges[i];
            if (m_pushConstantRootIndex < 0)
                m_pushConstantRootIndex = static_cast<i32>(rootParams.size());

            D3D12_ROOT_PARAMETER p{}; p.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
            p.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
            p.Constants.ShaderRegister = pc.offset / 4;
            p.Constants.RegisterSpace  = m_numBindGroups;
            p.Constants.Num32BitValues = pc.size / 4;
            rootParams.push_back(p);
        }

        // Serialize and create root signature.
        D3D12_ROOT_SIGNATURE_DESC rsDesc{};
        rsDesc.NumParameters = static_cast<UINT>(rootParams.size());
        rsDesc.pParameters   = rootParams.data();
        rsDesc.Flags         = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        ComPtr<ID3DBlob> sigBlob, errBlob;
        HRESULT hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1,
            &sigBlob, &errBlob);
        if (FAILED(hr)) {
            if (errBlob) logErrorf("DxPipelineLayout: %s", static_cast<const char*>(errBlob->GetBufferPointer()));
            return ErrorCode::Unknown;
        }

        hr = device->CreateRootSignature(0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(),
            IID_PPV_ARGS(&m_rootSig));
        return SUCCEEDED(hr) ? ErrorCode::Ok : ErrorCode::Unknown;
    }

    void cleanup() { m_rootSig.Reset(); }

    [[nodiscard]] ID3D12RootSignature* handle() const { return m_rootSig.Get(); }
    [[nodiscard]] i32 getCbvSrvUavRootIndex(u32 gi) const { return (gi*2 < m_rootParamMap.size()) ? m_rootParamMap[gi*2] : -1; }
    [[nodiscard]] i32 getSamplerRootIndex(u32 gi)   const { return (gi*2+1 < m_rootParamMap.size()) ? m_rootParamMap[gi*2+1] : -1; }
    [[nodiscard]] i32 pushConstantRootIndex()        const { return m_pushConstantRootIndex; }
    [[nodiscard]] u32 numBindGroups()                const { return m_numBindGroups; }
    [[nodiscard]] std::span<const DynamicRootEntry> dynamicRootEntries() const { return { m_dynamicRootEntries.data(), m_dynamicRootEntries.size() }; }

private:
    ComPtr<ID3D12RootSignature>    m_rootSig;
    std::vector<i32>                     m_rootParamMap;
    std::vector<DynamicRootEntry>        m_dynamicRootEntries;
    i32                            m_pushConstantRootIndex = -1;
    u32                            m_numBindGroups = 0;
};

} // namespace draco::rhi::dx12
