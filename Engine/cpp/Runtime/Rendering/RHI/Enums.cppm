// All RHI enumerations and flag operators.

export module rhi:enums;

import core.stdtypes;

using namespace draco;

export namespace draco::rhi {

enum class DeviceType : u32 { Vulkan, DX12, Null };
enum class QueueType  : u32 { Graphics, Compute, Transfer };

enum class BufferUsage : u32 {
    None             = 0,
    CopySrc          = 1 << 0,
    CopyDst          = 1 << 1,
    Vertex           = 1 << 2,
    Index            = 1 << 3,
    Uniform          = 1 << 4,
    Storage          = 1 << 5,
    StorageRead      = 1 << 6,
    Indirect         = 1 << 7,
    AccelStructInput = 1 << 8,
    ShaderBindingTable = 1 << 9,
    AccelStructScratch = 1 << 10,
};
inline constexpr BufferUsage operator|(BufferUsage a, BufferUsage b) { return static_cast<BufferUsage>(static_cast<u32>(a) | static_cast<u32>(b)); }
inline constexpr BufferUsage operator&(BufferUsage a, BufferUsage b) { return static_cast<BufferUsage>(static_cast<u32>(a) & static_cast<u32>(b)); }
inline constexpr bool hasFlag(BufferUsage v, BufferUsage f) { return (v & f) == f; }

enum class TextureUsage : u32 {
    None            = 0,
    CopySrc         = 1 << 0,
    CopyDst         = 1 << 1,
    Sampled         = 1 << 2,
    Storage         = 1 << 3,
    RenderTarget    = 1 << 4,
    DepthStencil    = 1 << 5,
    InputAttachment = 1 << 6,
};
inline constexpr TextureUsage operator|(TextureUsage a, TextureUsage b) { return static_cast<TextureUsage>(static_cast<u32>(a) | static_cast<u32>(b)); }
inline constexpr TextureUsage operator&(TextureUsage a, TextureUsage b) { return static_cast<TextureUsage>(static_cast<u32>(a) & static_cast<u32>(b)); }

enum class MemoryLocation : u32 { GpuOnly, CpuToGpu, GpuToCpu, Auto };

enum class ResourceState : u32 {
    Undefined        = 0,
    VertexBuffer     = 1 << 0,
    IndexBuffer      = 1 << 1,
    UniformBuffer    = 1 << 2,
    ShaderRead       = 1 << 3,
    ShaderWrite      = 1 << 4,
    RenderTarget     = 1 << 5,
    DepthStencilWrite= 1 << 6,
    DepthStencilRead = 1 << 7,
    IndirectArgument = 1 << 8,
    CopySrc          = 1 << 9,
    CopyDst          = 1 << 10,
    Present          = 1 << 11,
    InputAttachment  = 1 << 12,
    General          = 1 << 13,
    AccelStructRead  = 1 << 14,
    AccelStructWrite = 1 << 15,
};
inline constexpr ResourceState operator|(ResourceState a, ResourceState b) { return static_cast<ResourceState>(static_cast<u32>(a) | static_cast<u32>(b)); }
inline constexpr ResourceState operator&(ResourceState a, ResourceState b) { return static_cast<ResourceState>(static_cast<u32>(a) & static_cast<u32>(b)); }

enum class TextureDimension : u32 { Texture1D, Texture2D, Texture3D };
enum class TextureViewDimension : u32 { Texture1D, Texture1DArray, Texture2D, Texture2DArray, TextureCube, TextureCubeArray, Texture3D };

enum class TextureAspect : u32 { All = 0, DepthOnly = 1, StencilOnly = 2 };

enum class ShaderStage : u32 {
    None         = 0,
    Vertex       = 1 << 0,
    Fragment     = 1 << 1,
    Compute      = 1 << 2,
    Mesh         = 1 << 3,
    Task         = 1 << 4,
    RayGen       = 1 << 5,
    ClosestHit   = 1 << 6,
    Miss         = 1 << 7,
    AnyHit       = 1 << 8,
    Intersection = 1 << 9,
    Callable     = 1 << 10,
    AllGraphics  = Vertex | Fragment,
    All          = 0x7FF,
};
inline constexpr ShaderStage operator|(ShaderStage a, ShaderStage b) { return static_cast<ShaderStage>(static_cast<u32>(a) | static_cast<u32>(b)); }
inline constexpr ShaderStage operator&(ShaderStage a, ShaderStage b) { return static_cast<ShaderStage>(static_cast<u32>(a) & static_cast<u32>(b)); }

enum class BindingType : u32 {
    UniformBuffer, StorageBufferReadOnly, StorageBufferReadWrite,
    SampledTexture, StorageTextureReadOnly, StorageTextureReadWrite,
    Sampler, ComparisonSampler,
    BindlessTextures, BindlessSamplers, BindlessStorageBuffers, BindlessStorageTextures,
    AccelerationStructure,
};

enum class FilterMode      : u32 { Nearest, Linear };
enum class MipmapFilterMode: u32 { Nearest, Linear };
enum class AddressMode     : u32 { Repeat, MirrorRepeat, ClampToEdge, ClampToBorder };
enum class SamplerBorderColor : u32 { TransparentBlack, OpaqueBlack, OpaqueWhite };

enum class PrimitiveTopology : u32 { PointList, LineList, LineStrip, TriangleList, TriangleStrip };
enum class FrontFace    : u32 { CCW, CW };
enum class CullMode     : u32 { None, Front, Back };
enum class FillMode     : u32 { Solid, Wireframe };

enum class CompareFunction : u32 { Never, Less, Equal, LessEqual, Greater, NotEqual, GreaterEqual, Always };
enum class StencilOperation: u32 { Keep, Zero, Replace, IncrementClamp, DecrementClamp, Invert, IncrementWrap, DecrementWrap };

enum class BlendFactor : u32 {
    Zero, One, Src, OneMinusSrc, SrcAlpha, OneMinusSrcAlpha,
    Dst, OneMinusDst, DstAlpha, OneMinusDstAlpha,
    SrcAlphaSaturated, Constant, OneMinusConstant,
};
enum class BlendOperation : u32 { Add, Subtract, ReverseSubtract, Min, Max };

enum class LoadOp  : u32 { Load, Clear, DontCare };
enum class StoreOp : u32 { Store, DontCare };

// How a render pass's draw commands are supplied. `Inline` records draws directly into the
// pass (the default). `SecondaryCommandBuffers` means the pass body is supplied by executed
// render bundles only (no inline draws) - Vulkan begins the rendering scope with the secondary-
// command-buffer contents flag; DX12 / WebGPU ignore it (they allow bundles in any pass).
enum class RenderPassContents : u32 { Inline, SecondaryCommandBuffers };

enum class IndexFormat   : u32 { UInt16, UInt32 };
enum class VertexStepMode: u32 { Vertex, Instance };

enum class VertexFormat : u32 {
    Uint8x2, Uint8x4, Sint8x2, Sint8x4,
    Unorm8x2, Unorm8x4, Snorm8x2, Snorm8x4,
    Uint16x2, Uint16x4, Sint16x2, Sint16x4,
    Unorm16x2, Unorm16x4, Snorm16x2, Snorm16x4,
    Float16x2, Float16x4,
    Float32, Float32x2, Float32x3, Float32x4,
    Uint32, Uint32x2, Uint32x3, Uint32x4,
    Sint32, Sint32x2, Sint32x3, Sint32x4,
};

enum class ColorWriteMask : u8 {
    None  = 0,
    Red   = 1 << 0,
    Green = 1 << 1,
    Blue  = 1 << 2,
    Alpha = 1 << 3,
    All   = Red | Green | Blue | Alpha,
};
inline constexpr ColorWriteMask operator|(ColorWriteMask a, ColorWriteMask b) { return static_cast<ColorWriteMask>(static_cast<u8>(a) | static_cast<u8>(b)); }

enum class FormatSupport : u32 {
    Unsupported        = 0,
    Texture            = 1 << 0,
    StorageTexture     = 1 << 1,
    ColorAttachment    = 1 << 2,
    DepthStencil       = 1 << 3,
    Buffer             = 1 << 4,
    StorageBuffer      = 1 << 5,
    VertexBuffer       = 1 << 6,
    BlendableColor     = 1 << 7,
    LinearFilter       = 1 << 8,
};
inline constexpr FormatSupport operator|(FormatSupport a, FormatSupport b) { return static_cast<FormatSupport>(static_cast<u32>(a) | static_cast<u32>(b)); }
inline constexpr FormatSupport operator&(FormatSupport a, FormatSupport b) { return static_cast<FormatSupport>(static_cast<u32>(a) & static_cast<u32>(b)); }

enum class PresentMode  : u32 { Immediate, Mailbox, Fifo, FifoRelaxed };
enum class AdapterType   : u32 { DiscreteGpu, IntegratedGpu, Cpu, Unknown };
enum class QueryType     : u32 { Timestamp, Occlusion, PipelineStatistics };

// Ray tracing enums.
enum class AccelStructType : u32 { TopLevel, BottomLevel };
enum class GeometryType    : u32 { Triangles, AABBs };

enum class GeometryFlags : u32 {
    None = 0,
    Opaque = 1 << 0,
    NoDuplicateAnyHitInvocation = 1 << 1,
};
inline constexpr GeometryFlags operator|(GeometryFlags a, GeometryFlags b) { return static_cast<GeometryFlags>(static_cast<u32>(a) | static_cast<u32>(b)); }

enum class AccelStructBuildFlags : u32 {
    None = 0,
    AllowUpdate      = 1 << 0,
    AllowCompaction  = 1 << 1,
    PreferFastTrace  = 1 << 2,
    PreferFastBuild  = 1 << 3,
};
inline constexpr AccelStructBuildFlags operator|(AccelStructBuildFlags a, AccelStructBuildFlags b) { return static_cast<AccelStructBuildFlags>(static_cast<u32>(a) | static_cast<u32>(b)); }

} // namespace draco::rhi
