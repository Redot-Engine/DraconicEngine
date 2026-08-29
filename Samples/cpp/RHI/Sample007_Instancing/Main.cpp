#include <new>
/// Renders 64 small quads in a grid using instanced draw with per-instance offset + color.
/// Instance buffer is CpuToGpu with per-frame wobble animation.

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

struct InstanceData {
    float offset[2];
    float color[4];
};

class InstancingSample : public sf::SampleApp {
public:
    using sf::SampleApp::SampleApp;
    std::u8string_view title() const override { return u8"Sample007 - Instanced Rendering"; }
protected:
    draco::Status onInit() override;
    void onRender() override;
    void onShutdown() override;
private:
    static constexpr const char8_t kShader[] = u8R"(
        struct VSInput
        {
            float3 Position  : TEXCOORD0;
            float2 Offset    : TEXCOORD1;
            float4 InstColor : TEXCOORD2;
        };
        struct PSInput
        {
            float4 Position : SV_POSITION;
            float4 Color    : COLOR0;
        };
        PSInput VSMain(VSInput input)
        {
            PSInput output;
            output.Position = float4(input.Position.xy + input.Offset, input.Position.z, 1.0);
            output.Color = input.InstColor;
            return output;
        }
        float4 PSMain(PSInput input) : SV_TARGET
        {
            return input.Color;
        }
    )";
    static constexpr int kInstanceCount = 64;

    // Unit quad vertices (pos only).
    static constexpr float kQuadVerts[] = {
        -0.04f, -0.04f, 0.0f,
         0.04f, -0.04f, 0.0f,
         0.04f,  0.04f, 0.0f,
        -0.04f,  0.04f, 0.0f,
    };
    static constexpr draco::u16 kQuadIdx[] = { 0, 1, 2, 0, 2, 3 };

    void updateInstances();

    shaders::Compiler* m_compiler = nullptr;
    rhi::ShaderModule *m_vs = nullptr, *m_ps = nullptr;
    rhi::Buffer *m_vb = nullptr, *m_ib = nullptr, *m_instBuf = nullptr;
    void* m_instMapped = nullptr;
    rhi::PipelineLayout *m_pl = nullptr;
    rhi::RenderPipeline *m_pipeline = nullptr;
    rhi::CommandPool *m_pool = nullptr; rhi::Fence *m_fence = nullptr;
    draco::u64 m_fenceVal = 0;
};

