// Draconic::RenderGraph - :profiler partition
//
// Optional GPU profiler: per-pass timing via timestamp queries. BeginPass/EndPass
// write timestamps around each pass; Resolve copies the query results to a
// readback buffer; ReadResults (after a fence wait) maps it and builds a report.

module;

#include <algorithm>

#include <vector>
#include <string>
#include <string_view>

export module rendergraph:profiler;

import core;
import rhi;
import :types;

using namespace draco;

export namespace draco::rendergraph
{
    namespace rhi = draco::rhi;

    class GraphProfiler
    {
    public:
        bool enabled = true;

        ~GraphProfiler() { destroy(); }

        GraphProfiler() = default;
        GraphProfiler(const GraphProfiler&) = delete;
        GraphProfiler& operator=(const GraphProfiler&) = delete;

        // Two timestamp queries per pass (begin + end) + a GpuToCpu readback buffer.
        [[nodiscard]] Status init(rhi::Device& device, i32 maxPasses = 64)
        {
            m_device = &device;
            m_maxPasses = maxPasses;

            rhi::QuerySetDesc queryDesc{};
            queryDesc.type = rhi::QueryType::Timestamp;
            queryDesc.count = static_cast<u32>(maxPasses * 2);
            queryDesc.label = u8"RG_Profiler_Queries";
            if (!device.createQuerySet(queryDesc, m_querySet).isOk()) { return Status{ ErrorCode::Unknown }; }

            rhi::BufferDesc bufDesc{};
            bufDesc.size = static_cast<u64>(maxPasses) * 2u * sizeof(u64);
            bufDesc.usage = rhi::BufferUsage::CopyDst;
            bufDesc.memory = rhi::MemoryLocation::GpuToCpu;
            bufDesc.label = u8"RG_Profiler_Readback";
            if (!device.createBuffer(bufDesc, m_readbackBuffer).isOk()) { return Status{ ErrorCode::Unknown }; }

            m_passTimesMs.resize(static_cast<usize>(maxPasses));
            m_initialized = true;
            return Status{};
        }

        // Reset the query set at the START of the frame's encoder - timestamps can only be written
        // into a freshly-reset pool, and the reset must be outside any render pass.
        void beginFrame(rhi::CommandEncoder& encoder)
        {
            if (!m_initialized || !enabled) { return; }
            encoder.resetQuerySet(m_querySet, 0, static_cast<u32>(m_maxPasses * 2));
            m_passNames.clear();
        }

        void beginPass(rhi::CommandEncoder& encoder, i32 passIndex, std::u8string_view passName)
        {
            if (!m_initialized || !enabled || passIndex >= m_maxPasses) { return; }
            while (static_cast<i32>(m_passNames.size()) <= passIndex) { m_passNames.push_back(std::u8string{}); }
            m_passNames[static_cast<usize>(passIndex)] = std::u8string(passName);
            encoder.writeTimestamp(m_querySet, static_cast<u32>(passIndex * 2));
        }

        void endPass(rhi::CommandEncoder& encoder, i32 passIndex)
        {
            if (!m_initialized || !enabled || passIndex >= m_maxPasses) { return; }
            encoder.writeTimestamp(m_querySet, static_cast<u32>(passIndex * 2 + 1));
        }

        // Resolve queries into the readback buffer (call after recording all passes). The pool was
        // already reset by BeginFrame, so this only copies the written timestamps out.
        void resolve(rhi::CommandEncoder& encoder, i32 passCount)
        {
            if (!m_initialized || !enabled || passCount == 0) { return; }
            const u32 queryCount = static_cast<u32>(std::min(passCount * 2, m_maxPasses * 2));
            encoder.resolveQuerySet(m_querySet, 0, queryCount, m_readbackBuffer, 0);
        }

        // Read results and append a timing report (call after the GPU has finished).
        void readResults(i32 passCount, std::u8string& outReport)
        {
            if (!m_initialized || !enabled || passCount == 0) { return; }

            const u64* mapped = static_cast<const u64*>(m_readbackBuffer->map());
            if (mapped == nullptr) { return; }

            const i32 count = std::min(passCount, m_maxPasses);
            f32 totalMs = 0.0f;

            outReport.append(u8"=== GPU Pass Timing ===\n");
            for (i32 i = 0; i < count; ++i)
            {
                const u64 begin = mapped[i * 2];
                const u64 end = mapped[i * 2 + 1];
                const u64 ticks = end > begin ? end - begin : 0;
                const f32 ms = static_cast<f32>(ticks) * m_gpuTimestampPeriod / 1000000.0f;
                m_passTimesMs[static_cast<usize>(i)] = ms;
                totalMs += ms;

                const std::u8string_view name = i < static_cast<i32>(m_passNames.size())
                                      ? m_passNames[static_cast<usize>(i)] : std::u8string_view(u8"???");
                appendFormat(outReport, u8"  {} ms  {}\n", ms, name);
            }
            appendFormat(outReport, u8"  --------\n  {} ms  TOTAL\n", totalMs);

            // Aggregate by pass NAME so many same-named passes (e.g. 24x probes.prefilter) read as one
            // line - a "which pass category is expensive" summary, sorted most-expensive first.
            struct Agg { std::u8string_view name; f32 sum = 0.0f; i32 n = 0; };
            std::vector<Agg> agg;
            for (i32 i = 0; i < count; ++i)
            {
                const std::u8string_view nm = i < static_cast<i32>(m_passNames.size())
                                    ? m_passNames[static_cast<usize>(i)] : std::u8string_view(u8"???");
                bool found = false;
                for (Agg& a : agg) { if (a.name == nm) { a.sum += m_passTimesMs[static_cast<usize>(i)]; ++a.n; found = true; break; } }
                if (!found) { Agg a; a.name = nm; a.sum = m_passTimesMs[static_cast<usize>(i)]; a.n = 1; agg.push_back(a); }
            }
            for (usize i = 0; i < agg.size(); ++i)   // selection sort by total (few entries)
                for (usize j = i + 1; j < agg.size(); ++j)
                    if (agg[j].sum > agg[i].sum) { const Agg t = agg[i]; agg[i] = agg[j]; agg[j] = t; }
            outReport.append(u8"=== GPU by pass name (expensive first) ===\n");
            for (const Agg& a : agg) { appendFormat(outReport, u8"  {} ms  (x{})  {}\n", a.sum, a.n, a.name); }

            m_readbackBuffer->unmap();
        }

        [[nodiscard]] f32 getPassTimeMs(i32 passIndex) const
        {
            if (passIndex < 0 || passIndex >= static_cast<i32>(m_passTimesMs.size())) { return 0.0f; }
            return m_passTimesMs[static_cast<usize>(passIndex)];
        }

        // GPU timestamp period (nanoseconds per tick); backend-specific.
        void setTimestampPeriod(f32 nanosecondsPerTick) noexcept { m_gpuTimestampPeriod = nanosecondsPerTick; }

        void destroy()
        {
            if (m_device != nullptr)
            {
                if (m_readbackBuffer != nullptr) { m_device->destroyBuffer(m_readbackBuffer); }
                if (m_querySet != nullptr) { m_device->destroyQuerySet(m_querySet); }
            }
            m_initialized = false;
        }

    private:
        rhi::Device* m_device = nullptr;
        rhi::QuerySet* m_querySet = nullptr;
        rhi::Buffer* m_readbackBuffer = nullptr;
        i32 m_maxPasses = 0;
        bool m_initialized = false;
        std::vector<std::u8string> m_passNames;
        std::vector<f32> m_passTimesMs;
        f32 m_gpuTimestampPeriod = 0.0f;
    };
}
