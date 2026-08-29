#include <new>
/// Renders 4 colored quads using a single drawIndexedIndirect call with drawCount=4,
/// then overlays white line wireframes using LineList topology.

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

struct DrawIndexedIndirectArgs {
    draco::u32 indexCountPerInstance;
    draco::u32 instanceCount;
    draco::u32 startIndexLocation;
    draco::i32 baseVertexLocation;
    draco::u32 startInstanceLocation;
};

class MultiDrawIndirectSample : public sf::SampleApp {
public:
    using sf::SampleApp::SampleApp;
    std::u8string_view title() const override { return u8"Sample025 - Multi-Draw Indirect & Lines"; }
protected:
    rhi::DeviceFeatures requiredFeatures() const override {
        rhi::DeviceFeatures f{};
        f.multiDrawIndirect = true;
        return f;
    }
    draco::Status onInit() override;
    void onRender() override;
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

    draco::Status createGeometry();
    draco::Status createIndirectBuffer();
    draco::Status createLineGeometry();

    shaders::Compiler* m_compiler = nullptr;
    rhi::ShaderModule *m_vs = nullptr, *m_ps = nullptr;
    rhi::Buffer *m_vb = nullptr, *m_ib = nullptr, *m_indirectBuf = nullptr, *m_lineVb = nullptr;
    rhi::PipelineLayout *m_pl = nullptr;
    rhi::RenderPipeline *m_fillPipeline = nullptr, *m_linePipeline = nullptr;
    rhi::CommandPool *m_pool = nullptr;
    rhi::Fence *m_fence = nullptr;
    draco::u64 m_fenceVal = 0;
};

