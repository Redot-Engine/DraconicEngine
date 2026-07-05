module;

#include <vector>
#include <algorithm>
#include <functional>
#include <print>
#include <cstdlib>

#include <bgfx/bgfx.h>

#include "macros.h"

export module rendering.rhi;

import core.stdtypes;
import core.math.constants;
import core.memory;
import rendering.rhi.vertex;

export namespace draco::rendering::rhi
{
    // Which native windowing system the handles passed to init() belong to. RHI
    // only needs this to tell bgfx when a surface is Wayland (a wl_surface* must
    // be flagged as such); everything else uses bgfx's default handling. The
    // caller maps its own window abstraction onto this.
    enum class NativeWindowType
    {
        Default,
        Win32,
        X11,
        Wayland,
        Cocoa,
    };

    struct BufferTag {};
    struct PipelineTag {};
    struct ShaderTag {};
    struct UniformTag {};
    struct TextureTag {};
    struct FramebufferTag {};
    struct LayoutTag {};

    using BufferHandle      = core::memory::Handle<BufferTag>;
    using PipelineHandle    = core::memory::Handle<PipelineTag>;
    using ShaderHandle      = core::memory::Handle<ShaderTag>;
    using UniformHandle     = core::memory::Handle<UniformTag>;
    using TextureHandle     = core::memory::Handle<TextureTag>;
    using FramebufferHandle = core::memory::Handle<FramebufferTag>;
    using LayoutHandle      = core::memory::Handle<LayoutTag>;

    using ViewID        = u16;   // bgfx native
    using SamplerHandle = u64;   // bgfx sampler flags

    inline constexpr BufferHandle      InvalidBuffer{};
    inline constexpr PipelineHandle    InvalidPipeline{};
    inline constexpr ShaderHandle      InvalidShader{};
    inline constexpr UniformHandle     InvalidUniform{};
    inline constexpr TextureHandle     InvalidTexture{};
    inline constexpr FramebufferHandle InvalidFramebuffer{};
    inline constexpr LayoutHandle      InvalidLayout{};
    inline constexpr SamplerHandle     InvalidSampler = 0;
    inline constexpr ViewID            InvalidView = math::UINT16_MAX_VAL;

    enum class PipelineState : u64 {
        Default             = 0,
        WriteRGB            = 1ULL << 0,
        WriteAlpha          = 1ULL << 1,
        MSAA                = 1ULL << 2,
        PrimitiveTriStrip   = 1ULL << 3,
    };

    enum class ClearFlags : u32 {
        Color   = BGFX_CLEAR_COLOR,
        Depth   = BGFX_CLEAR_DEPTH,
        Stencil = BGFX_CLEAR_STENCIL
        };

    struct ViewDesc {
        FramebufferHandle fb = InvalidFramebuffer;
        u16 x = 0, y = 0, w = 0, h = 0;
        u32 clearFlags = 0;
        u32 clearColor = 0;
    };

    enum class UniformType
    {
        Sampler,
        Vec4,
        Mat3,
        Mat4,
    };

    struct UniformBind {
        UniformHandle handle;
        const void* data;
        u16 num;
    };

    enum class TextureFormat {
        RGBA8,
        BGRA8,
        D16,
        D24,
        D24S8,
        D32
    };

    enum class BlendMode {
        None,
        Alpha,
        Additive,
        Multiply
    };

    enum class DepthTest {
        None,
        Less,
        Equal,
        Always
    };

    enum class CullMode {
        None,
        CW,
        CCW
    };

    struct Buffer
    {
        bgfx::VertexBufferHandle vbh = BGFX_INVALID_HANDLE;
        bgfx::DynamicVertexBufferHandle dvbh = BGFX_INVALID_HANDLE;
        bgfx::IndexBufferHandle  ibh = BGFX_INVALID_HANDLE;
        bgfx::DynamicIndexBufferHandle dibh;
        bool isDynamic = false;
        bool isIndex = false;
    };

