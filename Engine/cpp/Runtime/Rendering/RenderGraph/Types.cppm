// Draconic::RenderGraph - :types partition
//
// Core value types: resource/pass handles, pass + access enums, subresource
// ResourceState, the currency the barrier solver works in.

module;

#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

export module rendergraph:types;

import core;
import rhi;

using namespace draco;

export namespace draco::rendergraph
{
    namespace rhi = draco::rhi;

    // NOTE: local stand-in. Replace with a core container helper once draco.core
    // provides a map with pointer-or-null lookup (Sedulous's HashMap::Find).
    // Look up a key in a std map/set-like container, returning a pointer to the
    // mapped value or nullptr (mirrors the pointer-or-null lookup the solver uses).
    template <class Map, class Key>
    [[nodiscard]] auto* mapFind(Map& m, const Key& k) noexcept
    {
        auto it = m.find(k);
        using P = decltype(&it->second);
        return it != m.end() ? &it->second : P{nullptr};
    }

    // NOTE: local stand-in. Replace with a core text-formatting facility once
    // draco.core provides one (Sedulous's AppendFormat). Substitutes each "{}" in
    // `fmt` with the next argument (integers, floats, and UTF-8 string views).
    namespace detail
    {
        inline void rgAppendArg(std::u8string& out, std::u8string_view v) { out.append(v); }
        template <class T> requires std::is_arithmetic_v<T>
        void rgAppendArg(std::u8string& out, T v)
        {
            const std::string s = std::to_string(v);
            out.append(reinterpret_cast<const char8_t*>(s.data()), s.size());
        }
    }

    inline void appendFormat(std::u8string& out, std::u8string_view fmt) { out.append(fmt); }

    template <class A, class... Rest>
    void appendFormat(std::u8string& out, std::u8string_view fmt, A&& a, Rest&&... rest)
    {
        const auto pos = fmt.find(u8"{}");
        if (pos == std::u8string_view::npos) { out.append(fmt); return; }
        out.append(fmt.substr(0, pos));
        detail::rgAppendArg(out, std::forward<A>(a));
        appendFormat(out, fmt.substr(pos + 2), std::forward<Rest>(rest)...);
    }

    // Handle to a graph resource (texture or buffer); generation-checked for staleness.
    struct RGHandle
    {
        u32 index = 0xFFFFFFFFu;
        u32 generation = 0;

        [[nodiscard]] static constexpr RGHandle invalid() noexcept { return RGHandle{ 0xFFFFFFFFu, 0 }; }
        [[nodiscard]] constexpr bool isValid() const noexcept { return index != 0xFFFFFFFFu; }
    };

    [[nodiscard]] constexpr bool operator==(RGHandle a, RGHandle b) noexcept
    {
        return a.index == b.index && a.generation == b.generation;
    }
    [[nodiscard]] constexpr bool operator!=(RGHandle a, RGHandle b) noexcept { return !(a == b); }

    // Handle to a graph pass.
    struct PassHandle
    {
        u32 index = 0xFFFFFFFFu;

        [[nodiscard]] static constexpr PassHandle invalid() noexcept { return PassHandle{ 0xFFFFFFFFu }; }
        [[nodiscard]] constexpr bool isValid() const noexcept { return index != 0xFFFFFFFFu; }
    };

    [[nodiscard]] constexpr bool operator==(PassHandle a, PassHandle b) noexcept { return a.index == b.index; }
    [[nodiscard]] constexpr bool operator!=(PassHandle a, PassHandle b) noexcept { return a.index != b.index; }

    enum class RGPassType : u8 { Render, Compute, Copy };

    // Type of resource access declared by a pass.
    enum class RGAccessType : u8
    {
        // Reads
        ReadTexture, ReadBuffer, ReadDepthStencil, ReadCopySrc,
        // Sample a DEPTH texture in a shader: like ReadTexture (sampled, not an attachment) but the
        // layout must be DepthStencilRead (DEPTH_STENCIL_READ_ONLY_OPTIMAL), not ShaderRead.
        SampleDepthStencil,
        // Writes
        WriteColorTarget, WriteDepthTarget, WriteStorage, WriteCopyDst,
        // Read + Write
        ReadWriteStorage, ReadWriteDepthTarget, ReadWriteColorTarget,
    };

    [[nodiscard]] constexpr bool isRead(RGAccessType type) noexcept
    {
        switch (type)
        {
            case RGAccessType::ReadTexture:
            case RGAccessType::ReadBuffer:
            case RGAccessType::ReadDepthStencil:
            case RGAccessType::SampleDepthStencil:
            case RGAccessType::ReadCopySrc:
            case RGAccessType::ReadWriteStorage:
            case RGAccessType::ReadWriteDepthTarget:
            case RGAccessType::ReadWriteColorTarget:
                return true;
            default:
                return false;
        }
    }

