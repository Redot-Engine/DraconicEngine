// Draconic::RenderGraph - :state_tracker partition
//
// Tracks ResourceState per subresource (mip x layer) with a uniform fast path:
// while all subresources share a state, only one value is stored; a per-
// subresource array is materialized lazily when states diverge, and collapsed
// (SubresourceStateTracker.bf); Beef's null-means-uniform becomes an empty Array.

module;

#include <algorithm>

#include <vector>

export module rendergraph:state_tracker;

import core;
import rhi;
import :types;

using namespace draco;

export namespace draco::rendergraph
{
    namespace rhi = draco::rhi;

    class SubresourceStateTracker
    {
    public:
        SubresourceStateTracker(u32 mipCount, u32 layerCount, rhi::ResourceState initialState)
            : m_mipCount(std::max(mipCount, 1u)), m_layerCount(std::max(layerCount, 1u)), m_uniformState(initialState) {}

        [[nodiscard]] u32 mipCount() const noexcept { return m_mipCount; }
        [[nodiscard]] u32 layerCount() const noexcept { return m_layerCount; }
        [[nodiscard]] bool isUniform() const noexcept { return m_states.empty(); }
        [[nodiscard]] rhi::ResourceState uniformState() const noexcept { return m_uniformState; }

        [[nodiscard]] rhi::ResourceState getState(u32 mip, u32 layer) const
        {
            if (m_states.empty()) { return m_uniformState; }
            const u32 idx = mip + layer * m_mipCount;
            if (idx >= m_states.size()) { return m_uniformState; }
            return m_states[idx];
        }

        // count of 0 or ~0u means "all remaining from base".
        void setState(u32 baseMip, u32 mipCount, u32 baseLayer, u32 layerCount, rhi::ResourceState state)
        {
            const u32 mipEnd = resolveEnd(baseMip, mipCount, m_mipCount);
            const u32 layerEnd = resolveEnd(baseLayer, layerCount, m_layerCount);

            // Whole resource? Collapse to uniform.
            if (baseMip == 0 && mipEnd >= m_mipCount && baseLayer == 0 && layerEnd >= m_layerCount)
            {
                m_uniformState = state;
                m_states.clear();
                return;
            }

            // Materialize per-subresource storage on first divergence.
            if (m_states.empty())
            {
                if (state == m_uniformState) { return; }
                m_states.resize(m_mipCount * m_layerCount);
                for (u32 i = 0; i < m_states.size(); ++i) { m_states[i] = m_uniformState; }
            }

            for (u32 layer = baseLayer; layer < layerEnd; ++layer)
            {
                for (u32 mip = baseMip; mip < mipEnd; ++mip)
                {
                    m_states[mip + layer * m_mipCount] = state;
                }
            }

            tryCollapseToUniform();
        }

        void setState(RGSubresourceRange range, rhi::ResourceState state)
        {
            const u32 mipCount = range.mipLevelCount == 0 ? 0xFFFFFFFFu : range.mipLevelCount;
            const u32 layerCount = range.arrayLayerCount == 0 ? 0xFFFFFFFFu : range.arrayLayerCount;
            setState(range.baseMipLevel, mipCount, range.baseArrayLayer, layerCount, state);
        }

        void setAll(rhi::ResourceState state)
        {
            m_uniformState = state;
            m_states.clear();
        }

        // Snapshot per-subresource states; empty if uniform. Caller owns the copy.
        [[nodiscard]] std::vector<rhi::ResourceState> copyStates() const
        {
            std::vector<rhi::ResourceState> copy;
            if (!m_states.empty())
            {
                copy.resize(m_states.size());
                for (u32 i = 0; i < m_states.size(); ++i) { copy[i] = m_states[i]; }
            }
            return copy;
        }

        // Restore from a per-subresource snapshot (empty/mismatched => uniform fallback).
        void initFromStates(const std::vector<rhi::ResourceState>& states, rhi::ResourceState uniformFallback)
        {
            if (states.size() != static_cast<usize>(m_mipCount) * m_layerCount)
            {
                m_uniformState = uniformFallback;
                m_states.clear();
                return;
            }
            m_states.resize(states.size());
            for (usize i = 0; i < states.size(); ++i) { m_states[i] = states[i]; }
            tryCollapseToUniform();
        }

    private:
        void tryCollapseToUniform()
        {
            if (m_states.empty()) { return; }
            const rhi::ResourceState first = m_states[0];
            for (usize i = 1; i < m_states.size(); ++i)
            {
                if (m_states[i] != first) { return; }
            }
            m_uniformState = first;
            m_states.clear();
        }

        static u32 resolveEnd(u32 base, u32 count, u32 total) noexcept
        {
            if (count == 0 || count == 0xFFFFFFFFu) { return total; }
            return std::min(base + count, total);
        }

        u32 m_mipCount;
        u32 m_layerCount;
        rhi::ResourceState m_uniformState;
        std::vector<rhi::ResourceState> m_states; // empty => uniform
    };
}
