#include <new>
/// Sample030 - Render Bundles. Records the triangle's draw commands into a render bundle
/// (a reusable, off-thread-recordable command sequence) and replays it into the frame's render
/// pass via ExecuteBundles. The pass is begun with RenderPassContents::SecondaryCommandBuffers.
/// Exercises the RHI render-bundle path (Vulkan secondary command buffers / DX12 bundles).

#include <cstdio>
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

class RenderBundlesSample : public sf::SampleApp {
public:
    using sf::SampleApp::SampleApp;
    std::u8string_view title() const override { return u8"Sample030 - Render Bundles"; }

protected:
    draco::Status onInit() override;
    void          onRender() override;
    void          onShutdown() override;

private:
    static constexpr const char8_t kShaderSource[] = u8R"(
        struct VSInput { float3 Position : TEXCOORD0; float3 Color : TEXCOORD1; };
        struct PSInput { float4 Position : SV_POSITION; float3 Color : TEXCOORD0; };
        PSInput VSMain(VSInput input) {
            PSInput output;
            output.Position = float4(input.Position, 1.0);
            output.Color = input.Color;
            return output;
        }
        float4 PSMain(PSInput input) : SV_TARGET { return float4(input.Color, 1.0); }
    )";

    static constexpr float kVertexData[] = {
         0.0f,  0.5f, 0.0f,   1.0f, 0.0f, 0.0f,
         0.5f, -0.5f, 0.0f,   0.0f, 1.0f, 0.0f,
        -0.5f, -0.5f, 0.0f,   0.0f, 0.0f, 1.0f,
    };

    shaders::Compiler*        m_compiler  = nullptr;
    rhi::Buffer*          m_vertexBuf = nullptr;
    rhi::ShaderModule*    m_vs        = nullptr;
    rhi::ShaderModule*    m_ps        = nullptr;
    rhi::BindGroupLayout* m_bgl       = nullptr;
    rhi::PipelineLayout*  m_pl        = nullptr;
    rhi::RenderPipeline*  m_pipeline  = nullptr;
    rhi::CommandPool*     m_pool      = nullptr;
    rhi::Fence*           m_fence     = nullptr;
    draco::u64    m_fenceVal  = 0;
};

