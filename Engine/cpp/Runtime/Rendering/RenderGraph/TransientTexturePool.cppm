// Draconic::RenderGraph - :transient_pool partition
//
// Pools GPU textures for reuse across frames by transient resources, avoiding
// per-frame allocation thrashing. Matches by exact descriptor; ages out unused

module;

#include <vector>

export module rendergraph:transient_pool;

import core;
import rhi;

using namespace draco;

export namespace draco::rendergraph
{
    namespace rhi = draco::rhi;

    class TransientTexturePool
    {
    public:
        explicit TransientTexturePool(rhi::Device& device) noexcept : m_device(&device) {}
        ~TransientTexturePool() { destroyAll(); }

        TransientTexturePool(const TransientTexturePool&) = delete;
        TransientTexturePool& operator=(const TransientTexturePool&) = delete;

        i32 maxUnusedFrames = 4;

        // Acquire a matching texture from the pool; true if one was found. `generation` receives the
        // physical texture's stable id (preserved while it lives in the pool), so consumers can detect
        // when a transient is backed by a DIFFERENT physical texture (a freed view address can reuse).
        [[nodiscard]] bool tryAcquire(const rhi::TextureDesc& desc, rhi::Texture*& texture, rhi::TextureView*& view, u64& generation)
        {
            for (usize i = 0; i < m_pool.size(); ++i)
            {
                if (descriptorsMatch(m_pool[i].desc, desc))
                {
                    texture = m_pool[i].texture;
                    view = m_pool[i].view;
                    generation = m_pool[i].generation;
                    m_pool.erase(m_pool.begin() + i);
                    return true;
                }
            }
            texture = nullptr;
            view = nullptr;
            generation = 0;
            return false;
        }

        void returnToPool(const rhi::TextureDesc& desc, rhi::Texture* texture, rhi::TextureView* view, u64 generation)
        {
            m_pool.push_back(PooledTexture{ desc, texture, view, generation, 0 });
        }

        // Age out entries unused for more than maxUnusedFrames.
        void endFrame()
        {
            for (usize i = m_pool.size(); i-- > 0;)
            {
                ++m_pool[i].unusedFrames;
                if (m_pool[i].unusedFrames > maxUnusedFrames)
                {
                    rhi::Texture* tex = m_pool[i].texture;
                    rhi::TextureView* view = m_pool[i].view;
                    if (view != nullptr) { m_device->destroyTextureView(view); }
                    if (tex != nullptr) { m_device->destroyTexture(tex); }
                    m_pool.erase(m_pool.begin() + i);
                }
            }
        }

        void destroyAll()
        {
            for (PooledTexture& entry : m_pool)
            {
                if (entry.view != nullptr) { m_device->destroyTextureView(entry.view); }
                if (entry.texture != nullptr) { m_device->destroyTexture(entry.texture); }
            }
            m_pool.clear();
        }

    private:
        struct PooledTexture
        {
            rhi::TextureDesc desc;
            rhi::Texture* texture = nullptr;
            rhi::TextureView* view = nullptr;
            u64 generation = 0;     // stable id of this physical texture (carried across pool reuse)
            i32 unusedFrames = 0;
        };

        static bool descriptorsMatch(const rhi::TextureDesc& a, const rhi::TextureDesc& b) noexcept
        {
            return a.dimension == b.dimension
                && a.format == b.format
                && a.width == b.width
                && a.height == b.height
                && a.depth == b.depth
                && a.arrayLayerCount == b.arrayLayerCount
                && a.mipLevelCount == b.mipLevelCount
                && a.sampleCount == b.sampleCount
                && a.usage == b.usage;
        }

        rhi::Device* m_device;
        std::vector<PooledTexture> m_pool;
    };
}
