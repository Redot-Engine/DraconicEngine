#include <new>
/// Demonstrates stencil buffer operations for object outlining.
/// Pass 1: Draw solid hexagon, write stencil = 1.
/// Pass 2: Draw scaled-up hexagon, only where stencil != 1 (outline effect).

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

class StencilOutlineSample : public sf::SampleApp {
public:
    using sf::SampleApp::SampleApp;
    std::u8string_view title() const override { return u8"Sample022 - Stencil Outline"; }

protected:
    draco::Status onInit() override;
    void onRender() override;
    void onResize(draco::u32 w, draco::u32 h) override { recreateDepthStencil(w, h); }
    void onShutdown() override;

private:
    static constexpr const char8_t kShader[] = u8R"(
        struct PushConstants
        {
            float Scale;
            float AspectRatio;
            float Time;
            float _pad;
        };

        [[vk::push_constant]] ConstantBuffer<PushConstants> pc : register(b0, space0);

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
            float2 pos = input.Position.xy * pc.Scale;
            pos.x /= pc.AspectRatio;
            // Gentle rotation
            float c = cos(pc.Time * 0.5);
            float s = sin(pc.Time * 0.5);
            float2 rotated = float2(pos.x * c - pos.y * s, pos.x * s + pos.y * c);
            output.Position = float4(rotated, input.Position.z, 1.0);
            output.Color = input.Color;
            return output;
        }

        float4 PSMain(PSInput input) : SV_TARGET
        {
            return input.Color;
        }
    )";

    struct PushData {
        float scale;
        float aspectRatio;
        float time;
        float _pad;
    };

    // Hexagon: center + 6 outer vertices.
    // Stride: 7 floats per vertex (pos xyz + color rgba).
    static constexpr float kVerts[] = {
        // Center
        0.0f, 0.0f, 0.5f,   0.9f, 0.9f, 0.9f, 1.0f,
        // Outer vertices (radius 0.6)
         0.6f,   0.0f,   0.5f,   0.3f, 0.6f, 1.0f, 1.0f,
         0.3f,   0.52f,  0.5f,   0.3f, 1.0f, 0.6f, 1.0f,
        -0.3f,   0.52f,  0.5f,   1.0f, 1.0f, 0.3f, 1.0f,
        -0.6f,   0.0f,   0.5f,   1.0f, 0.6f, 0.3f, 1.0f,
        -0.3f,  -0.52f,  0.5f,   1.0f, 0.3f, 0.6f, 1.0f,
         0.3f,  -0.52f,  0.5f,   0.6f, 0.3f, 1.0f, 1.0f,
    };
    static constexpr draco::u16 kIdx[] = {
        0, 1, 2,
        0, 2, 3,
        0, 3, 4,
        0, 4, 5,
        0, 5, 6,
        0, 6, 1,
    };

    void recreateDepthStencil(draco::u32 w, draco::u32 h);

    shaders::Compiler* m_compiler = nullptr;
    rhi::Buffer *m_vb = nullptr, *m_ib = nullptr;
    rhi::ShaderModule *m_vs = nullptr, *m_ps = nullptr;
    rhi::PipelineLayout* m_pl = nullptr;
    rhi::RenderPipeline* m_stencilWritePipeline = nullptr;
    rhi::RenderPipeline* m_stencilTestPipeline = nullptr;
    rhi::Texture* m_depthStencilTex = nullptr;
    rhi::TextureView* m_depthStencilView = nullptr;
    rhi::CommandPool* m_pool = nullptr;
    rhi::Fence* m_fence = nullptr;
    draco::u64 m_fenceVal = 0;
};

void StencilOutlineSample::recreateDepthStencil(draco::u32 w, draco::u32 h) {
    if (m_depthStencilView) { m_device->destroyTextureView(m_depthStencilView); m_depthStencilView = nullptr; }
    if (m_depthStencilTex) { m_device->destroyTexture(m_depthStencilTex); m_depthStencilTex = nullptr; }

    rhi::TextureDesc td = rhi::TextureDesc::depthBuffer(rhi::TextureFormat::Depth24PlusStencil8, w, h, 1, u8"StencilDSTex");
    m_device->createTexture(td, m_depthStencilTex);
    rhi::TextureViewDesc tvd{}; tvd.format = rhi::TextureFormat::Depth24PlusStencil8; tvd.dimension = rhi::TextureViewDimension::Texture2D;
    tvd.mipLevelCount = 1; tvd.arrayLayerCount = 1;
    m_device->createTextureView(m_depthStencilTex, tvd, m_depthStencilView);
}

