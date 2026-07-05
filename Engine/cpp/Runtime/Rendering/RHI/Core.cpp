module;

#include <vector>
#include <array>
#include <string>
#include <cstring>
#include <algorithm> 
#include <functional>
#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include "macros.h"

module rendering.rhi;

import core.stdtypes;
import core.math.constants;

namespace draco::rendering::rhi
{
    using namespace draco::core::memory;

    HandleRegistry<Buffer, BufferTag> g_buffers;
    HandleRegistry<Pipeline, PipelineTag> g_pipelines;
    HandleRegistry<bgfx::UniformHandle, UniformTag> g_uniforms;
    HandleRegistry<bgfx::TextureHandle, TextureTag> g_textures;
    HandleRegistry<FramebufferResource, FramebufferTag> g_framebuffers;
    HandleRegistry<bgfx::ShaderHandle, ShaderTag> g_shaders;
    HandleRegistry<VertexLayoutResource, LayoutTag> g_layouts;

    std::vector<DeletionReq> g_deletion_queue;
    u16 g_width = 0;
    u16 g_height = 0;

    void queueDestruction(std::function<void()> cb)
    {
        g_deletion_queue.push_back({
            bgfx::getStats()->gpuFrameNum,
            std::move(cb)
        });
    }

    // Explicit overloads for each bgfx resource
    void destroyLater(bgfx::ShaderHandle handle)
    { 
        queueDestruction([handle]() {
            bgfx::destroy(handle); 
        }); 
    }

    void destroyLater(bgfx::UniformHandle handle)
    { 
        queueDestruction([handle]() {
            bgfx::destroy(handle); 
        }); 
    }

    void destroyLater(bgfx::VertexBufferHandle handle)
    { 
        queueDestruction([handle]() {
            bgfx::destroy(handle); 
        }); 
    }

    void destroyLater(bgfx::IndexBufferHandle handle)
    { 
        queueDestruction([handle]() {
            bgfx::destroy(handle); 
        }); 
    }

    void destroyLater(bgfx::DynamicVertexBufferHandle handle)
    { 
        queueDestruction([handle]() {
            bgfx::destroy(handle); 
        }); 
    }

    void destroyLater(bgfx::DynamicIndexBufferHandle handle)
    { 
        queueDestruction([handle]() {
            bgfx::destroy(handle);
        }); 
    }

    void destroyLater(bgfx::TextureHandle handle)
    { 
        queueDestruction([handle]() {
            bgfx::destroy(handle); 
        }); 
    }

    void destroyLater(bgfx::FrameBufferHandle handle)
    { 
        queueDestruction([handle]() {
            bgfx::destroy(handle); 
        }); 
    }
    
    void processDeletions()
    {
        u64 frame = bgfx::getStats()->gpuFrameNum;

        std::erase_if(g_deletion_queue, [frame](const auto& d)
        {
            if (frame >= d.frame + 2)
            {
                d.cleanup();
                return true;
            }
            return false;
        });
    }

    bool init(void* display_type, void* window_handle, NativeWindowType window_type, u16 width, u16 height)
    {
        g_width = width;
        g_height = height;

        bgfx::Init init{};
        init.type = bgfx::RendererType::Count;

        init.platformData.ndt = display_type;
        init.platformData.nwh = window_handle;

        // Map our internal window type to bgfx's native window handle type
        if (window_type == NativeWindowType::Wayland)
        {
            init.platformData.type = bgfx::NativeWindowHandleType::Wayland;
        }
        else
        {
            // Others can work fine with the default type
            init.platformData.type = bgfx::NativeWindowHandleType::Default;
        }

        init.resolution.width  = width;
        init.resolution.height = height;
        init.resolution.reset  = BGFX_RESET_VSYNC;

        if (!bgfx::init(init))
        {
            RHI_WARN(false, "bgfx initialization failed");
            return false;
        }

        bgfx::setDebug(BGFX_DEBUG_TEXT);
        return true;
    }

    void resize(u16 width, u16 height)
    {
        if(width == 0 || height == 0)
            return; // Minimized window safety

        if(width == g_width && height == g_height)
            return; // No need to resize
        
        g_width = width;
        g_height = height;

        bgfx::reset(width, height, BGFX_RESET_VSYNC);
    }

