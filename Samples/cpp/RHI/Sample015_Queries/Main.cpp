#include <new>
/// Demonstrates GPU timestamp queries to measure render pass duration.
/// Prints render pass GPU time to console every 2 seconds.

#include <cstdint>
#include <cstdio>
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

class QuerySample : public sf::SampleApp {
public:
    using sf::SampleApp::SampleApp;
    std::u8string_view title() const override { return u8"Sample015 - GPU Queries"; }
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
    static constexpr float kVerts[] = {
         0.0f,  0.5f, 0.0f,   1.0f, 0.3f, 0.3f, 1.0f,
         0.5f, -0.5f, 0.0f,   0.3f, 1.0f, 0.3f, 1.0f,
        -0.5f, -0.5f, 0.0f,   0.3f, 0.3f, 1.0f, 1.0f,
    };
    static constexpr draco::u16 kIdx[] = { 0, 1, 2 };

    shaders::Compiler* m_compiler = nullptr;
    rhi::ShaderModule *m_vs = nullptr, *m_ps = nullptr;
    rhi::Buffer *m_vb = nullptr, *m_ib = nullptr;
    rhi::PipelineLayout *m_pl = nullptr; rhi::RenderPipeline *m_pipeline = nullptr;
    rhi::QuerySet *m_tsQuerySet = nullptr;
    rhi::Buffer *m_queryResultBuf = nullptr;
    rhi::CommandPool *m_pool = nullptr; rhi::Fence *m_fence = nullptr;
    draco::u64 m_fenceVal = 0;
    int m_frameCount = 0;
    float m_lastReportTime = 0.0f;
};

