/// Core RHI types: geometry primitives, adapter info, device features,
/// device descriptor, copy regions, and push constant ranges.

module;

#include <string>
#include <string_view>

export module rhi:types;

import core.stdtypes;
import :enums;
import :texture_format;

using namespace draco;

export namespace draco::rhi {

/// Maximum number of simultaneous color attachments.
constexpr i32 maxColorAttachments = 8;

// ---- Geometry primitives ----

struct Extent3D {
    u32 width  = 0;
    u32 height = 1;
    u32 depth  = 1;
};

struct Origin3D {
    u32 x = 0;
    u32 y = 0;
    u32 z = 0;
};

/// RGBA clear color in linear space.
struct ClearColor {
    f32 r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f;

    static constexpr ClearColor black()          { return { 0, 0, 0, 1 }; }
    static constexpr ClearColor white()          { return { 1, 1, 1, 1 }; }
    static constexpr ClearColor cornflowerBlue() { return { 0.392f, 0.584f, 0.929f, 1.0f }; }
};

// ---- Adapter / Device info ----

/// Information about a physical GPU adapter.
struct AdapterInfo {
    std::u8string name;
    u32         vendorId = 0;
    u32         deviceId = 0;
    AdapterType type     = AdapterType::Unknown;

    /// Features and limits the adapter supports. Populate via the backend's
    /// adapter enumeration; the caller inspects these before creating a device.
    struct Features {
        // Feature flags.
        bool bindlessDescriptors      = false;
        bool timestampQueries         = false;
        bool pipelineStatisticsQueries= false;
        bool multiDrawIndirect        = false;
        bool depthClamp               = false;
        bool fillModeWireframe        = false;
        bool textureCompressionBC     = false;
        bool textureCompressionASTC   = false;
        bool independentBlend         = false;
        bool multiViewport            = false;
        bool meshShaders              = false;
        bool rayTracing               = false;

        // Limits.
        u32 maxBindGroups                    = 4;
        u32 maxBindingsPerGroup              = 16;
        u32 maxPushConstantSize              = 128;
        u32 maxTextureDimension2D            = 8192;
        u32 maxTextureArrayLayers            = 256;
        u32 maxComputeWorkgroupSizeX         = 256;
        u32 maxComputeWorkgroupSizeY         = 256;
        u32 maxComputeWorkgroupSizeZ         = 64;
        u32 maxComputeWorkgroupsPerDimension = 65535;
        u32 minUniformBufferOffsetAlignment  = 256;
        u32 minStorageBufferOffsetAlignment  = 256;
        u32 timestampPeriodNs                = 1;
        u64 maxBufferSize                    = 256ull * 1024 * 1024;

        // Mesh shader limits.
        u32 maxMeshOutputVertices   = 0;
        u32 maxMeshOutputPrimitives = 0;
        u32 maxMeshWorkgroupSize    = 0;
        u32 maxTaskWorkgroupSize    = 0;
    } supportedFeatures;
};

/// Features and limits requested when creating a device.
using DeviceFeatures = AdapterInfo::Features;

/// Descriptor for logical device creation.
struct DeviceDesc {
    DeviceFeatures requiredFeatures{};
    u32 graphicsQueueCount = 1;
    u32 computeQueueCount  = 0;
    u32 transferQueueCount = 0;
    std::u8string_view label;
};

// ---- Copy regions ----

/// Layout of a buffer holding texture data (for CPU->GPU uploads).
struct TextureDataLayout {
    u64 offset      = 0;
    u32 bytesPerRow = 0;
    u32 rowsPerImage = 0;
};

/// Region for texture-to-texture copies.
struct TextureCopyRegion {
    u32 srcMipLevel    = 0;
    u32 srcArrayLayer  = 0;
    u32 dstMipLevel    = 0;
    u32 dstArrayLayer  = 0;
    Extent3D extent;
};

/// Region for buffer<->texture copies.
struct BufferTextureCopyRegion {
    u64 bufferOffset      = 0;
    u32 bytesPerRow       = 0;
    u32 rowsPerImage      = 0;
    u32 textureMipLevel   = 0;
    u32 textureArrayLayer = 0;
    Origin3D textureOrigin;
    Extent3D textureExtent;
};

/// Push constant range within a pipeline layout.
struct PushConstantRange {
    ShaderStage stages = ShaderStage::None;
    u32 offset = 0;
    u32 size   = 0;
};

} // namespace draco::rhi