    void shutdown()
    {
        // Walk all registries and destroy live GPU objects
        for (auto& slot : g_buffers.internal().raw())
        {
            if (!slot.alive) continue;

            if (bgfx::isValid(slot.value.vbh))
                bgfx::destroy(slot.value.vbh);

            if (bgfx::isValid(slot.value.ibh))
                bgfx::destroy(slot.value.ibh);
            
            if (bgfx::isValid(slot.value.dvbh))
                bgfx::destroy(slot.value.dvbh);
        }

        for (auto& slot : g_pipelines.internal().raw())
        {
            if (!slot.alive) continue;

            if (bgfx::isValid(slot.value.program))
                bgfx::destroy(slot.value.program);
        }

        for (auto& slot : g_uniforms.internal().raw())
        {
            if (!slot.alive) continue;

            if (bgfx::isValid(slot.value))
                bgfx::destroy(slot.value);
        }

        for (auto& slot : g_textures.internal().raw())
        {
            if (!slot.alive) continue;

            if (bgfx::isValid(slot.value))
                bgfx::destroy(slot.value);
        }

        bgfx::shutdown();
    }

    void destroyBuffer(BufferHandle h)
    {
        auto* buf = getChecked(g_buffers, h, "Buffer");

        if (!buf)
            return;

        if (bgfx::isValid(buf->vbh))
            destroyLater(buf->vbh);

        if (bgfx::isValid(buf->ibh))
            destroyLater(buf->ibh);

        if (bgfx::isValid(buf->dvbh))
            destroyLater(buf->dvbh);
        
        if (bgfx::isValid(buf->dibh))
            destroyLater(buf->dibh);

        g_buffers.destroy(h);
    }

    u64 mapState(PipelineState s, BlendMode blend, DepthTest depth, CullMode cull, bool depth_write)
    {
        u64 state = 0;

        if ((s & PipelineState::WriteRGB) != PipelineState::Default)
            state |= BGFX_STATE_WRITE_RGB;

        if ((s & PipelineState::WriteAlpha) != PipelineState::Default)
            state |= BGFX_STATE_WRITE_A;

        if (depth_write)
            state |= BGFX_STATE_WRITE_Z;

        switch (depth)
        {
            case DepthTest::Less:   state |= BGFX_STATE_DEPTH_TEST_LESS; break;
            case DepthTest::Equal:  state |= BGFX_STATE_DEPTH_TEST_EQUAL; break;
            case DepthTest::Always: state |= BGFX_STATE_DEPTH_TEST_ALWAYS; break;
            case DepthTest::None:   break;
        }

    
        switch (cull)
        {
            case CullMode::CW:  state |= BGFX_STATE_CULL_CW; break;
            case CullMode::CCW: state |= BGFX_STATE_CULL_CCW; break;
            case CullMode::None: break;
        }

        switch (blend)
        {
            case BlendMode::Alpha:
                state |= BGFX_STATE_BLEND_ALPHA;
                break;

            case BlendMode::Additive:
                state |= BGFX_STATE_BLEND_ADD;
                break;

            case BlendMode::Multiply:
                state |= BGFX_STATE_BLEND_MULTIPLY;
                break;

            case BlendMode::None:
                break;
        }

        if ((s & PipelineState::MSAA) != PipelineState::Default)
            state |= BGFX_STATE_MSAA;

        if ((s & PipelineState::PrimitiveTriStrip) != PipelineState::Default)
            state |= BGFX_STATE_PT_TRISTRIP;

        return state;
    }

    bgfx::UniformType::Enum map_uniform_type(UniformType t)
    {
        switch (t)
        {
            case UniformType::Sampler: return bgfx::UniformType::Sampler;
            case UniformType::Vec4:    return bgfx::UniformType::Vec4;
            case UniformType::Mat3:    return bgfx::UniformType::Mat3;
            case UniformType::Mat4:    return bgfx::UniformType::Mat4;
        }
        return bgfx::UniformType::Vec4;
    }

    bgfx::Attrib::Enum map_attrib(Attrib a)
    {
        switch (a)
        {
            case Attrib::Position:  return bgfx::Attrib::Position;
            case Attrib::Color0:    return bgfx::Attrib::Color0;
            case Attrib::TexCoord0: return bgfx::Attrib::TexCoord0;
            case Attrib::Normal:    return bgfx::Attrib::Normal;
            case Attrib::Tangent:   return bgfx::Attrib::Tangent;
        }

        return bgfx::Attrib::Position;
    }

    bgfx::AttribType::Enum map_attrib_type(AttribType t)
    {
        switch (t)
        {
            case AttribType::Float: return bgfx::AttribType::Float;
            case AttribType::Uint8: return bgfx::AttribType::Uint8;
        }

        return bgfx::AttribType::Float;
    }

    void beginFrame()
    {
        // Clean up GPU resources safely
        processDeletions();
    }

    void endFrame()
    {
        // Submit frame to GPU
        bgfx::frame();
    }
}
