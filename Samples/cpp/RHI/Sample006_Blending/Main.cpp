#include <new>
/// Renders overlapping semi-transparent colored quads.

#include <cstdint>
#include <span>
#include <string_view>

import core;
import rhi;
import shaders;
import samples.rhi.framework;
import rhi.vk;

namespace sf = draco::samples::framework;
namespace rhi = draco::rhi;
namespace shaders = draco::shaders;

class BlendingSample : public sf::SampleApp {
public:
    using sf::SampleApp::SampleApp;
    std::u8string_view title() const override { return u8"Sample006 - Alpha Blending"; }
protected:
    draco::Status onInit() override;
    void onRender() override;
    void onShutdown() override;
private:
    static constexpr const char8_t kShader[] = u8R"(
        struct VSInput { float3 Position : TEXCOORD0; float4 Color : TEXCOORD1; };
        struct PSInput { float4 Position : SV_POSITION; float4 Color : COLOR0; };
        PSInput VSMain(VSInput i) { PSInput o; o.Position = float4(i.Position, 1.0); o.Color = i.Color; return o; }
        float4 PSMain(PSInput i) : SV_TARGET { return i.Color; }
    )";
    // 4 quads: background opaque + 3 overlapping translucent.
    static constexpr float kVerts[] = {
        -.9f,-.9f,.5f, .15f,.15f,.2f,1, .9f,-.9f,.5f, .15f,.15f,.2f,1, .9f,.9f,.5f, .15f,.15f,.2f,1, -.9f,.9f,.5f, .15f,.15f,.2f,1,
        -.6f,-.4f,.3f, 1,.2f,.2f,.5f, .1f,-.4f,.3f, 1,.2f,.2f,.5f, .1f,.4f,.3f, 1,.2f,.2f,.5f, -.6f,.4f,.3f, 1,.2f,.2f,.5f,
        -.3f,-.5f,.2f, .2f,1,.2f,.5f, .4f,-.5f,.2f, .2f,1,.2f,.5f, .4f,.3f,.2f, .2f,1,.2f,.5f, -.3f,.3f,.2f, .2f,1,.2f,.5f,
        -.1f,-.3f,.1f, .2f,.3f,1,.5f, .6f,-.3f,.1f, .2f,.3f,1,.5f, .6f,.5f,.1f, .2f,.3f,1,.5f, -.1f,.5f,.1f, .2f,.3f,1,.5f,
    };
    static constexpr draco::u16 kIdx[] = { 0,1,2,0,2,3, 4,5,6,4,6,7, 8,9,10,8,10,11, 12,13,14,12,14,15 };

    shaders::Compiler* m_compiler = nullptr;
    rhi::ShaderModule *m_vs = nullptr, *m_ps = nullptr;
    rhi::Buffer *m_vb = nullptr, *m_ib = nullptr;
    rhi::PipelineLayout *m_pl = nullptr;
    rhi::RenderPipeline *m_opaquePipe = nullptr, *m_blendPipe = nullptr;
    rhi::CommandPool *m_pool = nullptr; rhi::Fence *m_fence = nullptr;
    draco::u64 m_fenceVal = 0;
};

