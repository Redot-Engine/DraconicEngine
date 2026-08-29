#include <new>
/// Renders a spinning triangle to a small 128x128 offscreen texture, then blits
/// it to the full swapchain (scaled up with linear filtering).

#include <cmath>
#include <cstdint>
#include <cstring>
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

class BlitSample : public sf::SampleApp {
public:
    using sf::SampleApp::SampleApp;
    std::u8string_view title() const override { return u8"Sample014 - Blit (Scaled Copy)"; }
protected:
    draco::Status onInit() override;
    void onRender() override;
    void onShutdown() override;
private:
    static constexpr const char8_t kShader[] = u8R"(
        struct VSInput { float3 Position : TEXCOORD0; float4 Color : TEXCOORD1; };
        struct PSInput { float4 Position : SV_POSITION; float4 Color : COLOR0; };
        PSInput VSMain(VSInput i) { PSInput o; o.Position = float4(i.Position,1); o.Color = i.Color; return o; }
        float4 PSMain(PSInput i) : SV_TARGET { return i.Color; }
    )";
    static constexpr draco::u32 kOffscreenSize = 128;

    void updateTriangle();

    shaders::Compiler* m_compiler = nullptr;
    rhi::ShaderModule *m_vs = nullptr, *m_ps = nullptr;
    rhi::Buffer *m_vb = nullptr;
    rhi::PipelineLayout *m_pl = nullptr; rhi::RenderPipeline *m_pipeline = nullptr;
    rhi::Texture *m_offscreenTex = nullptr; rhi::TextureView *m_offscreenView = nullptr;
    rhi::CommandPool *m_pool = nullptr; rhi::Fence *m_fence = nullptr;
    draco::u64 m_fenceVal = 0;
};

