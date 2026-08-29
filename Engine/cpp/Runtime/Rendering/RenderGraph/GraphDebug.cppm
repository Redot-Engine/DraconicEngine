// Draconic::RenderGraph - :debug partition
//
// Debug visualization/reporting: Graphviz DOT export and a text summary. Ported
// from Sedulous.RenderGraph (GraphDebug.bf).

module;

#include <vector>
#include <string>
#include <string_view>

export module rendergraph:debug;

import core;
import :types;
import :pass;
import :resource;
import :graph;

using namespace draco;

export namespace draco::rendergraph
{
    namespace detail
    {
        [[nodiscard]] inline std::u8string_view passColor(RGPassType type)
        {
            switch (type)
            {
                case RGPassType::Render:  return u8"#4488cc";
                case RGPassType::Compute: return u8"#cc8844";
                case RGPassType::Copy:    return u8"#44aa44";
            }
            return u8"#888888";
        }
        [[nodiscard]] inline std::u8string_view lifetimeLabel(RGResourceLifetime lifetime)
        {
            switch (lifetime)
            {
                case RGResourceLifetime::Transient:  return u8"transient";
                case RGResourceLifetime::Persistent: return u8"persistent";
                case RGResourceLifetime::Imported:   return u8"imported";
            }
            return u8"?";
        }
        [[nodiscard]] inline std::u8string_view accessLabel(RGAccessType type)
        {
            switch (type)
            {
                case RGAccessType::ReadTexture:          return u8"read";
                case RGAccessType::ReadBuffer:           return u8"read";
                case RGAccessType::ReadDepthStencil:     return u8"depth-read";
                case RGAccessType::SampleDepthStencil:   return u8"depth-sample";
                case RGAccessType::ReadCopySrc:          return u8"copy-src";
                case RGAccessType::WriteColorTarget:     return u8"color-out";
                case RGAccessType::WriteDepthTarget:     return u8"depth-out";
                case RGAccessType::WriteStorage:         return u8"storage-write";
                case RGAccessType::WriteCopyDst:         return u8"copy-dst";
                case RGAccessType::ReadWriteStorage:     return u8"rw-storage";
                case RGAccessType::ReadWriteDepthTarget: return u8"depth-rw";
                case RGAccessType::ReadWriteColorTarget: return u8"color-rw";
            }
            return u8"?";
        }
        [[nodiscard]] inline std::u8string_view passTypeLabel(RGPassType type)
        {
            switch (type)
            {
                case RGPassType::Render:  return u8"Render";
                case RGPassType::Compute: return u8"Compute";
                case RGPassType::Copy:    return u8"Copy";
            }
            return u8"?";
        }
    }

    class GraphDebug
    {
    public:
        // Graphviz DOT: pass nodes (boxes) + resource nodes (ellipse/diamond) +
        // access edges. Culled passes/edges are dashed/gray.
        static void exportDOT(RenderGraph& graph, std::u8string& out)
        {
            const std::vector<RenderGraphPass*>& passes = graph.passes();
            const std::vector<RenderGraphResource*>& resources = graph.resources();

            out.append(u8"digraph RenderGraph {\n");
            out.append(u8"  rankdir=LR;\n");
            out.append(u8"  node [fontname=\"Helvetica\"];\n\n");

            for (usize i = 0; i < passes.size(); ++i)
            {
                RenderGraphPass* pass = passes[i];
                const std::u8string_view style = pass->isCulled ? std::u8string_view(u8"dashed") : std::u8string_view(u8"filled");
                const std::u8string_view fontColor = pass->isCulled ? std::u8string_view(u8"gray") : std::u8string_view(u8"white");
                appendFormat(out,
                    u8"  pass{} [label=\"{}\" shape=box style={} fillcolor=\"{}\" fontcolor=\"{}\"",
                    i, pass->name, style, detail::passColor(pass->type), fontColor);
                if (pass->isCulled) { out.append(u8" color=gray"); }
                out.append(u8"];\n");
            }
            out.append(u8"\n");

            for (usize i = 0; i < resources.size(); ++i)
            {
                RenderGraphResource* res = resources[i];
                if (res == nullptr) { continue; }
                const std::u8string_view shape = res->resourceType == RGResourceType::Texture
                                       ? std::u8string_view(u8"ellipse") : std::u8string_view(u8"diamond");
                appendFormat(out, u8"  res{} [label=\"{}\\n({})\" shape={}];\n",
                    i, res->name, detail::lifetimeLabel(res->lifetime), shape);
            }
            out.append(u8"\n");

            for (usize passIdx = 0; passIdx < passes.size(); ++passIdx)
            {
                RenderGraphPass* pass = passes[passIdx];
                for (const RGResourceAccess& access : pass->accesses)
                {
                    if (!access.handle.isValid() || access.handle.index >= resources.size()) { continue; }
                    if (resources[access.handle.index] == nullptr) { continue; }

                    const std::u8string_view label = detail::accessLabel(access.type);
                    if (access.isRead())
                    {
                        appendFormat(out, u8"  res{} -> pass{} [label=\"{}\"", access.handle.index, passIdx, label);
                        if (pass->isCulled) { out.append(u8" style=dashed color=gray"); }
                        out.append(u8"];\n");
                    }
                    if (access.isWrite())
                    {
                        appendFormat(out, u8"  pass{} -> res{} [label=\"{}\"", passIdx, access.handle.index, label);
                        if (pass->isCulled) { out.append(u8" style=dashed color=gray"); }
                        out.append(u8"];\n");
                    }
                }
            }

            out.append(u8"}\n");
        }

        // Human-readable text summary (counts + execution order).
        static void exportSummary(RenderGraph& graph, std::u8string& out)
        {
            const std::vector<RenderGraphPass*>& passes = graph.passes();
            const std::vector<RenderGraphResource*>& resources = graph.resources();
            const std::vector<i32>& executionOrder = graph.executionOrder();

            usize activeCount = 0, culledCount = 0;
            for (RenderGraphPass* p : passes) { if (p->isCulled) { ++culledCount; } else { ++activeCount; } }

            usize resCount = 0, transientCount = 0, persistentCount = 0, importedCount = 0;
            for (RenderGraphResource* r : resources)
            {
                if (r == nullptr) { continue; }
                ++resCount;
                switch (r->lifetime)
                {
                    case RGResourceLifetime::Transient:  ++transientCount; break;
                    case RGResourceLifetime::Persistent: ++persistentCount; break;
                    case RGResourceLifetime::Imported:   ++importedCount; break;
                }
            }

            out.append(u8"=== Render Graph Summary ===\n");
            appendFormat(out, u8"Passes: {} active, {} culled, {} total\n", activeCount, culledCount, passes.size());
            appendFormat(out, u8"Resources: {} total ({} transient, {} persistent, {} imported)\n",
                resCount, transientCount, persistentCount, importedCount);
            appendFormat(out, u8"Output: {}x{}\n\n", graph.outputWidth(), graph.outputHeight());

            if (!executionOrder.empty())
            {
                out.append(u8"Execution order:\n");
                for (usize i = 0; i < executionOrder.size(); ++i)
                {
                    RenderGraphPass* pass = passes[static_cast<usize>(executionOrder[i])];
                    appendFormat(out, u8"  {}. [{}] {}\n", i + 1, detail::passTypeLabel(pass->type), pass->name);
                }
            }
        }
    };
}
