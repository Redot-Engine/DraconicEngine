#include <new>
/// Three overlapping quads at different Z depths demonstrate depth testing.
/// Red (z=0.8, far), Green (z=0.5, mid), Blue (z=0.2, near) - drawn far-to-near,
/// depth buffer ensures correct visibility.

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

class DepthBufferSample : public sf::SampleApp {
public:
    using sf::SampleApp::SampleApp;
    std::u8string_view title() const override { return u8"Sample008 - Depth Buffer"; }
protected:
    draco::Status onInit() override;
    void onRender() override;
    void onResize(draco::u32 w, draco::u32 h) override { recreateDepth(w, h); }
    void onShutdown() override;
private:
    static constexpr const char8_t kShader[] = u8R"(
        struct VSInput
        {
            float3 Position : TEXCOORD0;
            float4 Color    : TEXCOORD1;
        };
        struct PSInput
        {
            float4 Position : SV_POSITION;
            float4 Color    : COLOR0;
        };
        PSInput VSMain(VSInput input)
        {
            PSInput output;
            output.Position = float4(input.Position, 1.0);
            output.Color = input.Color;
            return output;
        }
        float4 PSMain(PSInput input) : SV_TARGET
        {
            return input.Color;
        }
    )";

    // Three overlapping quads drawn in order: red (far), green (middle), blue (near).
    // Stride: 7 floats per vertex (pos xyz + color rgba).
    static constexpr float kVerts[] = {
        // Quad 0: Red - large, behind (z=0.8), drawn first.
        -0.6f, -0.6f, 0.8f,   1.0f, 0.2f, 0.2f, 1.0f,
         0.4f, -0.6f, 0.8f,   1.0f, 0.2f, 0.2f, 1.0f,
         0.4f,  0.6f, 0.8f,   1.0f, 0.2f, 0.2f, 1.0f,
        -0.6f,  0.6f, 0.8f,   1.0f, 0.2f, 0.2f, 1.0f,
        // Quad 1: Green - overlaps red, closer (z=0.5), drawn second.
        -0.2f, -0.4f, 0.5f,   0.2f, 1.0f, 0.2f, 1.0f,
         0.6f, -0.4f, 0.5f,   0.2f, 1.0f, 0.2f, 1.0f,
         0.6f,  0.4f, 0.5f,   0.2f, 1.0f, 0.2f, 1.0f,
        -0.2f,  0.4f, 0.5f,   0.2f, 1.0f, 0.2f, 1.0f,
        // Quad 2: Blue - overlaps both, nearest (z=0.2), drawn third.
        -0.4f, -0.7f, 0.2f,   0.2f, 0.3f, 1.0f, 1.0f,
         0.2f, -0.7f, 0.2f,   0.2f, 0.3f, 1.0f, 1.0f,
         0.2f,  0.7f, 0.2f,   0.2f, 0.3f, 1.0f, 1.0f,
        -0.4f,  0.7f, 0.2f,   0.2f, 0.3f, 1.0f, 1.0f,
    };
    static constexpr draco::u16 kIdx[] = {
        0, 1, 2, 0, 2, 3,
        4, 5, 6, 4, 6, 7,
        8, 9, 10, 8, 10, 11,
    };

    void recreateDepth(draco::u32 w, draco::u32 h);

    shaders::Compiler* m_compiler = nullptr;
    rhi::ShaderModule *m_vs = nullptr, *m_ps = nullptr;
    rhi::Buffer *m_vb = nullptr, *m_ib = nullptr;
    rhi::PipelineLayout *m_pl = nullptr;
    rhi::RenderPipeline *m_pipeline = nullptr;
    rhi::Texture *m_depthTex = nullptr;
    rhi::TextureView *m_depthView = nullptr;
    rhi::CommandPool *m_pool = nullptr; rhi::Fence *m_fence = nullptr;
    draco::u64 m_fenceVal = 0;
};

void DepthBufferSample::recreateDepth(draco::u32 w, draco::u32 h) {
    if (m_depthView) { m_device->destroyTextureView(m_depthView); m_depthView = nullptr; }
    if (m_depthTex) { m_device->destroyTexture(m_depthTex); m_depthTex = nullptr; }

    rhi::TextureDesc td = rhi::TextureDesc::depthBuffer(rhi::TextureFormat::Depth24PlusStencil8, w, h, 1, u8"DepthTex");
    m_device->createTexture(td, m_depthTex);
    rhi::TextureViewDesc tvd{}; tvd.format = rhi::TextureFormat::Depth24PlusStencil8; tvd.dimension = rhi::TextureViewDimension::Texture2D;
    tvd.mipLevelCount = 1; tvd.arrayLayerCount = 1;
    m_device->createTextureView(m_depthTex, tvd, m_depthView);
}