draco::Status BlendingSample::onInit() {
    using draco::Status, std::span, draco::u8;
    if (shaders::createCompiler(shaders::CompilerDesc{}, m_compiler) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (sf::compileToModule(m_compiler, m_device, kShader, shaders::ShaderStage::Vertex,   u8"VSMain", u8"VS", m_vs) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (sf::compileToModule(m_compiler, m_device, kShader, shaders::ShaderStage::Fragment, u8"PSMain", u8"PS", m_ps) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    rhi::BufferDesc vbd{}; vbd.size = sizeof(kVerts); vbd.usage = rhi::BufferUsage::Vertex | rhi::BufferUsage::CopyDst; vbd.memory = rhi::MemoryLocation::GpuOnly;
    if (m_device->createBuffer(vbd, m_vb) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    rhi::BufferDesc ibd{}; ibd.size = sizeof(kIdx); ibd.usage = rhi::BufferUsage::Index | rhi::BufferUsage::CopyDst; ibd.memory = rhi::MemoryLocation::GpuOnly;
    if (m_device->createBuffer(ibd, m_ib) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    rhi::TransferBatch* batch = nullptr; m_graphicsQueue->createTransferBatch(batch);
    batch->writeBuffer(m_vb, 0, std::span<const u8>(reinterpret_cast<const u8*>(kVerts), sizeof(kVerts)));
    batch->writeBuffer(m_ib, 0, std::span<const u8>(reinterpret_cast<const u8*>(kIdx), sizeof(kIdx)));
    batch->submit(); m_graphicsQueue->destroyTransferBatch(batch);

    // Empty pipeline layout.
    rhi::PipelineLayoutDesc pld{};
    if (m_device->createPipelineLayout(pld, m_pl) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    rhi::VertexAttribute attrs[2] = { {rhi::VertexFormat::Float32x3, 0, 0}, {rhi::VertexFormat::Float32x4, 12, 1} };
    rhi::VertexBufferLayout vbl{}; vbl.stride = 28; vbl.attributes = std::span<const rhi::VertexAttribute>(attrs, 2);
    rhi::ColorTargetState ctOpaque{}; ctOpaque.format = m_swapChain->format();
    rhi::ColorTargetState ctBlend{}; ctBlend.format = m_swapChain->format();
    ctBlend.blend = rhi::BlendState::alphaBlend();

    // Opaque pipeline (for background quad).
    rhi::RenderPipelineDesc rpd{}; rpd.layout = m_pl;
    rpd.vertex.shader = { m_vs, u8"VSMain", rhi::ShaderStage::Vertex };
    rpd.vertex.buffers = std::span<const rhi::VertexBufferLayout>(&vbl, 1);
    rpd.fragment = rhi::FragmentState{}; rpd.fragment->shader = { m_ps, u8"PSMain", rhi::ShaderStage::Fragment };
    rpd.fragment->targets = std::span<const rhi::ColorTargetState>(&ctOpaque, 1);
    if (m_device->createRenderPipeline(rpd, m_opaquePipe) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Blend pipeline (for translucent quads).
    rpd.fragment->targets = std::span<const rhi::ColorTargetState>(&ctBlend, 1);
    if (m_device->createRenderPipeline(rpd, m_blendPipe) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    if (m_device->createCommandPool(rhi::QueueType::Graphics, m_pool) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (m_device->createFence(0, m_fence) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    return draco::ErrorCode::Ok;
}

void BlendingSample::onRender() {
    using draco::f32, std::span;
    if (m_fenceVal > 0) m_fence->wait(m_fenceVal, ~0ull);
    if (m_swapChain->acquireNextImage() != draco::ErrorCode::Ok) return;
    m_pool->reset();
    rhi::CommandEncoder* enc = nullptr;
    if (m_pool->createEncoder(enc) != draco::ErrorCode::Ok || !enc) return;
    enc->transitionTexture(m_swapChain->currentTexture(), rhi::ResourceState::Undefined, rhi::ResourceState::RenderTarget);

    rhi::ColorAttachment ca{}; ca.view = m_swapChain->currentTextureView();
    ca.loadOp = rhi::LoadOp::Clear; ca.storeOp = rhi::StoreOp::Store; ca.clearValue = rhi::ClearColor(0.05f, 0.05f, 0.08f, 1);
    rhi::RenderPassDesc rpd{}; rpd.colorAttachments.push_back(ca);
    auto* rp = enc->beginRenderPass(rpd);
    rp->setViewport(0, 0, static_cast<f32>(m_width), static_cast<f32>(m_height), 0, 1);
    rp->setScissor(0, 0, m_width, m_height);
    rp->setVertexBuffer(0, m_vb, 0);
    rp->setIndexBuffer(m_ib, rhi::IndexFormat::UInt16, 0);
    // Draw background opaque.
    rp->setPipeline(m_opaquePipe); rp->drawIndexed(6, 1, 0, 0, 0);
    // Draw 3 translucent quads.
    rp->setPipeline(m_blendPipe); rp->drawIndexed(18, 1, 6, 0, 0);
    rp->end();
    enc->transitionTexture(m_swapChain->currentTexture(), rhi::ResourceState::RenderTarget, rhi::ResourceState::Present);

    rhi::CommandBuffer* cb = enc->finish(); m_fenceVal++;
    rhi::CommandBuffer* cbs[1] = { cb };
    m_graphicsQueue->submit(std::span<rhi::CommandBuffer* const>(cbs, 1), m_fence, m_fenceVal);
    m_swapChain->present(m_graphicsQueue); m_pool->destroyEncoder(enc);
}

void BlendingSample::onShutdown() {
    if (m_fence) m_device->destroyFence(m_fence); if (m_pool) m_device->destroyCommandPool(m_pool);
    if (m_blendPipe) m_device->destroyRenderPipeline(m_blendPipe);
    if (m_opaquePipe) m_device->destroyRenderPipeline(m_opaquePipe);
    if (m_pl) m_device->destroyPipelineLayout(m_pl);
    if (m_ib) m_device->destroyBuffer(m_ib); if (m_vb) m_device->destroyBuffer(m_vb);
    if (m_ps) m_device->destroyShaderModule(m_ps); if (m_vs) m_device->destroyShaderModule(m_vs);
    if (m_compiler) { m_compiler->destroy(); delete m_compiler; }
}

int main(int argc, char** argv) { BlendingSample app; return app.run(argc, argv); }
