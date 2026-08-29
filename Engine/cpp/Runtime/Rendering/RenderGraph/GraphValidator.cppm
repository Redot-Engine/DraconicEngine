// Draconic::RenderGraph - :validator partition
//
// Validates a graph for common authoring errors: reads of never-written
// resources (error), passes with no execute callback (warning), and redundant
// (GraphValidator.bf).

module;

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <string_view>

export module rendergraph:validator;

import core;
import :types;
import :pass;
import :resource;
import :graph;

using namespace draco;

export namespace draco::rendergraph
{
    enum class ValidationSeverity { Warning, Error };

    struct ValidationMessage
    {
        ValidationSeverity severity = ValidationSeverity::Warning;
        std::u8string message;
    };

    namespace detail
    {
        [[nodiscard]] inline std::u8string_view resName(const std::vector<RenderGraphResource*>& resources, u32 index)
        {
            return (index < resources.size() && resources[index] != nullptr)
                ? resources[index]->name : std::u8string_view(u8"???");
        }
    }

    class GraphValidator
    {
    public:
        static void validate(RenderGraph& graph, std::vector<ValidationMessage>& out)
        {
            checkUninitializedReads(graph, out);
            checkEmptyPasses(graph, out);
            checkRedundantWrites(graph, out);
        }

        static void validateToString(RenderGraph& graph, std::u8string& out)
        {
            std::vector<ValidationMessage> messages;
            validate(graph, messages);

            if (messages.empty())
            {
                out.append(u8"Render graph validation: OK (no issues)\n");
                return;
            }

            appendFormat(out, u8"Render graph validation: {} issue(s)\n", messages.size());
            for (const ValidationMessage& msg : messages)
            {
                const std::u8string_view prefix = msg.severity == ValidationSeverity::Error ? std::u8string_view(u8"ERROR")
                                                                                    : std::u8string_view(u8"WARNING");
                appendFormat(out, u8"  [{}] {}\n", prefix, msg.message);
            }
        }

    private:
        // Reads of resources that no prior pass wrote (transient only - imported
        // and persistent are considered externally initialized).
        static void checkUninitializedReads(RenderGraph& graph, std::vector<ValidationMessage>& out)
        {
            const std::vector<RenderGraphResource*>& resources = graph.resources();
            std::unordered_set<u32> written;

            for (u32 i = 0; i < resources.size(); ++i)
            {
                RenderGraphResource* res = resources[i];
                if (res != nullptr && (res->lifetime == RGResourceLifetime::Imported
                                       || res->lifetime == RGResourceLifetime::Persistent))
                {
                    written.insert(i);
                }
            }

            for (RenderGraphPass* pass : graph.passes())
            {
                for (const RGResourceAccess& access : pass->accesses)
                {
                    if (access.isRead() && access.handle.isValid() && !written.contains(access.handle.index))
                    {
                        ValidationMessage msg;
                        msg.severity = ValidationSeverity::Error;
                        appendFormat(msg.message,
                            u8"Pass '{}' reads resource '{}' (index {}) which has not been written to",
                            pass->name, detail::resName(resources, access.handle.index), access.handle.index);
                        out.push_back(static_cast<ValidationMessage&&>(msg));
                    }
                }
                for (const RGResourceAccess& access : pass->accesses)
                {
                    if (access.isWrite() && access.handle.isValid()) { written.insert(access.handle.index); }
                }
            }
        }

        static void checkEmptyPasses(RenderGraph& graph, std::vector<ValidationMessage>& out)
        {
            for (RenderGraphPass* pass : graph.passes())
            {
                bool hasCallback = false;
                switch (pass->type)
                {
                    case RGPassType::Render:  hasCallback = static_cast<bool>(pass->executeCallback); break;
                    case RGPassType::Compute: hasCallback = static_cast<bool>(pass->computeCallback); break;
                    case RGPassType::Copy:    hasCallback = static_cast<bool>(pass->copyCallback); break;
                }
                if (!hasCallback)
                {
                    ValidationMessage msg;
                    msg.severity = ValidationSeverity::Warning;
                    appendFormat(msg.message, u8"Pass '{}' has no execute callback", pass->name);
                    out.push_back(static_cast<ValidationMessage&&>(msg));
                }
            }
        }

        // Resources written twice with no read in between.
        static void checkRedundantWrites(RenderGraph& graph, std::vector<ValidationMessage>& out)
        {
            const std::vector<RenderGraphResource*>& resources = graph.resources();
            std::unordered_map<u32, std::u8string> lastWriter;

            for (RenderGraphPass* pass : graph.passes())
            {
                for (const RGResourceAccess& access : pass->accesses)
                {
                    if (access.isRead() && access.handle.isValid()) { lastWriter.erase(access.handle.index); }
                }
                for (const RGResourceAccess& access : pass->accesses)
                {
                    if (!access.isWrite() || !access.handle.isValid()) { continue; }
                    if (std::u8string* prev = mapFind(lastWriter, access.handle.index))
                    {
                        ValidationMessage msg;
                        msg.severity = ValidationSeverity::Warning;
                        appendFormat(msg.message,
                            u8"Resource '{}' written by pass '{}' was already written by '{}' without being read",
                            detail::resName(resources, access.handle.index), pass->name, *prev);
                        out.push_back(static_cast<ValidationMessage&&>(msg));
                    }
                    lastWriter.insert_or_assign(access.handle.index, std::u8string(pass->name));
                }
            }
        }
    };
}
