#include <doctest_with_main.h>
#include <unordered_map>
#include <cstddef>


import core;
import rhi;
import rendergraph;

using namespace draco;
using namespace draco::rendergraph;
namespace rhi = draco::rhi;

namespace std {
    template <> struct hash<draco::rendergraph::RGHandle> {
        [[nodiscard]] std::size_t operator()(const draco::rendergraph::RGHandle& h) const noexcept {
            return (static_cast<std::size_t>(h.index) * 1099511628211ull) ^ h.generation;
        }
    };
}


TEST_CASE("rg.type: handle equality")
{
    CHECK(RGHandle{ 1, 1 } == RGHandle{ 1, 1 });
    CHECK(RGHandle{ 1, 1 } != RGHandle{ 2, 1 });
    CHECK(RGHandle{ 1, 1 } != RGHandle{ 1, 2 });   // generation matters
}

TEST_CASE("rg.type: handle validity")
{
    CHECK_FALSE(RGHandle::invalid().isValid());
    CHECK(RGHandle{ 0, 1 }.isValid());
    CHECK_FALSE(PassHandle::invalid().isValid());
    CHECK(PassHandle{ 0 }.isValid());
}

TEST_CASE("rg.type: handle works as a hash-map key")
{
    std::unordered_map<RGHandle, i32> map;
    map.insert_or_assign(RGHandle{ 1, 1 }, 7);
    const i32* found = mapFind(map, RGHandle{ 1, 1 });   // equal handle
    REQUIRE(found != nullptr);
    CHECK(*found == 7);
    CHECK(mapFind(map, RGHandle{ 2, 1 }) == nullptr);
}

TEST_CASE("rg.type: access IsRead / IsWrite")
{
    CHECK(isRead(RGAccessType::ReadTexture));
    CHECK(isRead(RGAccessType::ReadBuffer));
    CHECK(isRead(RGAccessType::ReadDepthStencil));
    CHECK(isRead(RGAccessType::SampleDepthStencil));
    CHECK(isRead(RGAccessType::ReadCopySrc));
    CHECK(isRead(RGAccessType::ReadWriteStorage));
    CHECK_FALSE(isRead(RGAccessType::WriteColorTarget));
    CHECK_FALSE(isRead(RGAccessType::WriteStorage));

    CHECK(isWrite(RGAccessType::WriteColorTarget));
    CHECK(isWrite(RGAccessType::WriteDepthTarget));
    CHECK(isWrite(RGAccessType::WriteStorage));
    CHECK(isWrite(RGAccessType::WriteCopyDst));
    CHECK(isWrite(RGAccessType::ReadWriteStorage));
    CHECK_FALSE(isWrite(RGAccessType::ReadTexture));
    CHECK_FALSE(isWrite(RGAccessType::ReadBuffer));
    CHECK_FALSE(isWrite(RGAccessType::SampleDepthStencil));
}

TEST_CASE("rg.type: access -> resource state")
{
    using RS = rhi::ResourceState;
    CHECK(toResourceState(RGAccessType::ReadTexture) == RS::ShaderRead);
    CHECK(toResourceState(RGAccessType::WriteColorTarget) == RS::RenderTarget);
    CHECK(toResourceState(RGAccessType::WriteDepthTarget) == RS::DepthStencilWrite);
    CHECK(toResourceState(RGAccessType::ReadDepthStencil) == RS::DepthStencilRead);
    // Sampling a depth texture in a shader uses the same read-only depth layout as a
    // read-only depth attachment (DEPTH_STENCIL_READ_ONLY_OPTIMAL), not ShaderRead.
    CHECK(toResourceState(RGAccessType::SampleDepthStencil) == RS::DepthStencilRead);
    CHECK(toResourceState(RGAccessType::ReadCopySrc) == RS::CopySrc);
    CHECK(toResourceState(RGAccessType::WriteCopyDst) == RS::CopyDst);
    CHECK(toResourceState(RGAccessType::WriteStorage) == RS::ShaderWrite);
}

TEST_CASE("rg.type: subresource All + overlap")
{
    const RGSubresourceRange all = RGSubresourceRange::all();
    CHECK(all.isAll());
    CHECK(all.baseMipLevel == 0u);
    CHECK(all.mipLevelCount == 0u);

    const RGSubresourceRange layer0{ 0, 1, 0, 1 };
    const RGSubresourceRange layer1{ 0, 1, 1, 1 };
    CHECK_FALSE(layer0.overlaps(layer1, 1, 4));
    CHECK(all.overlaps(layer0, 1, 4));
    CHECK(all.overlaps(layer1, 1, 4));
    CHECK(layer0.overlaps(layer0, 1, 4));

    const RGSubresourceRange mip0{ 0, 1, 0, 0 };
    const RGSubresourceRange mip1{ 1, 1, 0, 0 };
    CHECK_FALSE(mip0.overlaps(mip1, 4, 1));
    CHECK(mip0.overlaps(mip0, 4, 1));
}