    struct FramebufferResource {
        bgfx::FrameBufferHandle fbh;
        TextureHandle texture;
    };

    struct VertexLayoutResource
    {
        bgfx::VertexLayout layout;
    };

    struct ScissorRect {
        u16 x, y, w, h;
        bool enabled = true;
    };

    struct DeletionReq {
        u64 frame;
        std::function<void()> cleanup;
    };

    struct PipelineDesc
    {
        ShaderHandle vs;
        ShaderHandle fs;
        PipelineState state = PipelineState::Default;

        BlendMode blend = BlendMode::None;
        DepthTest depth = DepthTest::Less;
        CullMode  cull  = CullMode::CCW;

        bool depthWrite = true;
    };

    struct RenderPacket
    {
        u64 sortKey = 0;

        BufferHandle vertexBuffer = InvalidBuffer;
        BufferHandle indexBuffer  = InvalidBuffer;
        PipelineHandle pipeline    = InvalidPipeline;

        u32 vertexCount = math::UINT32_MAX_VAL;
        u32 indexCount  = math::UINT32_MAX_VAL;

        UniformHandle samplerUniform = InvalidUniform;
        SamplerHandle samplerFlags   = InvalidSampler;
        TextureHandle textureHandle  = InvalidTexture;

        f32 color[4] = {1,1,1,1};

        std::vector<UniformBind> uniforms;
        u8 textureUnit = 0;

        f32 model[16] = {
            1,0,0,0,
            0,1,0,0,
            0,0,1,0,
            0,0,0,1
        };

        u32 drawTags = 0;
    };

    struct Pipeline {
        bgfx::ProgramHandle program;
        u64 state;
    };

    bool init(void* display_type, void* window_handle, NativeWindowType window_type, u16 width, u16 height);
    void resize(u16 width, u16 height);
    void shutdown();

    PipelineHandle createPipeline(const PipelineDesc&);

    BufferHandle createVertexBuffer(const void* data, u32 size, LayoutHandle layout_h);
    BufferHandle createIndexBuffer(const void* data, u32 size);
    void destroyBuffer(BufferHandle handle);

    UniformHandle createUniform(const char* name, UniformType type, u16 num = 1);
    void destroyUniform(UniformHandle handle);
    void setUniform(UniformHandle handle, const void* value, u16 num = 1);

    TextureHandle createTexture(const void* data, u32 width, u32 height, u32 flags = 0);
    void destroyTexture(TextureHandle handle);

    FramebufferHandle createFramebuffer(u32 width, u32 height, TextureFormat format);
    void destroyFramebuffer(FramebufferHandle handle);
    TextureHandle getFramebufferTexture(FramebufferHandle handle);

    BufferHandle createDynamicVertexBuffer(u32 size, LayoutHandle layout);
    void updateDynamicVertexBuffer(BufferHandle handle, u32 start_vertex, const void* data, u32 size);

    BufferHandle createDynamicIndexBuffer(u32 size, u16 flags = BGFX_BUFFER_NONE);
    void updateDynamicIndexBuffer(BufferHandle handle, u32 start_index, const void* data, u32 size);

    LayoutHandle createVertexLayout(const VertexLayoutDesc& desc);

    SamplerHandle createSampler(bool linear, bool clamp);

    // Expects bgfx compiled shader binary (shaderc output)
    ShaderHandle createShader(const void* data, u32 size);
    bgfx::ShaderHandle resolve(ShaderHandle h);
    // For debugging/tooling
    bgfx::ShaderHandle* getShaderNative(ShaderHandle h);
    void destroyShader(ShaderHandle h);

    void perspective(f32* out, f32 fov, f32 aspect, f32 nearp, f32 farp);
    void lookAt(f32* out, const f32* eye, const f32* at, const f32* up);

