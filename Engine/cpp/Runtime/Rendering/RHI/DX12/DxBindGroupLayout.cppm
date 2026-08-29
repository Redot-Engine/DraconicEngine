/// DX12 implementation of BindGroupLayout.
/// Tracks descriptor range definitions and heap offset info.

module;

#include <vector>
#include <span>

#include <cstdint>

export module rhi.dx12:bind_group_layout;

import core.stdtypes;
import core.status;
import rhi;
import :conversions;

using namespace draco;

export namespace draco::rhi::dx12 {

struct DxBindingRangeInfo {
    u32         binding = 0;
    BindingType type    = BindingType::UniformBuffer;
    u32         count   = 0;
    u32         heapOffset = 0;
    bool        isSampler  = false;
    bool        hasDynamicOffset = false;
    u32         storageBufferStride = 0;
};

class DxBindGroupLayoutImpl : public BindGroupLayout {
public:
    Status init(const BindGroupLayoutDesc& d) {
        u32 csvUavOff = 0, sampOff = 0;

        for (usize i = 0; i < d.entries.size(); ++i) {
            const auto& e = d.entries[i];
            m_entries.push_back(e);

            bool sampler = isSamplerBinding(e.type);
            u32 cnt = e.count;
            if (cnt == ~0u) { cnt = 1024 * 16; m_hasBindless = true; }

            DxBindingRangeInfo r{};
            r.binding    = e.binding;
            r.type       = e.type;
            r.count      = cnt;
            r.isSampler  = sampler;
            r.hasDynamicOffset = e.hasDynamicOffset;
            r.storageBufferStride = e.storageBufferStride;

            if (e.hasDynamicOffset) {
                r.heapOffset = 0;
                ++m_dynamicOffsetCount;
            } else if (sampler) {
                r.heapOffset = sampOff;
                sampOff += cnt;
            } else {
                r.heapOffset = csvUavOff;
                csvUavOff += cnt;
            }
            m_ranges.push_back(r);
        }
        m_cbvSrvUavCount = csvUavOff;
        m_samplerCount   = sampOff;
        return ErrorCode::Ok;
    }

    [[nodiscard]] std::span<const BindGroupLayoutEntry> entries() const { return { m_entries.data(), m_entries.size() }; }
    [[nodiscard]] std::span<const DxBindingRangeInfo>   ranges()  const { return { m_ranges.data(), m_ranges.size() }; }
    [[nodiscard]] u32  cbvSrvUavCount()    const { return m_cbvSrvUavCount; }
    [[nodiscard]] u32  samplerCount()      const { return m_samplerCount; }
    [[nodiscard]] u32  dynamicOffsetCount()const { return m_dynamicOffsetCount; }
    [[nodiscard]] bool hasBindless()       const { return m_hasBindless; }

private:
    std::vector<BindGroupLayoutEntry> m_entries;
    std::vector<DxBindingRangeInfo>   m_ranges;
    u32  m_cbvSrvUavCount    = 0;
    u32  m_samplerCount      = 0;
    u32  m_dynamicOffsetCount= 0;
    bool m_hasBindless       = false;
};

} // namespace draco::rhi::dx12