draco::Status DepthBufferSample::onInit() {
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

    // Pipeline layout (empty - no bind groups needed).
    rhi::PipelineLayoutDesc pld{};
    if (m_device->createPipelineLayout(pld, m_pl) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    recreateDepth(m_width, m_height);

    rhi::VertexAttribute attrs[2] = { {rhi::VertexFormat::Float32x3, 0, 0}, {rhi::VertexFormat::Float32x4, 12, 1} };
    rhi::VertexBufferLayout vbl{}; vbl.stride = 28; vbl.attributes = std::span<const rhi::VertexAttribute>(attrs, 2);

    rhi::ColorTargetState ct{}; ct.format = m_swapChain->format();
    rhi::RenderPipelineDesc rpd{}; rpd.layout = m_pl;
    rpd.vertex.shader = { m_vs, u8"VSMain", rhi::ShaderStage::Vertex };
    rpd.vertex.buffers = std::span<const rhi::VertexBufferLayout>(&vbl, 1);
    rpd.fragment = rhi::FragmentState{}; rpd.fragment->shader = { m_ps, u8"PSMain", rhi::ShaderStage::Fragment };
    rpd.fragment->targets = std::span<const rhi::ColorTargetState>(&ct, 1);
    rpd.depthStencil = rhi::DepthStencilState{}; rpd.depthStencil->format = rhi::TextureFormat::Depth24PlusStencil8;
    rpd.depthStencil->depthWriteEnabled = true;
    rpd.depthStencil->depthCompare = rhi::CompareFunction::Less;
    if (m_device->createRenderPipeline(rpd, m_pipeline) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    if (m_device->createCommandPool(rhi::QueueType::Graphics, m_pool) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (m_device->createFence(0, m_fence) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    return draco::ErrorCode::Ok;
}

void DepthBufferSample::onRender() {
    using draco::f32, std::span;
    if (m_fenceVal > 0) m_fence->wait(m_fenceVal, ~0ull);
    if (m_swapChain->acquireNextImage() != draco::ErrorCode::Ok) return;

    m_pool->reset();
    rhi::CommandEncoder* enc = nullptr;
    if (m_pool->createEncoder(enc) != draco::ErrorCode::Ok || !enc) return;
    enc->transitionTexture(m_swapChain->currentTexture(), rhi::ResourceState::Undefined, rhi::ResourceState::RenderTarget);
    enc->transitionTexture(m_depthTex, rhi::ResourceState::Undefined, rhi::ResourceState::DepthStencilWrite);

    rhi::ColorAttachment ca{}; ca.view = m_swapChain->currentTextureView();
    ca.loadOp = rhi::LoadOp::Clear; ca.storeOp = rhi::StoreOp::Store;
    ca.clearValue = rhi::ClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    rhi::DepthStencilAttachment dsa{}; dsa.view = m_depthView;
    dsa.depthLoadOp = rhi::LoadOp::Clear; dsa.depthStoreOp = rhi::StoreOp::Store; dsa.depthClearValue = 1.0f;
    rhi::RenderPassDesc rpd{}; rpd.colorAttachments.push_back(ca); rpd.depthStencilAttachment = dsa;
    auto* rp = enc->beginRenderPass(rpd);

    rp->setPipeline(m_pipeline);
    rp->setViewport(0, 0, static_cast<f32>(m_width), static_cast<f32>(m_height), 0, 1);
    rp->setScissor(0, 0, m_width, m_height);
    rp->setVertexBuffer(0, m_vb, 0);
    rp->setIndexBuffer(m_ib, rhi::IndexFormat::UInt16, 0);
    // Draw all 3 quads - depth buffer determines visibility.
    rp->drawIndexed(18);
    rp->end();

    enc->transitionTexture(m_swapChain->currentTexture(), rhi::ResourceState::RenderTarget, rhi::ResourceState::Present);
    rhi::CommandBuffer* cb = enc->finish(); m_fenceVal++;
    rhi::CommandBuffer* cbs[1] = { cb };
    m_graphicsQueue->submit(std::span<rhi::CommandBuffer* const>(cbs, 1), m_fence, m_fenceVal);
    m_swapChain->present(m_graphicsQueue);
    m_pool->destroyEncoder(enc);
}

void DepthBufferSample::onShutdown() {
    if (m_fence) m_device->destroyFence(m_fence); if (m_pool) m_device->destroyCommandPool(m_pool);
    if (m_pipeline) m_device->destroyRenderPipeline(m_pipeline); if (m_pl) m_device->destroyPipelineLayout(m_pl);
    if (m_depthView) m_device->destroyTextureView(m_depthView);
    if (m_depthTex) m_device->destroyTexture(m_depthTex);
    if (m_ib) m_device->destroyBuffer(m_ib); if (m_vb) m_device->destroyBuffer(m_vb);
    if (m_ps) m_device->destroyShaderModule(m_ps); if (m_vs) m_device->destroyShaderModule(m_vs);
    if (m_compiler) { m_compiler->destroy(); delete m_compiler; }
}

int main(int argc, char** argv) { DepthBufferSample app; return app.run(argc, argv); }