draco::Status QuerySample::onInit() {
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

    rhi::PipelineLayoutDesc pld{};
    if (m_device->createPipelineLayout(pld, m_pl) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    rhi::VertexAttribute attrs[2] = { {rhi::VertexFormat::Float32x3, 0, 0}, {rhi::VertexFormat::Float32x4, 12, 1} };
    rhi::VertexBufferLayout vbl{}; vbl.stride = 28; vbl.attributes = std::span<const rhi::VertexAttribute>(attrs, 2);
    rhi::ColorTargetState ct{}; ct.format = m_swapChain->format();
    rhi::RenderPipelineDesc rpd{}; rpd.layout = m_pl;
    rpd.vertex.shader = { m_vs, u8"VSMain", rhi::ShaderStage::Vertex };
    rpd.vertex.buffers = std::span<const rhi::VertexBufferLayout>(&vbl, 1);
    rpd.fragment = rhi::FragmentState{}; rpd.fragment->shader = { m_ps, u8"PSMain", rhi::ShaderStage::Fragment };
    rpd.fragment->targets = std::span<const rhi::ColorTargetState>(&ct, 1);
    if (m_device->createRenderPipeline(rpd, m_pipeline) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Timestamp query set: 2 queries (before + after render pass).
    rhi::QuerySetDesc qsd{}; qsd.type = rhi::QueryType::Timestamp; qsd.count = 2;
    if (m_device->createQuerySet(qsd, m_tsQuerySet) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Buffer to receive resolved query results (2 * uint64 = 16 bytes).
    rhi::BufferDesc qbd{}; qbd.size = 16; qbd.usage = rhi::BufferUsage::CopyDst; qbd.memory = rhi::MemoryLocation::GpuToCpu;
    if (m_device->createBuffer(qbd, m_queryResultBuf) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    if (m_device->createCommandPool(rhi::QueueType::Graphics, m_pool) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (m_device->createFence(0, m_fence) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    return draco::ErrorCode::Ok;
}

void QuerySample::onRender() {
    using draco::f32, draco::u64, std::span;
    if (m_fenceVal > 0) m_fence->wait(m_fenceVal, ~0ull);

    // Read back previous frame's query results (after fence wait ensures GPU is done).
    if (m_frameCount > 1) {
        void* mapped = m_queryResultBuf->map();
        if (mapped) {
            auto* timestamps = static_cast<u64*>(mapped);
            u64 begin = timestamps[0], end = timestamps[1], delta = end - begin;
            f32 period = m_graphicsQueue->timestampPeriod();
            f32 gpuTimeUs = static_cast<f32>(delta) * period / 1000.0f;
            if (m_totalTime - m_lastReportTime >= 2.0f) {
                std::printf("GPU render pass time: %.2f us (%llu ticks, period=%.2f ns)\n",
                    gpuTimeUs, static_cast<unsigned long long>(delta), period);
                m_lastReportTime = m_totalTime;
            }
            m_queryResultBuf->unmap();
        }
    }

    if (m_swapChain->acquireNextImage() != draco::ErrorCode::Ok) return;
    m_pool->reset();
    rhi::CommandEncoder* enc = nullptr;
    if (m_pool->createEncoder(enc) != draco::ErrorCode::Ok || !enc) return;

    // Reset queries for this frame.
    enc->resetQuerySet(m_tsQuerySet, 0, 2);

    enc->transitionTexture(m_swapChain->currentTexture(), rhi::ResourceState::Undefined, rhi::ResourceState::RenderTarget);

    // Timestamp before render pass.
    enc->writeTimestamp(m_tsQuerySet, 0);

    rhi::ColorAttachment ca{}; ca.view = m_swapChain->currentTextureView();
    ca.loadOp = rhi::LoadOp::Clear; ca.storeOp = rhi::StoreOp::Store;
    ca.clearValue = rhi::ClearColor(0.08f, 0.08f, 0.12f, 1.0f);
    rhi::RenderPassDesc rpd{}; rpd.colorAttachments.push_back(ca);
    auto* rp = enc->beginRenderPass(rpd);
    rp->setPipeline(m_pipeline);
    rp->setViewport(0, 0, static_cast<f32>(m_width), static_cast<f32>(m_height), 0, 1);
    rp->setScissor(0, 0, m_width, m_height);
    rp->setVertexBuffer(0, m_vb, 0);
    rp->setIndexBuffer(m_ib, rhi::IndexFormat::UInt16, 0);
    rp->drawIndexed(3);
    rp->end();

    // Timestamp after render pass.
    enc->writeTimestamp(m_tsQuerySet, 1);

    // Resolve timestamps to buffer.
    enc->resolveQuerySet(m_tsQuerySet, 0, 2, m_queryResultBuf, 0);

    enc->transitionTexture(m_swapChain->currentTexture(), rhi::ResourceState::RenderTarget, rhi::ResourceState::Present);
    rhi::CommandBuffer* cb = enc->finish(); m_fenceVal++;
    rhi::CommandBuffer* cbs[1] = { cb };
    m_graphicsQueue->submit(std::span<rhi::CommandBuffer* const>(cbs, 1), m_fence, m_fenceVal);
    m_swapChain->present(m_graphicsQueue);
    m_pool->destroyEncoder(enc);
    m_frameCount++;
}

void QuerySample::onShutdown() {
    if (m_fence) m_device->destroyFence(m_fence); if (m_pool) m_device->destroyCommandPool(m_pool);
    if (m_queryResultBuf) m_device->destroyBuffer(m_queryResultBuf);
    if (m_tsQuerySet) m_device->destroyQuerySet(m_tsQuerySet);
    if (m_pipeline) m_device->destroyRenderPipeline(m_pipeline); if (m_pl) m_device->destroyPipelineLayout(m_pl);
    if (m_ib) m_device->destroyBuffer(m_ib); if (m_vb) m_device->destroyBuffer(m_vb);
    if (m_ps) m_device->destroyShaderModule(m_ps); if (m_vs) m_device->destroyShaderModule(m_vs);
    if (m_compiler) { m_compiler->destroy(); delete m_compiler; }
}

int main(int argc, char** argv) { QuerySample app; return app.run(argc, argv); }
