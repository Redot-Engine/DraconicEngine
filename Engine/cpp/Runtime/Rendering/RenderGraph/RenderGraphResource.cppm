// Draconic::RenderGraph - :resource partition
//
// A resource managed by the graph (texture or buffer): its descriptor, the
// allocated GPU object, reference/lifetime tracking computed during compile, and
// Fields are public data the graph orchestrator manipulates directly.

module;

#include <optional>
#include <memory>
#include <string>
#include <string_view>

export module rendergraph:resource;

import core;
import rhi;
import :types;
import :descriptors;
import :persistent_resource;

using namespace draco;

export namespace draco::rendergraph
{
    namespace rhi = draco::rhi;

    class RenderGraphResource
    {
    public:
        RenderGraphResource(std::u8string_view resourceName, RGResourceType type, RGResourceLifetime life)
            : name(resourceName), resourceType(type), lifetime(life) {}

        // Allocate GPU resources for a transient texture.
        [[nodiscard]] Status allocateTexture(rhi::Device& device)
        {
            rhi::TextureDesc rhiDesc = textureDesc.toTextureDesc(name);
            rhiDesc.usage = rhiDesc.usage | rhi::TextureUsage::Sampled; // may be sampled
            if (rhi::isDepthFormat(textureDesc.format)) { rhiDesc.usage = rhiDesc.usage | rhi::TextureUsage::DepthStencil; }
            else                                        { rhiDesc.usage = rhiDesc.usage | rhi::TextureUsage::RenderTarget; }

            rhi::Texture* tex = nullptr;
            if (!device.createTexture(rhiDesc, tex).isOk()) { return Status{ ErrorCode::Unknown }; }
            texture = tex;
            lastKnownState = tex->initialState;

            rhi::TextureView* view = nullptr;
            if (!device.createTextureView(tex, rhi::TextureViewDesc{}, view).isOk()) { return Status{ ErrorCode::Unknown }; }
            textureView = view;

            // Depth-only view for depth/stencil textures (shader sampling of depth).
            if (rhi::isDepthFormat(textureDesc.format) && rhi::hasStencil(textureDesc.format))
            {
                rhi::TextureViewDesc depthDesc{};
                depthDesc.aspect = rhi::TextureAspect::DepthOnly;
                depthDesc.label = u8"RGDepthOnlyView";
                rhi::TextureView* depthOnly = nullptr;
                if (!device.createTextureView(tex, depthDesc, depthOnly).isOk()) { return Status{ ErrorCode::Unknown }; }
                depthOnlyView = depthOnly;
            }
            return Status{};
        }

        // Allocate GPU resources for a transient buffer.
        [[nodiscard]] Status allocateBuffer(rhi::Device& device)
        {
            rhi::BufferDesc rhiDesc{};
            rhiDesc.size = bufferDesc.size;
            rhiDesc.usage = bufferDesc.usage;
            rhiDesc.label = name;

            rhi::Buffer* buf = nullptr;
            if (!device.createBuffer(rhiDesc, buf).isOk()) { return Status{ ErrorCode::Unknown }; }
            buffer = buf;
            lastKnownState = rhi::ResourceState::Undefined;
            return Status{};
        }

        // Release GPU resources for a transient resource (no-op otherwise).
        void releaseTransient(rhi::Device& device)
        {
            if (lifetime != RGResourceLifetime::Transient) { return; }
            if (depthOnlyView != nullptr) { device.destroyTextureView(depthOnlyView); }
            if (textureView != nullptr) { device.destroyTextureView(textureView); }
            if (texture != nullptr) { device.destroyTexture(texture); }
            if (buffer != nullptr) { device.destroyBuffer(buffer); }
        }

        [[nodiscard]] u32 totalMipLevels() const
        {
            if (texture != nullptr) { return texture->desc.mipLevelCount; }
            if (resourceType == RGResourceType::Texture) { return textureDesc.mipLevelCount; }
            return 1;
        }
        [[nodiscard]] u32 totalArrayLayers() const
        {
            if (texture != nullptr) { return texture->desc.arrayLayerCount; }
            if (resourceType == RGResourceType::Texture) { return textureDesc.arrayLayerCount; }
            return 1;
        }

        void resetTracking()
        {
            refCount = 0;
            firstWriter = PassHandle::invalid();
            lastReader = PassHandle::invalid();
            firstUsePass = -1;
            lastUsePass = -1;
        }

        // --- identity / lifetime ---
        std::u8string name;
        RGResourceType resourceType;
        RGResourceLifetime lifetime;
        u32 generation = 1;

        // --- reference tracking (computed during compile) ---
        i32 refCount = 0;
        PassHandle firstWriter = PassHandle::invalid();
        PassHandle lastReader = PassHandle::invalid();
        i32 firstUsePass = -1; // for aliasing
        i32 lastUsePass = -1;

        // --- texture data ---
        RGTextureDesc textureDesc;
        rhi::Texture* texture = nullptr;
        rhi::TextureView* textureView = nullptr;
        rhi::TextureView* depthOnlyView = nullptr;
        // Stable id of the backing physical texture; changes when a transient is (re)allocated a
        // different texture (e.g. on resize). Consumers caching a bind group over textureView key on
        // this so a reused-address view does not alias a stale, destroyed texture.
        u64 textureGeneration = 0;

        // --- buffer data ---
        RGBufferDesc bufferDesc;
        rhi::Buffer* buffer = nullptr;

        // --- state tracking ---
        rhi::ResourceState lastKnownState = rhi::ResourceState::Undefined;
        std::optional<rhi::ResourceState> finalState;     // transition-to after last use (imported)
        bool readableAfterWrite = false;             // transition to ShaderRead after last writer

        // --- persistent data (null for transient/imported) ---
        std::unique_ptr<PersistentResource> persistentData;
    };
}