draco::Status RenderBundlesSample::onInit() {
    if (shaders::createCompiler(shaders::CompilerDesc{}, m_compiler) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (sf::compileToModule(m_compiler, m_device, kShaderSource, shaders::ShaderStage::Vertex,   u8"VSMain", u8"BundleVS", m_vs) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (sf::compileToModule(m_compiler, m_device, kShaderSource, shaders::ShaderStage::Fragment, u8"PSMain", u8"BundlePS", m_ps) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    rhi::BufferDesc bd{}; bd.size = sizeof(kVertexData); bd.usage = rhi::BufferUsage::Vertex | rhi::BufferUsage::CopyDst;
    bd.memory = rhi::MemoryLocation::GpuOnly; bd.label = u8"BundleVB";
    if (m_device->createBuffer(bd, m_vertexBuf) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    rhi::TransferBatch* batch = nullptr;
    m_graphicsQueue->createTransferBatch(batch);
    batch->writeBuffer(m_vertexBuf, 0, std::span<const draco::u8>(reinterpret_cast<const draco::u8*>(kVertexData), sizeof(kVertexData)));
    batch->submit();
    m_graphicsQueue->destroyTransferBatch(batch);

    rhi::BindGroupLayoutDesc bglDesc{}; bglDesc.label = u8"EmptyBGL";
    if (m_device->createBindGroupLayout(bglDesc, m_bgl) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    rhi::PipelineLayoutDesc pld{};
    rhi::BindGroupLayout* sets[1] = { m_bgl };
    pld.bindGroupLayouts = std::span<rhi::BindGroupLayout* const>(sets, 1);
    pld.label = u8"BundlePL";
    if (m_device->createPipelineLayout(pld, m_pl) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    rhi::VertexAttribute attrs[2] = { { rhi::VertexFormat::Float32x3, 0, 0 }, { rhi::VertexFormat::Float32x3, 12, 1 } };
    rhi::VertexBufferLayout vbl{}; vbl.stride = 24; vbl.attributes = std::span<const rhi::VertexAttribute>(attrs, 2);
    rhi::ColorTargetState ct{}; ct.format = m_swapChain->format(); ct.writeMask = rhi::ColorWriteMask::All;

    rhi::RenderPipelineDesc rpd{};
    rpd.layout   = m_pl;
    rpd.vertex.shader = { m_vs, u8"VSMain", rhi::ShaderStage::Vertex };
    rpd.vertex.buffers = std::span<const rhi::VertexBufferLayout>(&vbl, 1);
    rpd.fragment = rhi::FragmentState{};
    rpd.fragment->shader = { m_ps, u8"PSMain", rhi::ShaderStage::Fragment };
    rpd.fragment->targets = std::span<const rhi::ColorTargetState>(&ct, 1);
    rpd.primitive.topology = rhi::PrimitiveTopology::TriangleList;
    rpd.label = u8"BundlePipeline";
    if (m_device->createRenderPipeline(rpd, m_pipeline) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    if (m_device->createCommandPool(rhi::QueueType::Graphics, m_pool) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (m_device->createFence(0, m_fence) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    return draco::ErrorCode::Ok;
}

void RenderBundlesSample::onRender() {
    if (m_fenceVal > 0) m_fence->wait(m_fenceVal, ~0ull);
    if (m_swapChain->acquireNextImage() != draco::ErrorCode::Ok) return;

    m_pool->reset();
    rhi::CommandEncoder* enc = nullptr;
    if (m_pool->createEncoder(enc) != draco::ErrorCode::Ok || !enc) return;

    enc->transitionTexture(m_swapChain->currentTexture(), rhi::ResourceState::Undefined, rhi::ResourceState::RenderTarget);

    // Record the triangle's draw into a reusable bundle (could be done off-thread).
    rhi::RenderBundleDesc bdesc{};
    bdesc.colorFormats[0]   = m_swapChain->format();
    bdesc.colorFormatCount  = 1;
    bdesc.width             = m_width;
    bdesc.height            = m_height;
    bdesc.label             = u8"TriangleBundle";
    rhi::RenderBundleEncoder* be = enc->createRenderBundleEncoder(bdesc);
    rhi::RenderBundle* bundle = nullptr;
    if (be) {
        be->setPipeline(m_pipeline);
        be->setVertexBuffer(0, m_vertexBuf, 0);
        be->draw(3);
        bundle = be->finish();
    }

    rhi::ColorAttachment ca{};
    ca.view = m_swapChain->currentTextureView();
    ca.loadOp = rhi::LoadOp::Clear; ca.storeOp = rhi::StoreOp::Store;
    ca.clearValue = rhi::ClearColor(0.1f, 0.1f, 0.15f, 1.0f);

    rhi::RenderPassDesc rpd{};
    rpd.colorAttachments.push_back(ca);
    rpd.contents = rhi::RenderPassContents::SecondaryCommandBuffers;   // pass body is supplied by bundles

    auto* rp = enc->beginRenderPass(rpd);
    // Viewport/scissor must be set on the parent pass - DX12 bundles inherit these.
    rp->setViewport(0, 0, static_cast<draco::f32>(m_width), static_cast<draco::f32>(m_height), 0, 1);
    rp->setScissor(0, 0, m_width, m_height);
    if (bundle) {
        rhi::RenderBundle* bundles[1] = { bundle };
        rp->executeBundles(std::span<rhi::RenderBundle* const>(bundles, 1));
    }
    rp->end();

    enc->transitionTexture(m_swapChain->currentTexture(), rhi::ResourceState::RenderTarget, rhi::ResourceState::Present);

    rhi::CommandBuffer* cb = enc->finish();
    m_fenceVal++;
    rhi::CommandBuffer* cbs[1] = { cb };
    m_graphicsQueue->submit(std::span<rhi::CommandBuffer* const>(cbs, 1), m_fence, m_fenceVal);

    m_swapChain->present(m_graphicsQueue);
    m_pool->destroyEncoder(enc);
}

void RenderBundlesSample::onShutdown() {
    if (m_fence)     m_device->destroyFence(m_fence);
    if (m_pool)      m_device->destroyCommandPool(m_pool);
    if (m_pipeline)  m_device->destroyRenderPipeline(m_pipeline);
    if (m_pl)        m_device->destroyPipelineLayout(m_pl);
    if (m_bgl)       m_device->destroyBindGroupLayout(m_bgl);
    if (m_ps)        m_device->destroyShaderModule(m_ps);
    if (m_vs)        m_device->destroyShaderModule(m_vs);
    if (m_vertexBuf) m_device->destroyBuffer(m_vertexBuf);
    if (m_compiler) { m_compiler->destroy(); delete m_compiler; }
}

int main(int argc, char** argv) { RenderBundlesSample app; return app.run(argc, argv); }
