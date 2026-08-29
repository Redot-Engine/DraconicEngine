/// Reusable depth buffer helper for samples.

export module samples.rhi.framework:depth_buffer;

import core.stdtypes;
import core.status;
import rhi;

using namespace draco;

export namespace draco::samples::framework {

struct DepthBuffer {
    rhi::Texture*     texture = nullptr;
    rhi::TextureView* view    = nullptr;
    rhi::TextureFormat format = rhi::TextureFormat::Depth24PlusStencil8;

    Status recreate(rhi::Device* device, u32 width, u32 height, u32 sampleCount = 1) {
        destroy(device);
        auto desc = rhi::TextureDesc::depthBuffer(format, width, height, sampleCount, u8"Depth");
        if (device->createTexture(desc, texture) != ErrorCode::Ok) return ErrorCode::Unknown;

        rhi::TextureViewDesc vd{};
        vd.format           = format;
        vd.dimension        = rhi::TextureViewDimension::Texture2D;
        vd.baseMipLevel     = 0;
        vd.mipLevelCount    = 1;
        vd.baseArrayLayer   = 0;
        vd.arrayLayerCount  = 1;
        vd.label            = u8"DepthView";
        if (device->createTextureView(texture, vd, view) != ErrorCode::Ok) {
            device->destroyTexture(texture);
            return ErrorCode::Unknown;
        }
        return ErrorCode::Ok;
    }

    void destroy(rhi::Device* device) {
        if (view)    { device->destroyTextureView(view);  view    = nullptr; }
        if (texture) { device->destroyTexture(texture);   texture = nullptr; }
    }
};

} // namespace draco::samples::framework