draco::Status BlitSample::onInit() {
    using draco::Status, std::span, draco::u8;
    if (shaders::createCompiler(shaders::CompilerDesc{}, m_compiler) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (sf::compileToModule(m_compiler, m_device, kShader, shaders::ShaderStage::Vertex,   u8"VSMain", u8"VS", m_vs) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (sf::compileToModule(m_compiler, m_device, kShader, shaders::ShaderStage::Fragment, u8"PSMain", u8"PS", m_ps) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Triangle VB (CpuToGpu for per-frame rotation updates).
    rhi::BufferDesc vbd{}; vbd.size = 84; vbd.usage = rhi::BufferUsage::Vertex; vbd.memory = rhi::MemoryLocation::CpuToGpu;
    if (m_device->createBuffer(vbd, m_vb) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    rhi::PipelineLayoutDesc pld{};
    if (m_device->createPipelineLayout(pld, m_pl) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Offscreen render target.
    rhi::TextureDesc td{}; td.format = m_swapChain->format(); td.width = kOffscreenSize; td.height = kOffscreenSize;
    td.mipLevelCount = 1; td.usage = rhi::TextureUsage::RenderTarget | rhi::TextureUsage::CopySrc | rhi::TextureUsage::Sampled;
    if (m_device->createTexture(td, m_offscreenTex) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    rhi::TextureViewDesc tvd{}; tvd.format = m_swapChain->format(); tvd.mipLevelCount = 1; tvd.arrayLayerCount = 1;
    if (m_device->createTextureView(m_offscreenTex, tvd, m_offscreenView) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    rhi::VertexAttribute attrs[2] = { {rhi::VertexFormat::Float32x3, 0, 0}, {rhi::VertexFormat::Float32x4, 12, 1} };
    rhi::VertexBufferLayout vbl{}; vbl.stride = 28; vbl.attributes = std::span<const rhi::VertexAttribute>(attrs, 2);
    rhi::ColorTargetState ct{}; ct.format = m_swapChain->format();
    rhi::RenderPipelineDesc rpd{}; rpd.layout = m_pl;
    rpd.vertex.shader = { m_vs, u8"VSMain", rhi::ShaderStage::Vertex };
    rpd.vertex.buffers = std::span<const rhi::VertexBufferLayout>(&vbl, 1);
    rpd.fragment = rhi::FragmentState{}; rpd.fragment->shader = { m_ps, u8"PSMain", rhi::ShaderStage::Fragment };
    rpd.fragment->targets = std::span<const rhi::ColorTargetState>(&ct, 1);
    if (m_device->createRenderPipeline(rpd, m_pipeline) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    if (m_device->createCommandPool(rhi::QueueType::Graphics, m_pool) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (m_device->createFence(0, m_fence) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    return draco::ErrorCode::Ok;
}

void BlitSample::updateTriangle() {
    float angle = m_totalTime * 2.0f;
    float c = std::cos(angle), s = std::sin(angle);
    float basePos[6] = { 0.0f, 0.5f, 0.433f, -0.25f, -0.433f, -0.25f };
    float colors[12] = { 1,0.2f,0.2f,1, 0.2f,1,0.2f,1, 0.2f,0.4f,1,1 };
    float verts[21];
    for (int i = 0; i < 3; ++i) {
        float x = basePos[i*2], y = basePos[i*2+1];
        verts[i*7+0] = x*c - y*s; verts[i*7+1] = x*s + y*c; verts[i*7+2] = 0.0f;
        verts[i*7+3] = colors[i*4]; verts[i*7+4] = colors[i*4+1];
        verts[i*7+5] = colors[i*4+2]; verts[i*7+6] = colors[i*4+3];
    }
    void* mapped = m_vb->map();
    if (mapped) { std::memcpy(mapped, verts, 84); m_vb->unmap(); }
}

void BlitSample::onRender() {
    using draco::f32, std::span;
    if (m_fenceVal > 0) m_fence->wait(m_fenceVal, ~0ull);
    if (m_swapChain->acquireNextImage() != draco::ErrorCode::Ok) return;

    updateTriangle();

    m_pool->reset();
    rhi::CommandEncoder* enc = nullptr;
    if (m_pool->createEncoder(enc) != draco::ErrorCode::Ok || !enc) return;

    // Pass 1: Render spinning triangle to offscreen texture.
    enc->transitionTexture(m_offscreenTex, rhi::ResourceState::Undefined, rhi::ResourceState::RenderTarget);
    {
        rhi::ColorAttachment ca{}; ca.view = m_offscreenView;
        ca.loadOp = rhi::LoadOp::Clear; ca.storeOp = rhi::StoreOp::Store;
        ca.clearValue = rhi::ClearColor(0.15f, 0.1f, 0.2f, 1.0f);
        rhi::RenderPassDesc rpd{}; rpd.colorAttachments.push_back(ca);
        auto* rp = enc->beginRenderPass(rpd);
        rp->setPipeline(m_pipeline);
        rp->setViewport(0, 0, static_cast<f32>(kOffscreenSize), static_cast<f32>(kOffscreenSize), 0, 1);
        rp->setScissor(0, 0, kOffscreenSize, kOffscreenSize);
        rp->setVertexBuffer(0, m_vb, 0);
        rp->draw(3);
        rp->end();
    }
    enc->transitionTexture(m_offscreenTex, rhi::ResourceState::RenderTarget, rhi::ResourceState::CopySrc);

    // Pass 2: Blit offscreen (128x128) to full swapchain (scaled up with linear filtering).
    enc->transitionTexture(m_swapChain->currentTexture(), rhi::ResourceState::Undefined, rhi::ResourceState::CopyDst);
    enc->blit(m_offscreenTex, m_swapChain->currentTexture());
    enc->transitionTexture(m_swapChain->currentTexture(), rhi::ResourceState::CopyDst, rhi::ResourceState::Present);

    rhi::CommandBuffer* cb = enc->finish(); m_fenceVal++;
    rhi::CommandBuffer* cbs[1] = { cb };
    m_graphicsQueue->submit(std::span<rhi::CommandBuffer* const>(cbs, 1), m_fence, m_fenceVal);
    m_swapChain->present(m_graphicsQueue);
    m_pool->destroyEncoder(enc);
}

void BlitSample::onShutdown() {
    if (m_fence) m_device->destroyFence(m_fence); if (m_pool) m_device->destroyCommandPool(m_pool);
    if (m_pipeline) m_device->destroyRenderPipeline(m_pipeline); if (m_pl) m_device->destroyPipelineLayout(m_pl);
    if (m_offscreenView) m_device->destroyTextureView(m_offscreenView);
    if (m_offscreenTex) m_device->destroyTexture(m_offscreenTex);
    if (m_vb) m_device->destroyBuffer(m_vb);
    if (m_ps) m_device->destroyShaderModule(m_ps); if (m_vs) m_device->destroyShaderModule(m_vs);
    if (m_compiler) { m_compiler->destroy(); delete m_compiler; }
}

int main(int argc, char** argv) { BlitSample app; return app.run(argc, argv); }
