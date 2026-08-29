#include <new>
/// Demonstrates occlusion queries and debug labels.
/// Renders an occluder quad, then two test quads behind it with occlusion queries.
/// Prints pixel counts to console. Uses debug labels to mark render sections.

#include <cstdint>
#include <cstdio>
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

class OcclusionQuerySample : public sf::SampleApp {
public:
    using sf::SampleApp::SampleApp;
    std::u8string_view title() const override { return u8"Sample024 - Occlusion Queries & Debug Labels"; }
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

    // Geometry: 3 quads
    // Quad 0: Occluder (opaque gray, z=0.3, center)
    // Quad 1: Test A (red, z=0.7, partially behind occluder)
    // Quad 2: Test B (blue, z=0.7, fully behind occluder)
    static constexpr float kVerts[] = {
        // Quad 0: Occluder - center, near
        -0.3f, -0.4f, 0.3f,   0.4f, 0.4f, 0.4f, 1.0f,
         0.3f, -0.4f, 0.3f,   0.4f, 0.4f, 0.4f, 1.0f,
         0.3f,  0.4f, 0.3f,   0.5f, 0.5f, 0.5f, 1.0f,
        -0.3f,  0.4f, 0.3f,   0.5f, 0.5f, 0.5f, 1.0f,

        // Quad 1: Test A - partially occluded (left side visible)
        -0.7f, -0.3f, 0.7f,   1.0f, 0.3f, 0.3f, 1.0f,
         0.0f, -0.3f, 0.7f,   1.0f, 0.3f, 0.3f, 1.0f,
         0.0f,  0.3f, 0.7f,   1.0f, 0.5f, 0.5f, 1.0f,
        -0.7f,  0.3f, 0.7f,   1.0f, 0.5f, 0.5f, 1.0f,

        // Quad 2: Test B - fully occluded (behind occluder)
        -0.15f, -0.2f, 0.7f,  0.3f, 0.3f, 1.0f, 1.0f,
         0.15f, -0.2f, 0.7f,  0.3f, 0.3f, 1.0f, 1.0f,
         0.15f,  0.2f, 0.7f,  0.5f, 0.5f, 1.0f, 1.0f,
        -0.15f,  0.2f, 0.7f,  0.5f, 0.5f, 1.0f, 1.0f,
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
    rhi::QuerySet *m_occlusionQuerySet = nullptr;
    rhi::Buffer *m_queryResultBuf = nullptr;
    rhi::CommandPool *m_pool = nullptr;
    rhi::Fence *m_fence = nullptr;
    draco::u64 m_fenceVal = 0;
    int m_frameCount = 0;
    float m_lastReportTime = 0.0f;
};

void OcclusionQuerySample::recreateDepth(draco::u32 w, draco::u32 h) {
    if (m_depthView) { m_device->destroyTextureView(m_depthView); m_depthView = nullptr; }
    if (m_depthTex) { m_device->destroyTexture(m_depthTex); m_depthTex = nullptr; }

    rhi::TextureDesc td = rhi::TextureDesc::depthBuffer(rhi::TextureFormat::Depth24PlusStencil8, w, h, 1, u8"OccDepthTex");
    m_device->createTexture(td, m_depthTex);
    rhi::TextureViewDesc tvd{}; tvd.format = rhi::TextureFormat::Depth24PlusStencil8; tvd.dimension = rhi::TextureViewDimension::Texture2D;
    tvd.mipLevelCount = 1; tvd.arrayLayerCount = 1;
    m_device->createTextureView(m_depthTex, tvd, m_depthView);
}

draco::Status OcclusionQuerySample::onInit() {
    using draco::Status, std::span, draco::u8;
    if (shaders::createCompiler(shaders::CompilerDesc{}, m_compiler) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (sf::compileToModule(m_compiler, m_device, kShader, shaders::ShaderStage::Vertex,   u8"VSMain", u8"OccVS", m_vs) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (sf::compileToModule(m_compiler, m_device, kShader, shaders::ShaderStage::Fragment, u8"PSMain", u8"OccPS", m_ps) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    rhi::BufferDesc vbd{}; vbd.size = sizeof(kVerts); vbd.usage = rhi::BufferUsage::Vertex | rhi::BufferUsage::CopyDst; vbd.memory = rhi::MemoryLocation::GpuOnly; vbd.label = u8"OccVB";
    if (m_device->createBuffer(vbd, m_vb) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    rhi::BufferDesc ibd{}; ibd.size = sizeof(kIdx); ibd.usage = rhi::BufferUsage::Index | rhi::BufferUsage::CopyDst; ibd.memory = rhi::MemoryLocation::GpuOnly; ibd.label = u8"OccIB";
    if (m_device->createBuffer(ibd, m_ib) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    rhi::TransferBatch* batch = nullptr; m_graphicsQueue->createTransferBatch(batch);
    batch->writeBuffer(m_vb, 0, std::span<const u8>(reinterpret_cast<const u8*>(kVerts), sizeof(kVerts)));
    batch->writeBuffer(m_ib, 0, std::span<const u8>(reinterpret_cast<const u8*>(kIdx), sizeof(kIdx)));
    batch->submit(); m_graphicsQueue->destroyTransferBatch(batch);

    rhi::PipelineLayoutDesc pld{}; pld.label = u8"OccPL";
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
    rpd.label = u8"OccPipeline";
    if (m_device->createRenderPipeline(rpd, m_pipeline) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Occlusion query set: 2 queries (one per test quad).
    rhi::QuerySetDesc qsd{}; qsd.type = rhi::QueryType::Occlusion; qsd.count = 2; qsd.label = u8"OcclusionQS";
    if (m_device->createQuerySet(qsd, m_occlusionQuerySet) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Buffer for query results (2 * uint64 = 16 bytes).
    rhi::BufferDesc qbd{}; qbd.size = 16; qbd.usage = rhi::BufferUsage::CopyDst; qbd.memory = rhi::MemoryLocation::GpuToCpu; qbd.label = u8"OccResultBuf";
    if (m_device->createBuffer(qbd, m_queryResultBuf) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    if (m_device->createCommandPool(rhi::QueueType::Graphics, m_pool) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (m_device->createFence(0, m_fence) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    return draco::ErrorCode::Ok;
}

void OcclusionQuerySample::onRender() {
    using draco::f32, draco::u64, std::span;
    if (m_fenceVal > 0) m_fence->wait(m_fenceVal, ~0ull);

    // Read back previous frame's occlusion results (after fence wait ensures GPU is done).
    if (m_frameCount > 1) {
        void* mapped = m_queryResultBuf->map();
        if (mapped) {
            auto* results = static_cast<u64*>(mapped);
            u64 pixelsA = results[0];
            u64 pixelsB = results[1];

            if (m_totalTime - m_lastReportTime >= 2.0f) {
                std::printf("Occlusion: QuadA=%llu pixels, QuadB=%llu pixels (B should be ~0)\n",
                    static_cast<unsigned long long>(pixelsA), static_cast<unsigned long long>(pixelsB));
                m_lastReportTime = m_totalTime;
            }
            m_queryResultBuf->unmap();
        }
    }

    if (m_swapChain->acquireNextImage() != draco::ErrorCode::Ok) return;

    m_pool->reset();
    rhi::CommandEncoder* enc = nullptr;
    if (m_pool->createEncoder(enc) != draco::ErrorCode::Ok || !enc) return;

    // Debug label: frame start.
    enc->insertDebugLabel(u8"Frame Start", 0, 1, 0);

    // Reset queries for this frame.
    enc->resetQuerySet(m_occlusionQuerySet, 0, 2);

    enc->transitionTexture(m_swapChain->currentTexture(), rhi::ResourceState::Undefined, rhi::ResourceState::RenderTarget);
    enc->transitionTexture(m_depthTex, rhi::ResourceState::Undefined, rhi::ResourceState::DepthStencilWrite);

    // Debug label: render pass.
    enc->beginDebugLabel(u8"Main Render Pass", 0.2f, 0.5f, 1.0f);

    rhi::ColorAttachment ca{}; ca.view = m_swapChain->currentTextureView();
    ca.loadOp = rhi::LoadOp::Clear; ca.storeOp = rhi::StoreOp::Store;
    ca.clearValue = rhi::ClearColor(0.08f, 0.08f, 0.12f, 1.0f);
    rhi::DepthStencilAttachment dsa{}; dsa.view = m_depthView;
    dsa.depthLoadOp = rhi::LoadOp::Clear; dsa.depthStoreOp = rhi::StoreOp::Store; dsa.depthClearValue = 1.0f;
    rhi::RenderPassDesc rpd{}; rpd.colorAttachments.push_back(ca); rpd.depthStencilAttachment = dsa;
    auto* rp = enc->beginRenderPass(rpd);

    rp->setPipeline(m_pipeline);
    rp->setViewport(0, 0, static_cast<f32>(m_width), static_cast<f32>(m_height), 0, 1);
    rp->setScissor(0, 0, m_width, m_height);
    rp->setVertexBuffer(0, m_vb, 0);
    rp->setIndexBuffer(m_ib, rhi::IndexFormat::UInt16, 0);

    // Draw occluder first (writes depth).
    rp->drawIndexed(6, 1, 0, 0, 0);

    // Draw test quad A with occlusion query 0.
    rp->beginOcclusionQuery(m_occlusionQuerySet, 0);
    rp->drawIndexed(6, 1, 6, 0, 0);
    rp->endOcclusionQuery(m_occlusionQuerySet, 0);

    // Draw test quad B with occlusion query 1.
    rp->beginOcclusionQuery(m_occlusionQuerySet, 1);
    rp->drawIndexed(6, 1, 12, 0, 0);
    rp->endOcclusionQuery(m_occlusionQuerySet, 1);

    rp->end();

    enc->endDebugLabel();

    // Resolve occlusion queries to buffer.
    enc->resolveQuerySet(m_occlusionQuerySet, 0, 2, m_queryResultBuf, 0);

    enc->transitionTexture(m_swapChain->currentTexture(), rhi::ResourceState::RenderTarget, rhi::ResourceState::Present);
    rhi::CommandBuffer* cb = enc->finish(); m_fenceVal++;
    rhi::CommandBuffer* cbs[1] = { cb };
    m_graphicsQueue->submit(std::span<rhi::CommandBuffer* const>(cbs, 1), m_fence, m_fenceVal);
    m_swapChain->present(m_graphicsQueue);
    m_pool->destroyEncoder(enc);

    m_frameCount++;
}

void OcclusionQuerySample::onShutdown() {
    if (m_fence) m_device->destroyFence(m_fence); if (m_pool) m_device->destroyCommandPool(m_pool);
    if (m_queryResultBuf) m_device->destroyBuffer(m_queryResultBuf);
    if (m_occlusionQuerySet) m_device->destroyQuerySet(m_occlusionQuerySet);
    if (m_pipeline) m_device->destroyRenderPipeline(m_pipeline); if (m_pl) m_device->destroyPipelineLayout(m_pl);
    if (m_depthView) m_device->destroyTextureView(m_depthView);
    if (m_depthTex) m_device->destroyTexture(m_depthTex);
    if (m_ib) m_device->destroyBuffer(m_ib); if (m_vb) m_device->destroyBuffer(m_vb);
    if (m_ps) m_device->destroyShaderModule(m_ps); if (m_vs) m_device->destroyShaderModule(m_vs);
    if (m_compiler) { m_compiler->destroy(); delete m_compiler; }
}

int main(int argc, char** argv) { OcclusionQuerySample app; return app.run(argc, argv); }