    [[nodiscard]] constexpr bool isWrite(RGAccessType type) noexcept
    {
        switch (type)
        {
            case RGAccessType::WriteColorTarget:
            case RGAccessType::WriteDepthTarget:
            case RGAccessType::WriteStorage:
            case RGAccessType::WriteCopyDst:
            case RGAccessType::ReadWriteStorage:
            case RGAccessType::ReadWriteDepthTarget:
            case RGAccessType::ReadWriteColorTarget:
                return true;
            default:
                return false;
        }
    }

    [[nodiscard]] constexpr rhi::ResourceState toResourceState(RGAccessType type) noexcept
    {
        using RS = rhi::ResourceState;
        switch (type)
        {
            case RGAccessType::ReadTexture:           return RS::ShaderRead;
            case RGAccessType::ReadBuffer:            return RS::ShaderRead;
            case RGAccessType::ReadDepthStencil:      return RS::DepthStencilRead;
            case RGAccessType::SampleDepthStencil:    return RS::DepthStencilRead;
            case RGAccessType::ReadCopySrc:           return RS::CopySrc;
            case RGAccessType::WriteColorTarget:      return RS::RenderTarget;
            case RGAccessType::WriteDepthTarget:      return RS::DepthStencilWrite;
            case RGAccessType::WriteStorage:          return RS::ShaderWrite;
            case RGAccessType::WriteCopyDst:          return RS::CopyDst;
            case RGAccessType::ReadWriteStorage:      return RS::ShaderWrite | RS::ShaderRead;
            case RGAccessType::ReadWriteDepthTarget:  return RS::DepthStencilWrite;
            case RGAccessType::ReadWriteColorTarget:  return RS::RenderTarget;
        }
        return RS::Undefined;
    }

    // Subresource range for fine-grained access tracking (e.g. shadow cascades).
    // A count of 0 means "all remaining from the base".
    struct RGSubresourceRange
    {
        u32 baseMipLevel = 0;
        u32 mipLevelCount = 0;
        u32 baseArrayLayer = 0;
        u32 arrayLayerCount = 0;

        [[nodiscard]] static constexpr RGSubresourceRange all() noexcept { return RGSubresourceRange{}; }

        [[nodiscard]] constexpr bool isAll() const noexcept
        {
            return baseMipLevel == 0 && mipLevelCount == 0 && baseArrayLayer == 0 && arrayLayerCount == 0;
        }

        [[nodiscard]] constexpr bool overlaps(RGSubresourceRange other, u32 totalMips = 1, u32 totalLayers = 1) const noexcept
        {
            const u32 myMipEnd = mipLevelCount == 0 ? totalMips : baseMipLevel + mipLevelCount;
            const u32 otherMipEnd = other.mipLevelCount == 0 ? totalMips : other.baseMipLevel + other.mipLevelCount;
            const u32 myLayerEnd = arrayLayerCount == 0 ? totalLayers : baseArrayLayer + arrayLayerCount;
            const u32 otherLayerEnd = other.arrayLayerCount == 0 ? totalLayers : other.baseArrayLayer + other.arrayLayerCount;

            const bool mipOverlap = baseMipLevel < otherMipEnd && other.baseMipLevel < myMipEnd;
            const bool layerOverlap = baseArrayLayer < otherLayerEnd && other.baseArrayLayer < myLayerEnd;
            return mipOverlap && layerOverlap;
        }
    };

    [[nodiscard]] constexpr bool operator==(RGSubresourceRange a, RGSubresourceRange b) noexcept
    {
        return a.baseMipLevel == b.baseMipLevel && a.mipLevelCount == b.mipLevelCount
            && a.baseArrayLayer == b.baseArrayLayer && a.arrayLayerCount == b.arrayLayerCount;
    }
    [[nodiscard]] constexpr bool operator!=(RGSubresourceRange a, RGSubresourceRange b) noexcept { return !(a == b); }

    // A single resource access declared by a pass.
    struct RGResourceAccess
    {
        RGHandle handle = RGHandle::invalid();
        RGAccessType type = RGAccessType::ReadTexture;
        RGSubresourceRange subresource;

        [[nodiscard]] bool isRead() const noexcept { return rendergraph::isRead(type); }
        [[nodiscard]] bool isWrite() const noexcept { return rendergraph::isWrite(type); }
        [[nodiscard]] rhi::ResourceState toResourceState() const noexcept { return rendergraph::toResourceState(type); }
    };

    // How transient resource dimensions resolve relative to the graph output.
    enum class SizeMode : u8 { FullSize, HalfSize, QuarterSize, Custom };

    enum class RGResourceLifetime : u8 { Transient, Persistent, Imported };

    enum class RGResourceType : u8 { Texture, Buffer };
}