draco::Status InstancingSample::onInit() {
    using draco::Status, std::span, draco::u8, draco::f32, draco::u32;
    if (shaders::createCompiler(shaders::CompilerDesc{}, m_compiler) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (sf::compileToModule(m_compiler, m_device, kShader, shaders::ShaderStage::Vertex,   u8"VSMain", u8"VS", m_vs) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (sf::compileToModule(m_compiler, m_device, kShader, shaders::ShaderStage::Fragment, u8"PSMain", u8"PS", m_ps) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Vertex + index buffers (GpuOnly, static quad geometry).
    rhi::BufferDesc vbd{}; vbd.size = sizeof(kQuadVerts); vbd.usage = rhi::BufferUsage::Vertex | rhi::BufferUsage::CopyDst; vbd.memory = rhi::MemoryLocation::GpuOnly;
    if (m_device->createBuffer(vbd, m_vb) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    rhi::BufferDesc ibd{}; ibd.size = sizeof(kQuadIdx); ibd.usage = rhi::BufferUsage::Index | rhi::BufferUsage::CopyDst; ibd.memory = rhi::MemoryLocation::GpuOnly;
    if (m_device->createBuffer(ibd, m_ib) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    rhi::TransferBatch* batch = nullptr; m_graphicsQueue->createTransferBatch(batch);
    batch->writeBuffer(m_vb, 0, std::span<const u8>(reinterpret_cast<const u8*>(kQuadVerts), sizeof(kQuadVerts)));
    batch->writeBuffer(m_ib, 0, std::span<const u8>(reinterpret_cast<const u8*>(kQuadIdx), sizeof(kQuadIdx)));
    batch->submit(); m_graphicsQueue->destroyTransferBatch(batch);

    // Instance buffer (CpuToGpu for per-frame updates).
    rhi::BufferDesc instBd{}; instBd.size = kInstanceCount * sizeof(InstanceData); instBd.usage = rhi::BufferUsage::Vertex; instBd.memory = rhi::MemoryLocation::CpuToGpu;
    if (m_device->createBuffer(instBd, m_instBuf) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    m_instMapped = m_instBuf->map();
    if (!m_instMapped) return draco::ErrorCode::Unknown;

    // Pipeline layout (empty - no bind groups needed).
    rhi::PipelineLayoutDesc pld{};
    if (m_device->createPipelineLayout(pld, m_pl) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Two vertex buffer layouts: slot 0 = per-vertex, slot 1 = per-instance.
    rhi::VertexAttribute vtxAttrs[1] = { {rhi::VertexFormat::Float32x3, 0, 0} };
    rhi::VertexBufferLayout vtxLayout{}; vtxLayout.stride = 12; vtxLayout.stepMode = rhi::VertexStepMode::Vertex;
    vtxLayout.attributes = std::span<const rhi::VertexAttribute>(vtxAttrs, 1);

    rhi::VertexAttribute instAttrs[2] = { {rhi::VertexFormat::Float32x2, 0, 1}, {rhi::VertexFormat::Float32x4, 8, 2} };
    rhi::VertexBufferLayout instLayout{}; instLayout.stride = static_cast<u32>(sizeof(InstanceData)); instLayout.stepMode = rhi::VertexStepMode::Instance;
    instLayout.attributes = std::span<const rhi::VertexAttribute>(instAttrs, 2);

    rhi::VertexBufferLayout layouts[2] = { vtxLayout, instLayout };

    rhi::ColorTargetState ct{}; ct.format = m_swapChain->format();
    rhi::RenderPipelineDesc rpd{}; rpd.layout = m_pl;
    rpd.vertex.shader = { m_vs, u8"VSMain", rhi::ShaderStage::Vertex };
    rpd.vertex.buffers = std::span<const rhi::VertexBufferLayout>(layouts, 2);
    rpd.fragment = rhi::FragmentState{}; rpd.fragment->shader = { m_ps, u8"PSMain", rhi::ShaderStage::Fragment };
    rpd.fragment->targets = std::span<const rhi::ColorTargetState>(&ct, 1);
    if (m_device->createRenderPipeline(rpd, m_pipeline) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    if (m_device->createCommandPool(rhi::QueueType::Graphics, m_pool) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (m_device->createFence(0, m_fence) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    return draco::ErrorCode::Ok;
}

void InstancingSample::updateInstances() {
    auto* data = static_cast<InstanceData*>(m_instMapped);
    int gridSize = static_cast<int>(std::sqrt(static_cast<float>(kInstanceCount)));

    for (int i = 0; i < kInstanceCount; ++i) {
        int row = i / gridSize;
        int col = i % gridSize;

        float spacing = 2.0f / static_cast<float>(gridSize);
        float baseX = -1.0f + spacing * 0.5f + col * spacing;
        float baseY = -1.0f + spacing * 0.5f + row * spacing;

        // Animate: wobble in a circle.
        float phase = m_totalTime * 2.0f + i * 0.3f;
        float wobbleX = std::sin(phase) * 0.02f;
        float wobbleY = std::cos(phase * 1.3f) * 0.02f;

        data[i].offset[0] = baseX + wobbleX;
        data[i].offset[1] = baseY + wobbleY;

        // Color: hue based on index.
        float t = static_cast<float>(i) / static_cast<float>(kInstanceCount);
        constexpr float pi2 = 3.14159265f * 2.0f;
        data[i].color[0] = std::abs(std::sin(t * pi2));
        data[i].color[1] = std::abs(std::sin(t * pi2 + 2.094f));
        data[i].color[2] = std::abs(std::sin(t * pi2 + 4.189f));
        data[i].color[3] = 1.0f;
    }
}

void InstancingSample::onRender() {
    using draco::f32, std::span;
    if (m_fenceVal > 0) m_fence->wait(m_fenceVal, ~0ull);
    if (m_swapChain->acquireNextImage() != draco::ErrorCode::Ok) return;

    updateInstances();

    m_pool->reset();
    rhi::CommandEncoder* enc = nullptr;
    if (m_pool->createEncoder(enc) != draco::ErrorCode::Ok || !enc) return;
    enc->transitionTexture(m_swapChain->currentTexture(), rhi::ResourceState::Undefined, rhi::ResourceState::RenderTarget);

    rhi::ColorAttachment ca{}; ca.view = m_swapChain->currentTextureView();
    ca.loadOp = rhi::LoadOp::Clear; ca.storeOp = rhi::StoreOp::Store;
    ca.clearValue = rhi::ClearColor(0.05f, 0.05f, 0.08f, 1.0f);
    rhi::RenderPassDesc rpd{}; rpd.colorAttachments.push_back(ca);
    auto* rp = enc->beginRenderPass(rpd);

    rp->setPipeline(m_pipeline);
    rp->setViewport(0, 0, static_cast<f32>(m_width), static_cast<f32>(m_height), 0, 1);
    rp->setScissor(0, 0, m_width, m_height);
    rp->setVertexBuffer(0, m_vb, 0);
    rp->setVertexBuffer(1, m_instBuf, 0);
    rp->setIndexBuffer(m_ib, rhi::IndexFormat::UInt16, 0);
    rp->drawIndexed(6, kInstanceCount);
    rp->end();

    enc->transitionTexture(m_swapChain->currentTexture(), rhi::ResourceState::RenderTarget, rhi::ResourceState::Present);
    rhi::CommandBuffer* cb = enc->finish(); m_fenceVal++;
    rhi::CommandBuffer* cbs[1] = { cb };
    m_graphicsQueue->submit(std::span<rhi::CommandBuffer* const>(cbs, 1), m_fence, m_fenceVal);
    m_swapChain->present(m_graphicsQueue);
    m_pool->destroyEncoder(enc);
}

void InstancingSample::onShutdown() {
    if (m_instBuf && m_instMapped) m_instBuf->unmap();
    if (m_fence) m_device->destroyFence(m_fence); if (m_pool) m_device->destroyCommandPool(m_pool);
    if (m_pipeline) m_device->destroyRenderPipeline(m_pipeline); if (m_pl) m_device->destroyPipelineLayout(m_pl);
    if (m_instBuf) m_device->destroyBuffer(m_instBuf);
    if (m_ib) m_device->destroyBuffer(m_ib); if (m_vb) m_device->destroyBuffer(m_vb);
    if (m_ps) m_device->destroyShaderModule(m_ps); if (m_vs) m_device->destroyShaderModule(m_vs);
    if (m_compiler) { m_compiler->destroy(); delete m_compiler; }
}

int main(int argc, char** argv) { InstancingSample app; return app.run(argc, argv); }
