// Draconic::RenderGraph - :descriptors partition
//
// Render-graph resource descriptors (transient texture/buffer) and pass target
// resolves size-relative-to-output and converts to an RHI TextureDesc.

module;

#include <algorithm>

#include <string_view>

export module rendergraph:descriptors;

import core;
import rhi;
import :types;

using namespace draco;

export namespace draco::rendergraph
{
    namespace rhi = draco::rhi;

    // Describes a transient texture resource in the graph.
    struct RGTextureDesc
    {
        rhi::TextureFormat format = rhi::TextureFormat::Undefined;
        SizeMode sizeMode = SizeMode::FullSize;
        u32 width = 0;                 // used only when sizeMode == Custom
        u32 height = 0;
        u32 arrayLayerCount = 1;
        u32 mipLevelCount = 1;
        u32 sampleCount = 1;
        rhi::TextureUsage usage = rhi::TextureUsage::None;

        RGTextureDesc() = default;
        RGTextureDesc(rhi::TextureFormat fmt, SizeMode mode = SizeMode::FullSize) noexcept
            : format(fmt), sizeMode(mode) {}
        RGTextureDesc(rhi::TextureFormat fmt, u32 w, u32 h) noexcept
            : format(fmt), sizeMode(SizeMode::Custom), width(w), height(h) {}

        // Resolves actual dimensions from the graph output size.
        void resolve(u32 outputWidth, u32 outputHeight) noexcept
        {
            switch (sizeMode)
            {
                case SizeMode::FullSize:
                    width = outputWidth; height = outputHeight; break;
                case SizeMode::HalfSize:
                    width = std::max(1u, outputWidth / 2u); height = std::max(1u, outputHeight / 2u); break;
                case SizeMode::QuarterSize:
                    width = std::max(1u, outputWidth / 4u); height = std::max(1u, outputHeight / 4u); break;
                case SizeMode::Custom:
                    break; // already set
            }
        }

        [[nodiscard]] rhi::TextureDesc toTextureDesc(std::u8string_view label) const
        {
            rhi::TextureDesc desc{};
            desc.format = format;
            desc.width = width;
            desc.height = height;
            desc.arrayLayerCount = arrayLayerCount;
            desc.mipLevelCount = mipLevelCount;
            desc.sampleCount = sampleCount;
            desc.usage = usage;
            desc.label = label;
            return desc;
        }
    };

    // Describes a transient buffer resource in the graph.
    struct RGBufferDesc
    {
        u64 size = 0;
        rhi::BufferUsage usage = rhi::BufferUsage::None;
    };

    // Color target attachment for a render pass.
    struct RGColorTarget
    {
        RGHandle handle = RGHandle::invalid();
        rhi::LoadOp loadOp = rhi::LoadOp::Clear;
        rhi::StoreOp storeOp = rhi::StoreOp::Store;
        rhi::ClearColor clearValue = rhi::ClearColor::black();
        RGSubresourceRange subresource;
    };

    // Depth/stencil target attachment for a render pass.
    struct RGDepthTarget
    {
        RGHandle handle = RGHandle::invalid();
        rhi::LoadOp depthLoadOp = rhi::LoadOp::Clear;
        rhi::StoreOp depthStoreOp = rhi::StoreOp::Store;
        f32 depthClearValue = 1.0f;
        bool readOnly = false;
        rhi::LoadOp stencilLoadOp = rhi::LoadOp::DontCare;
        rhi::StoreOp stencilStoreOp = rhi::StoreOp::DontCare;
        u32 stencilClearValue = 0;
        RGSubresourceRange subresource;
    };

    // Configuration for the render graph.
    struct RenderGraphConfig
    {
        i32 frameBufferCount = 2; // multi-buffering slots (typically 2 or 3)
    };
}
