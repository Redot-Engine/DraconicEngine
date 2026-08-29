/// Vulkan implementation of QuerySet.

module;

#include "VkIncludes.h"

export module rhi.vk:query_set;

import core.stdtypes;
import core.status;
import rhi;

using namespace draco;

export namespace draco::rhi::vk {

class VkQuerySetImpl : public QuerySet {
public:
    Status init(VkDevice device, const QuerySetDesc& d) {
        type  = d.type;
        count = d.count;

        VkQueryPoolCreateInfo ci{};
        ci.sType      = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        ci.queryCount = d.count;

        switch (d.type) {
        case QueryType::Timestamp:          ci.queryType = VK_QUERY_TYPE_TIMESTAMP; break;
        case QueryType::Occlusion:          ci.queryType = VK_QUERY_TYPE_OCCLUSION; break;
        case QueryType::PipelineStatistics:
            ci.queryType = VK_QUERY_TYPE_PIPELINE_STATISTICS;
            ci.pipelineStatistics =
                VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT |
                VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT |
                VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT |
                VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT |
                VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT;
            break;
        }

        if (vkCreateQueryPool(device, &ci, nullptr, &m_pool) != VK_SUCCESS) return ErrorCode::Unknown;
        return ErrorCode::Ok;
    }

    void cleanup(VkDevice device) {
        if (m_pool != VK_NULL_HANDLE) { vkDestroyQueryPool(device, m_pool, nullptr); m_pool = VK_NULL_HANDLE; }
    }

    [[nodiscard]] VkQueryPool handle() const { return m_pool; }

private:
    VkQueryPool m_pool = VK_NULL_HANDLE;
};

} // namespace draco::rhi::vk