draco::Status MultiDrawIndirectSample::onInit() {
    using draco::Status, std::span, draco::u8;
    if (shaders::createCompiler(shaders::CompilerDesc{}, m_compiler) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (sf::compileToModule(m_compiler, m_device, kShader, shaders::ShaderStage::Vertex,   u8"VSMain", u8"VS", m_vs) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (sf::compileToModule(m_compiler, m_device, kShader, shaders::ShaderStage::Fragment, u8"PSMain", u8"PS", m_ps) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    if (createGeometry() != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (createIndirectBuffer() != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (createLineGeometry() != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Pipeline layout (empty - no bind groups needed).
    rhi::PipelineLayoutDesc pld{};
    if (m_device->createPipelineLayout(pld, m_pl) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Vertex layout: pos float3 + color float4, stride 28.
    rhi::VertexAttribute attrs[2] = {
        {rhi::VertexFormat::Float32x3,  0, 0},
        {rhi::VertexFormat::Float32x4, 12, 1}
    };
    rhi::VertexBufferLayout vbl{}; vbl.stride = 28; vbl.stepMode = rhi::VertexStepMode::Vertex;
    vbl.attributes = std::span<const rhi::VertexAttribute>(attrs, 2);

    rhi::ColorTargetState ct{}; ct.format = m_swapChain->format();

    // Fill pipeline (TriangleList).
    {
        rhi::RenderPipelineDesc rpd{}; rpd.layout = m_pl;
        rpd.vertex.shader = { m_vs, u8"VSMain", rhi::ShaderStage::Vertex };
        rpd.vertex.buffers = std::span<const rhi::VertexBufferLayout>(&vbl, 1);
        rpd.fragment = rhi::FragmentState{}; rpd.fragment->shader = { m_ps, u8"PSMain", rhi::ShaderStage::Fragment };
        rpd.fragment->targets = std::span<const rhi::ColorTargetState>(&ct, 1);
        rpd.primitive.topology = rhi::PrimitiveTopology::TriangleList;
        if (m_device->createRenderPipeline(rpd, m_fillPipeline) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    }

    // Line pipeline (LineList).
    {
        rhi::RenderPipelineDesc rpd{}; rpd.layout = m_pl;
        rpd.vertex.shader = { m_vs, u8"VSMain", rhi::ShaderStage::Vertex };
        rpd.vertex.buffers = std::span<const rhi::VertexBufferLayout>(&vbl, 1);
        rpd.fragment = rhi::FragmentState{}; rpd.fragment->shader = { m_ps, u8"PSMain", rhi::ShaderStage::Fragment };
        rpd.fragment->targets = std::span<const rhi::ColorTargetState>(&ct, 1);
        rpd.primitive.topology = rhi::PrimitiveTopology::LineList;
        if (m_device->createRenderPipeline(rpd, m_linePipeline) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    }

    if (m_device->createCommandPool(rhi::QueueType::Graphics, m_pool) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (m_device->createFence(0, m_fence) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    return draco::ErrorCode::Ok;
}

draco::Status MultiDrawIndirectSample::createGeometry() {
    using draco::Status, std::span, draco::u8, draco::u32;

    // 4 quads at different positions.
    static constexpr float verts[] = {
        // Quad 0: top-left (red)
        -0.9f,  0.1f, 0.5f,   0.8f, 0.2f, 0.2f, 1.0f,
        -0.1f,  0.1f, 0.5f,   0.8f, 0.2f, 0.2f, 1.0f,
        -0.1f,  0.9f, 0.5f,   1.0f, 0.4f, 0.4f, 1.0f,
        -0.9f,  0.9f, 0.5f,   1.0f, 0.4f, 0.4f, 1.0f,

        // Quad 1: top-right (green)
         0.1f,  0.1f, 0.5f,   0.2f, 0.8f, 0.2f, 1.0f,
         0.9f,  0.1f, 0.5f,   0.2f, 0.8f, 0.2f, 1.0f,
         0.9f,  0.9f, 0.5f,   0.4f, 1.0f, 0.4f, 1.0f,
         0.1f,  0.9f, 0.5f,   0.4f, 1.0f, 0.4f, 1.0f,

        // Quad 2: bottom-left (blue)
        -0.9f, -0.9f, 0.5f,   0.2f, 0.2f, 0.8f, 1.0f,
        -0.1f, -0.9f, 0.5f,   0.2f, 0.2f, 0.8f, 1.0f,
        -0.1f, -0.1f, 0.5f,   0.4f, 0.4f, 1.0f, 1.0f,
        -0.9f, -0.1f, 0.5f,   0.4f, 0.4f, 1.0f, 1.0f,

        // Quad 3: bottom-right (yellow)
         0.1f, -0.9f, 0.5f,   0.8f, 0.8f, 0.2f, 1.0f,
         0.9f, -0.9f, 0.5f,   0.8f, 0.8f, 0.2f, 1.0f,
         0.9f, -0.1f, 0.5f,   1.0f, 1.0f, 0.4f, 1.0f,
         0.1f, -0.1f, 0.5f,   1.0f, 1.0f, 0.4f, 1.0f,
    };

    static constexpr draco::u16 indices[] = {
         0,  1,  2,  0,  2,  3,   // Quad 0
         4,  5,  6,  4,  6,  7,   // Quad 1
         8,  9, 10,  8, 10, 11,   // Quad 2
        12, 13, 14, 12, 14, 15,   // Quad 3
    };

    rhi::BufferDesc vbd{}; vbd.size = sizeof(verts);
    vbd.usage = rhi::BufferUsage::Vertex | rhi::BufferUsage::CopyDst;
    vbd.memory = rhi::MemoryLocation::GpuOnly;
    if (m_device->createBuffer(vbd, m_vb) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    rhi::BufferDesc ibd{}; ibd.size = sizeof(indices);
    ibd.usage = rhi::BufferUsage::Index | rhi::BufferUsage::CopyDst;
    ibd.memory = rhi::MemoryLocation::GpuOnly;
    if (m_device->createBuffer(ibd, m_ib) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    rhi::TransferBatch* batch = nullptr; m_graphicsQueue->createTransferBatch(batch);
    batch->writeBuffer(m_vb, 0, std::span<const u8>(reinterpret_cast<const u8*>(verts), sizeof(verts)));
    batch->writeBuffer(m_ib, 0, std::span<const u8>(reinterpret_cast<const u8*>(indices), sizeof(indices)));
    batch->submit(); m_graphicsQueue->destroyTransferBatch(batch);

    return draco::ErrorCode::Ok;
}

draco::Status MultiDrawIndirectSample::createIndirectBuffer() {
    using draco::Status, std::span, draco::u8;

    // 4 indirect draw commands - one per quad.
    DrawIndexedIndirectArgs args[4] = {
        { 6, 1,  0, 0, 0 },
        { 6, 1,  6, 0, 0 },
        { 6, 1, 12, 0, 0 },
        { 6, 1, 18, 0, 0 },
    };

    rhi::BufferDesc bd{}; bd.size = sizeof(args);
    bd.usage = rhi::BufferUsage::Indirect | rhi::BufferUsage::CopyDst;
    bd.memory = rhi::MemoryLocation::GpuOnly;
    if (m_device->createBuffer(bd, m_indirectBuf) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    rhi::TransferBatch* batch = nullptr; m_graphicsQueue->createTransferBatch(batch);
    batch->writeBuffer(m_indirectBuf, 0, std::span<const u8>(reinterpret_cast<const u8*>(args), sizeof(args)));
    batch->submit(); m_graphicsQueue->destroyTransferBatch(batch);

    return draco::ErrorCode::Ok;
}

draco::Status MultiDrawIndirectSample::createLineGeometry() {
    using draco::Status, std::span, draco::u8;

    // Line wireframes for each quad: 4 edges per quad = 8 verts per quad, white lines.
    static constexpr float lineVerts[] = {
        // Quad 0 edges
        -0.9f,  0.1f, 0.4f,   1.0f, 1.0f, 1.0f, 1.0f,
        -0.1f,  0.1f, 0.4f,   1.0f, 1.0f, 1.0f, 1.0f,
        -0.1f,  0.1f, 0.4f,   1.0f, 1.0f, 1.0f, 1.0f,
        -0.1f,  0.9f, 0.4f,   1.0f, 1.0f, 1.0f, 1.0f,
        -0.1f,  0.9f, 0.4f,   1.0f, 1.0f, 1.0f, 1.0f,
        -0.9f,  0.9f, 0.4f,   1.0f, 1.0f, 1.0f, 1.0f,
        -0.9f,  0.9f, 0.4f,   1.0f, 1.0f, 1.0f, 1.0f,
        -0.9f,  0.1f, 0.4f,   1.0f, 1.0f, 1.0f, 1.0f,

        // Quad 1 edges
         0.1f,  0.1f, 0.4f,   1.0f, 1.0f, 1.0f, 1.0f,
         0.9f,  0.1f, 0.4f,   1.0f, 1.0f, 1.0f, 1.0f,
         0.9f,  0.1f, 0.4f,   1.0f, 1.0f, 1.0f, 1.0f,
         0.9f,  0.9f, 0.4f,   1.0f, 1.0f, 1.0f, 1.0f,
         0.9f,  0.9f, 0.4f,   1.0f, 1.0f, 1.0f, 1.0f,
         0.1f,  0.9f, 0.4f,   1.0f, 1.0f, 1.0f, 1.0f,
         0.1f,  0.9f, 0.4f,   1.0f, 1.0f, 1.0f, 1.0f,
         0.1f,  0.1f, 0.4f,   1.0f, 1.0f, 1.0f, 1.0f,

        // Quad 2 edges
        -0.9f, -0.9f, 0.4f,   1.0f, 1.0f, 1.0f, 1.0f,
        -0.1f, -0.9f, 0.4f,   1.0f, 1.0f, 1.0f, 1.0f,
        -0.1f, -0.9f, 0.4f,   1.0f, 1.0f, 1.0f, 1.0f,
        -0.1f, -0.1f, 0.4f,   1.0f, 1.0f, 1.0f, 1.0f,
        -0.1f, -0.1f, 0.4f,   1.0f, 1.0f, 1.0f, 1.0f,
        -0.9f, -0.1f, 0.4f,   1.0f, 1.0f, 1.0f, 1.0f,
        -0.9f, -0.1f, 0.4f,   1.0f, 1.0f, 1.0f, 1.0f,
        -0.9f, -0.9f, 0.4f,   1.0f, 1.0f, 1.0f, 1.0f,

        // Quad 3 edges
         0.1f, -0.9f, 0.4f,   1.0f, 1.0f, 1.0f, 1.0f,
         0.9f, -0.9f, 0.4f,   1.0f, 1.0f, 1.0f, 1.0f,
         0.9f, -0.9f, 0.4f,   1.0f, 1.0f, 1.0f, 1.0f,
         0.9f, -0.1f, 0.4f,   1.0f, 1.0f, 1.0f, 1.0f,
         0.9f, -0.1f, 0.4f,   1.0f, 1.0f, 1.0f, 1.0f,
         0.1f, -0.1f, 0.4f,   1.0f, 1.0f, 1.0f, 1.0f,
         0.1f, -0.1f, 0.4f,   1.0f, 1.0f, 1.0f, 1.0f,
         0.1f, -0.9f, 0.4f,   1.0f, 1.0f, 1.0f, 1.0f,
    };

    rhi::BufferDesc bd{}; bd.size = sizeof(lineVerts);
    bd.usage = rhi::BufferUsage::Vertex | rhi::BufferUsage::CopyDst;
    bd.memory = rhi::MemoryLocation::GpuOnly;
    if (m_device->createBuffer(bd, m_lineVb) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    rhi::TransferBatch* batch = nullptr; m_graphicsQueue->createTransferBatch(batch);
    batch->writeBuffer(m_lineVb, 0, std::span<const u8>(reinterpret_cast<const u8*>(lineVerts), sizeof(lineVerts)));
    batch->submit(); m_graphicsQueue->destroyTransferBatch(batch);

    return draco::ErrorCode::Ok;
}

void MultiDrawIndirectSample::onRender() {
    using draco::f32, std::span, draco::u32;
    if (m_fenceVal > 0) m_fence->wait(m_fenceVal, ~0ull);
    if (m_swapChain->acquireNextImage() != draco::ErrorCode::Ok) return;

    m_pool->reset();
    rhi::CommandEncoder* enc = nullptr;
    if (m_pool->createEncoder(enc) != draco::ErrorCode::Ok || !enc) return;
    enc->transitionTexture(m_swapChain->currentTexture(), rhi::ResourceState::Undefined, rhi::ResourceState::RenderTarget);

    rhi::ColorAttachment ca{}; ca.view = m_swapChain->currentTextureView();
    ca.loadOp = rhi::LoadOp::Clear; ca.storeOp = rhi::StoreOp::Store;
    ca.clearValue = rhi::ClearColor(0.06f, 0.06f, 0.1f, 1.0f);
    rhi::RenderPassDesc rpd{}; rpd.colorAttachments.push_back(ca);
    auto* rp = enc->beginRenderPass(rpd);

    rp->setViewport(0, 0, static_cast<f32>(m_width), static_cast<f32>(m_height), 0.0f, 1.0f);
    rp->setScissor(0, 0, m_width, m_height);

    // Pass 1: Draw all 4 quads with a single multi-draw indirect call.
    rp->setPipeline(m_fillPipeline);
    rp->setVertexBuffer(0, m_vb, 0);
    rp->setIndexBuffer(m_ib, rhi::IndexFormat::UInt16, 0);
    rp->drawIndexedIndirect(m_indirectBuf, 0, 4, static_cast<u32>(sizeof(DrawIndexedIndirectArgs)));

    // Pass 2: Draw line wireframes.
    rp->setPipeline(m_linePipeline);
    rp->setVertexBuffer(0, m_lineVb, 0);
    rp->draw(32); // 4 quads * 4 edges * 2 verts = 32 line verts

    rp->end();

    enc->transitionTexture(m_swapChain->currentTexture(), rhi::ResourceState::RenderTarget, rhi::ResourceState::Present);
    rhi::CommandBuffer* cb = enc->finish(); m_fenceVal++;
    rhi::CommandBuffer* cbs[1] = { cb };
    m_graphicsQueue->submit(std::span<rhi::CommandBuffer* const>(cbs, 1), m_fence, m_fenceVal);
    m_swapChain->present(m_graphicsQueue);
    m_pool->destroyEncoder(enc);
}

void MultiDrawIndirectSample::onShutdown() {
    if (m_fence) m_device->destroyFence(m_fence);
    if (m_pool) m_device->destroyCommandPool(m_pool);
    if (m_linePipeline) m_device->destroyRenderPipeline(m_linePipeline);
    if (m_fillPipeline) m_device->destroyRenderPipeline(m_fillPipeline);
    if (m_pl) m_device->destroyPipelineLayout(m_pl);
    if (m_ps) m_device->destroyShaderModule(m_ps);
    if (m_vs) m_device->destroyShaderModule(m_vs);
    if (m_lineVb) m_device->destroyBuffer(m_lineVb);
    if (m_indirectBuf) m_device->destroyBuffer(m_indirectBuf);
    if (m_ib) m_device->destroyBuffer(m_ib);
    if (m_vb) m_device->destroyBuffer(m_vb);
    if (m_compiler) { m_compiler->destroy(); delete m_compiler; }
}

int main(int argc, char** argv) { MultiDrawIndirectSample app; return app.run(argc, argv); }