draco::Status StencilOutlineSample::onInit() {
    using draco::Status, std::span, draco::u8;

    if (shaders::createCompiler(shaders::CompilerDesc{}, m_compiler) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (sf::compileToModule(m_compiler, m_device, kShader, shaders::ShaderStage::Vertex,   u8"VSMain", u8"StencilVS", m_vs) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (sf::compileToModule(m_compiler, m_device, kShader, shaders::ShaderStage::Fragment, u8"PSMain", u8"StencilPS", m_ps) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Vertex & index buffers.
    rhi::BufferDesc vbd{}; vbd.size = sizeof(kVerts); vbd.usage = rhi::BufferUsage::Vertex | rhi::BufferUsage::CopyDst; vbd.memory = rhi::MemoryLocation::GpuOnly;
    if (m_device->createBuffer(vbd, m_vb) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    rhi::BufferDesc ibd{}; ibd.size = sizeof(kIdx); ibd.usage = rhi::BufferUsage::Index | rhi::BufferUsage::CopyDst; ibd.memory = rhi::MemoryLocation::GpuOnly;
    if (m_device->createBuffer(ibd, m_ib) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    rhi::TransferBatch* batch = nullptr; m_graphicsQueue->createTransferBatch(batch);
    batch->writeBuffer(m_vb, 0, std::span<const u8>(reinterpret_cast<const u8*>(kVerts), sizeof(kVerts)));
    batch->writeBuffer(m_ib, 0, std::span<const u8>(reinterpret_cast<const u8*>(kIdx), sizeof(kIdx)));
    batch->submit(); m_graphicsQueue->destroyTransferBatch(batch);

    // Pipeline layout with push constants (no bind groups).
    rhi::PushConstantRange pcRange{ rhi::ShaderStage::Vertex, 0, sizeof(PushData) };
    rhi::PipelineLayoutDesc pld{};
    pld.pushConstantRanges = std::span<const rhi::PushConstantRange>(&pcRange, 1);
    if (m_device->createPipelineLayout(pld, m_pl) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    recreateDepthStencil(m_width, m_height);

    // Shared vertex layout.
    rhi::VertexAttribute attrs[2] = { {rhi::VertexFormat::Float32x3, 0, 0}, {rhi::VertexFormat::Float32x4, 12, 1} };
    rhi::VertexBufferLayout vbl{}; vbl.stride = 28; vbl.attributes = std::span<const rhi::VertexAttribute>(attrs, 2);
    rhi::ColorTargetState ct{}; ct.format = m_swapChain->format();

    // Pipeline 1: Stencil write - draw solid, always pass depth, write stencil = ref (1).
    {
        rhi::RenderPipelineDesc rpd{};
        rpd.layout = m_pl;
        rpd.vertex.shader = { m_vs, u8"VSMain", rhi::ShaderStage::Vertex };
        rpd.vertex.buffers = std::span<const rhi::VertexBufferLayout>(&vbl, 1);
        rpd.fragment = rhi::FragmentState{}; rpd.fragment->shader = { m_ps, u8"PSMain", rhi::ShaderStage::Fragment };
        rpd.fragment->targets = std::span<const rhi::ColorTargetState>(&ct, 1);
        rpd.depthStencil = rhi::DepthStencilState{};
        rpd.depthStencil->format = rhi::TextureFormat::Depth24PlusStencil8;
        rpd.depthStencil->depthWriteEnabled = true;
        rpd.depthStencil->depthCompare = rhi::CompareFunction::Always;
        rpd.depthStencil->stencilEnabled = true;
        rpd.depthStencil->stencilReadMask = 0xFF;
        rpd.depthStencil->stencilWriteMask = 0xFF;
        rpd.depthStencil->stencilFront = { rhi::CompareFunction::Always, rhi::StencilOperation::Keep, rhi::StencilOperation::Keep, rhi::StencilOperation::Replace };
        rpd.depthStencil->stencilBack  = { rhi::CompareFunction::Always, rhi::StencilOperation::Keep, rhi::StencilOperation::Keep, rhi::StencilOperation::Replace };
        if (m_device->createRenderPipeline(rpd, m_stencilWritePipeline) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    }

    // Pipeline 2: Stencil test - draw outline, only where stencil != 1.
    {
        rhi::RenderPipelineDesc rpd{};
        rpd.layout = m_pl;
        rpd.vertex.shader = { m_vs, u8"VSMain", rhi::ShaderStage::Vertex };
        rpd.vertex.buffers = std::span<const rhi::VertexBufferLayout>(&vbl, 1);
        rpd.fragment = rhi::FragmentState{}; rpd.fragment->shader = { m_ps, u8"PSMain", rhi::ShaderStage::Fragment };
        rpd.fragment->targets = std::span<const rhi::ColorTargetState>(&ct, 1);
        rpd.depthStencil = rhi::DepthStencilState{};
        rpd.depthStencil->format = rhi::TextureFormat::Depth24PlusStencil8;
        rpd.depthStencil->depthWriteEnabled = false;
        rpd.depthStencil->depthCompare = rhi::CompareFunction::Always;
        rpd.depthStencil->stencilEnabled = true;
        rpd.depthStencil->stencilReadMask = 0xFF;
        rpd.depthStencil->stencilWriteMask = 0x00;
        rpd.depthStencil->stencilFront = { rhi::CompareFunction::NotEqual, rhi::StencilOperation::Keep, rhi::StencilOperation::Keep, rhi::StencilOperation::Keep };
        rpd.depthStencil->stencilBack  = { rhi::CompareFunction::NotEqual, rhi::StencilOperation::Keep, rhi::StencilOperation::Keep, rhi::StencilOperation::Keep };
        if (m_device->createRenderPipeline(rpd, m_stencilTestPipeline) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    }

    if (m_device->createCommandPool(rhi::QueueType::Graphics, m_pool) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (m_device->createFence(0, m_fence) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    return draco::ErrorCode::Ok;
}

void StencilOutlineSample::onRender() {
    using draco::f32, std::span;
    if (m_fenceVal > 0) m_fence->wait(m_fenceVal, ~0ull);
    if (m_swapChain->acquireNextImage() != draco::ErrorCode::Ok) return;

    m_pool->reset();
    rhi::CommandEncoder* enc = nullptr;
    if (m_pool->createEncoder(enc) != draco::ErrorCode::Ok || !enc) return;

    enc->transitionTexture(m_swapChain->currentTexture(), rhi::ResourceState::Undefined, rhi::ResourceState::RenderTarget);
    enc->transitionTexture(m_depthStencilTex, rhi::ResourceState::Undefined, rhi::ResourceState::DepthStencilWrite);

    rhi::ColorAttachment ca{}; ca.view = m_swapChain->currentTextureView();
    ca.loadOp = rhi::LoadOp::Clear; ca.storeOp = rhi::StoreOp::Store;
    ca.clearValue = rhi::ClearColor(0.08f, 0.08f, 0.12f, 1.0f);

    rhi::DepthStencilAttachment dsa{}; dsa.view = m_depthStencilView;
    dsa.depthLoadOp = rhi::LoadOp::Clear; dsa.depthStoreOp = rhi::StoreOp::Store; dsa.depthClearValue = 1.0f;
    dsa.stencilLoadOp = rhi::LoadOp::Clear; dsa.stencilStoreOp = rhi::StoreOp::Store; dsa.stencilClearValue = 0;

    rhi::RenderPassDesc rpd{}; rpd.colorAttachments.push_back(ca); rpd.depthStencilAttachment = dsa;
    auto* rp = enc->beginRenderPass(rpd);

    f32 aspect = static_cast<f32>(m_width) / static_cast<f32>(m_height);

    // Pass 1: Draw solid hexagon, write stencil = 1.
    rp->setPipeline(m_stencilWritePipeline);
    rp->setViewport(0, 0, static_cast<f32>(m_width), static_cast<f32>(m_height), 0.0f, 1.0f);
    rp->setScissor(0, 0, m_width, m_height);
    rp->setVertexBuffer(0, m_vb, 0);
    rp->setIndexBuffer(m_ib, rhi::IndexFormat::UInt16, 0);
    rp->setStencilReference(1);
    PushData pc1{ 1.0f, aspect, m_totalTime, 0.0f };
    rp->setPushConstants(rhi::ShaderStage::Vertex, 0, sizeof(PushData), &pc1);
    rp->drawIndexed(18);

    // Pass 2: Draw scaled-up hexagon, only where stencil != 1 (outline ring).
    rp->setPipeline(m_stencilTestPipeline);
    rp->setStencilReference(1);
    PushData pc2{ 1.15f, aspect, m_totalTime, 0.0f };
    rp->setPushConstants(rhi::ShaderStage::Vertex, 0, sizeof(PushData), &pc2);
    rp->drawIndexed(18);

    rp->end();

    enc->transitionTexture(m_swapChain->currentTexture(), rhi::ResourceState::RenderTarget, rhi::ResourceState::Present);

    rhi::CommandBuffer* cb = enc->finish(); m_fenceVal++;
    rhi::CommandBuffer* cbs[1] = { cb };
    m_graphicsQueue->submit(std::span<rhi::CommandBuffer* const>(cbs, 1), m_fence, m_fenceVal);
    m_swapChain->present(m_graphicsQueue);
    m_pool->destroyEncoder(enc);
}

void StencilOutlineSample::onShutdown() {
    if (m_fence) m_device->destroyFence(m_fence);
    if (m_pool) m_device->destroyCommandPool(m_pool);
    if (m_stencilTestPipeline) m_device->destroyRenderPipeline(m_stencilTestPipeline);
    if (m_stencilWritePipeline) m_device->destroyRenderPipeline(m_stencilWritePipeline);
    if (m_depthStencilView) m_device->destroyTextureView(m_depthStencilView);
    if (m_depthStencilTex) m_device->destroyTexture(m_depthStencilTex);
    if (m_pl) m_device->destroyPipelineLayout(m_pl);
    if (m_ps) m_device->destroyShaderModule(m_ps);
    if (m_vs) m_device->destroyShaderModule(m_vs);
    if (m_ib) m_device->destroyBuffer(m_ib);
    if (m_vb) m_device->destroyBuffer(m_vb);
    if (m_compiler) { m_compiler->destroy(); delete m_compiler; }
}

int main(int argc, char** argv) { StencilOutlineSample app; return app.run(argc, argv); }
