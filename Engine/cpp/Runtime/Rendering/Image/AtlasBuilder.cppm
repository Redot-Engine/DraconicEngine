// Draconic::Image - :atlas_builder partition.
//
// RectI (integer rectangle for atlas regions) and ImageAtlasBuilder - a
// general-purpose shelf-packing atlas packer that combines multiple RGBA8
// images into one atlas texture (UI themes, sprite sheets, ...). Ported from
// Sedulous.Images/ImageAtlasBuilder.bf.

module;

#include <cstring>

#include <span>
#include <vector>
#include <string>
#include <string_view>
#include <unordered_map>

export module image:atlas_builder;

import core;
import :pixel_format;
import :image_data;

using namespace draco;

export namespace draco::image
{
    /// Integer rectangle for atlas regions.
    struct RectI
    {
        i32 x = 0;
        i32 y = 0;
        i32 width = 0;
        i32 height = 0;

        constexpr RectI() noexcept = default;
        constexpr RectI(i32 inX, i32 inY, i32 w, i32 h) noexcept : x(inX), y(inY), width(w), height(h) {}
    };

    /// General-purpose image atlas packer (shelf packing). Combines RGBA8 images
    /// into one atlas texture. Images are not owned (caller keeps them alive).
    class ImageAtlasBuilder
    {
    public:
        /// minSize/maxSize: atlas dimensions (rounded up to powers of two).
        /// padding: pixels between packed images.
        explicit ImageAtlasBuilder(u32 minSize = 256, u32 maxSize = 4096, u32 padding = 1)
            : m_minSize(nextPowerOf2(minSize)), m_maxSize(nextPowerOf2(maxSize)), m_padding(padding) {}

        /// The built atlas image. Null until build() succeeds.
        [[nodiscard]] const ImageData* atlas() const { return m_built ? &m_atlas : nullptr; }

        /// Number of entries added.
        [[nodiscard]] usize entryCount() const { return m_entries.size(); }

        /// Add an image to be packed. Name must be unique. Image is not owned.
        void addImage(std::u8string_view name, const ImageData* image)
        {
            if (image == nullptr) return;
            Entry entry;
            entry.name = std::u8string(name);
            entry.image = image;
            m_entries.push_back(std::move(entry));
        }

        /// Pack all added images into a single RGBA8 atlas. Returns true on success.
        bool build()
        {
            if (m_entries.empty())
            {
                const u8 emptyPixel[4] = { 0, 0, 0, 0 };
                m_atlas = OwnedImageData(1, 1, PixelFormat::RGBA8, std::span<const u8>(emptyPixel, 4));
                m_built = true;
                return true;
            }

            // Sort by height descending for better shelf packing.
            sortByHeightDesc();

            // Try increasing atlas sizes until everything fits.
            for (u32 size = m_minSize; size <= m_maxSize; size *= 2)
            {
                if (tryPack(size, size))
                {
                    m_built = true;
                    return true;
                }
            }
            return false; // Couldn't fit in max size.
        }

        /// The pixel-space region of a packed image by name (null if not found).
        [[nodiscard]] const RectI* getRegion(std::u8string_view name) const {
            auto it = m_regions.find(std::u8string(name));
            return it != m_regions.end() ? &it->second : nullptr;
        }

    private:
        struct Entry
        {
            std::u8string name;
            const ImageData* image = nullptr;
        };

        void sortByHeightDesc()
        {
            // Insertion sort (entry counts are small); stable-ish, descending height.
            for (usize i = 1; i < m_entries.size(); ++i)
            {
                Entry key = std::move(m_entries[i]);
                const u32 keyH = key.image->height();
                usize j = i;
                while (j > 0 && m_entries[j - 1].image->height() < keyH)
                {
                    m_entries[j] = std::move(m_entries[j - 1]);
                    --j;
                }
                m_entries[j] = std::move(key);
            }
        }

        bool tryPack(u32 atlasW, u32 atlasH)
        {
            m_regions.clear();

            // Shelf packing: place images left-to-right, start a new row when full.
            u32 curX = m_padding;
            u32 curY = m_padding;
            u32 rowHeight = 0;

            for (usize e = 0; e < m_entries.size(); ++e)
            {
                const Entry& entry = m_entries[e];
                const u32 imgW = entry.image->width();
                const u32 imgH = entry.image->height();

                if (curX + imgW + m_padding > atlasW)
                {
                    curX = m_padding;
                    curY += rowHeight + m_padding;
                    rowHeight = 0;
                }

                if (curY + imgH + m_padding > atlasH)
                    return false;

                m_regions.insert_or_assign(std::u8string(entry.name),
                    RectI{ static_cast<i32>(curX), static_cast<i32>(curY), static_cast<i32>(imgW), static_cast<i32>(imgH) });

                curX += imgW + m_padding;
                rowHeight = std::max(rowHeight, imgH);
            }

            // Build the atlas pixel data.
            std::vector<u8> pixelData;
            pixelData.resize(static_cast<usize>(atlasW) * atlasH * 4);
            std::memset(pixelData.data(), 0, pixelData.size());

            for (usize e = 0; e < m_entries.size(); ++e)
            {
                const Entry& entry = m_entries[e];
                auto regionIt = m_regions.find(std::u8string(entry.name));
                const RectI* region = regionIt != m_regions.end() ? &regionIt->second : nullptr;
                if (region == nullptr) continue;

                const ImageData* src = entry.image;
                if (src->format() == PixelFormat::RGBA8)
                {
                    const std::span<const u8> srcData = src->pixelData();
                    const u32 srcStride = src->width() * 4;
                    const u32 dstStride = atlasW * 4;

                    for (u32 y = 0; y < src->height(); ++y)
                    {
                        const u32 srcOffset = y * srcStride;
                        const u32 dstOffset = (static_cast<u32>(region->y) + y) * dstStride + static_cast<u32>(region->x) * 4;

                        if (srcOffset + srcStride <= srcData.size() && dstOffset + srcStride <= pixelData.size())
                            std::memcpy(pixelData.data() + dstOffset, srcData.data() + srcOffset, srcStride);
                    }
                }
            }

            m_atlas = OwnedImageData(atlasW, atlasH, PixelFormat::RGBA8, std::move(pixelData));
            return true;
        }

        [[nodiscard]] static u32 nextPowerOf2(u32 v)
        {
            u32 n = v;
            --n;
            n |= n >> 1;
            n |= n >> 2;
            n |= n >> 4;
            n |= n >> 8;
            n |= n >> 16;
            ++n;
            return std::max(n, 1u);
        }

        std::vector<Entry> m_entries;
        OwnedImageData m_atlas;
        std::unordered_map<std::u8string, RectI> m_regions;
        u32 m_minSize;
        u32 m_maxSize;
        u32 m_padding;
        bool m_built = false;
    };
}