    // Note: Internal use only, use apply_view() instead
    void setViewRect(ViewID view, u16 x, u16 y, u16 w, u16 h);
    void setViewFramebuffer(ViewID view, FramebufferHandle handle);

    void setViewProjection(ViewID view, const f32* view_mtx, const f32* proj_mtx);
    void setScissor(const ScissorRect& r);
    void setStencil(u32 f_stencil, u32 b_stencil);

    void applyView(ViewID view, const ViewDesc& desc);

    void identityMatrix(f32* _mtx);

    u64 mapState(PipelineState s, BlendMode, DepthTest, CullMode, bool depth_write);
    bgfx::UniformType::Enum map_uniform_type(UniformType t);
    bgfx::Attrib::Enum map_attrib(Attrib a);
    bgfx::AttribType::Enum map_attrib_type(AttribType t);

    void submit(const RenderPacket&, ViewID);

    void beginFrame();
    void endFrame();

    template<typename T>
    void destroyResource(T handle);

    void processDeletions();

    inline u64 makeSortKey(u8 layer, u8 pass, u16 pipeline, u16 texture, u16 depth = 0)
    {
        return (u64(layer) << 56) | (u64(pass) << 48) | (u64(pipeline) << 32) | (u64(texture) << 16) | u64(depth);
    }

    constexpr PipelineState operator|(PipelineState a, PipelineState b) {
        return static_cast<PipelineState>(static_cast<u64>(a) | static_cast<u64>(b));
    }

    constexpr PipelineState operator&(PipelineState a, PipelineState b) {
        return static_cast<PipelineState>(static_cast<u64>(a) & static_cast<u64>(b));
    }
}

// These are the things that we don't export but are visible to all implementation files
namespace draco::rendering::rhi
{
    using namespace draco::core::memory;

    extern HandleRegistry<Buffer, BufferTag> g_buffers;
    extern HandleRegistry<Pipeline, PipelineTag> g_pipelines;
    extern HandleRegistry<bgfx::UniformHandle, UniformTag> g_uniforms;
    extern HandleRegistry<bgfx::TextureHandle, TextureTag> g_textures;
    extern HandleRegistry<FramebufferResource, FramebufferTag> g_framebuffers;
    extern HandleRegistry<bgfx::ShaderHandle, ShaderTag> g_shaders;
    extern HandleRegistry<VertexLayoutResource, LayoutTag> g_layouts;

    // Deferred destruction queue (GPU-safe deletion)
    extern std::vector<DeletionReq> g_deletion_queue;
    
    extern u16 g_width;
    extern u16 g_height;

    // Ensures a handle is valid before use
    // TODO: Replace with something better
    template<typename Registry, typename HandleT>
    auto* getChecked(Registry& reg, HandleT h, const char* name)
    {
        if (!reg.valid(h))
        {
            RHI_WARN(false, "{} handle invalid or stale!", name);
            return (decltype(reg.get(h)))nullptr;
        }

        return reg.get(h);
    }
}

// Re-export the namespace since the things it uses aren't declared before
// This is a bit hacky but it works
// The problem is that if we move the unexported namespace above the other exported namespace, stuff isn't declared yet & it gives errors
// The same goes for the exported namespace
// In a nutshell, they both rely on each other which is why we use this hack (AR-DEV-1)
export namespace draco::rendering::rhi
{
    void queueDestruction(std::function<void()> cb);

    // Explicit overloads for each bgfx resource type
    void destroyLater(bgfx::ShaderHandle handle);
    void destroyLater(bgfx::UniformHandle handle);
    void destroyLater(bgfx::VertexBufferHandle handle);
    void destroyLater(bgfx::IndexBufferHandle handle);
    void destroyLater(bgfx::DynamicVertexBufferHandle handle);
    void destroyLater(bgfx::DynamicIndexBufferHandle handle);
    void destroyLater(bgfx::TextureHandle handle);
    void destroyLater(bgfx::FrameBufferHandle handle);
}
