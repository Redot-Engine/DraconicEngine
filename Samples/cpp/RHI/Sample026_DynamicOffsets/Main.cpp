#include <new>
/// Demonstrates dynamic uniform buffer offsets and blend constants.
/// Draws 4 quads, each reading from a different offset in one shared UBO.
/// Uses setBlendConstant with BlendFactor::Constant for per-frame color modulation.

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

class DynamicOffsetSample : public sf::SampleApp {
public:
    using sf::SampleApp::SampleApp;
    std::u8string_view title() const override { return u8"Sample026 - Dynamic Offsets & Blend Constants"; }
protected:
    draco::Status onInit() override;
    void onRender() override;
    void onShutdown() override;
private:
    void updateUBO();

    static constexpr const char8_t kShader[] = u8R"(
        cbuffer ObjectData : register(b0, space0)
        {
            float4 TintColor;
            float4 OffsetScale; // xy=offset, zw=scale
        };

        struct VSInput
        {
            float3 Position : TEXCOORD0;
        };

        struct PSInput
        {
            float4 Position : SV_POSITION;
        };

        PSInput VSMain(VSInput input)
        {
            PSInput output;
            float2 pos = input.Position.xy * OffsetScale.zw + OffsetScale.xy;
            output.Position = float4(pos, input.Position.z, 1.0);
            return output;
        }

        float4 PSMain(PSInput input) : SV_TARGET
        {
            return TintColor;
        }
    )";

    struct ObjectData {
        float tintColor[4];
        float offsetScale[4];
        // Pad to 256-byte alignment (D3D12 CBV minimum).
        float _pad[56];
    };

    shaders::Compiler* m_compiler = nullptr;
    rhi::ShaderModule *m_vs = nullptr, *m_ps = nullptr;
    rhi::Buffer *m_vb = nullptr, *m_ib = nullptr, *m_ub = nullptr;
    rhi::BindGroupLayout *m_bgl = nullptr;
    rhi::BindGroup *m_bg = nullptr;
    rhi::PipelineLayout *m_pl = nullptr;
    rhi::RenderPipeline *m_pipeline = nullptr;
    rhi::CommandPool *m_pool = nullptr;
    rhi::Fence *m_fence = nullptr;
    draco::u64 m_fenceVal = 0;
};

draco::Status DynamicOffsetSample::onInit() {
    using draco::Status, std::span, draco::u8, draco::u32;
    if (shaders::createCompiler(shaders::CompilerDesc{}, m_compiler) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (sf::compileToModule(m_compiler, m_device, kShader, shaders::ShaderStage::Vertex,   u8"VSMain", u8"DynVS", m_vs) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (sf::compileToModule(m_compiler, m_device, kShader, shaders::ShaderStage::Fragment, u8"PSMain", u8"DynPS", m_ps) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Unit quad vertices (will be transformed by UBO data).
    static constexpr float verts[] = {
        -0.5f, -0.5f, 0.5f,
         0.5f, -0.5f, 0.5f,
         0.5f,  0.5f, 0.5f,
        -0.5f,  0.5f, 0.5f,
    };
    static constexpr draco::u16 indices[] = { 0, 1, 2, 0, 2, 3 };

    rhi::BufferDesc vbd{}; vbd.size = sizeof(verts); vbd.usage = rhi::BufferUsage::Vertex | rhi::BufferUsage::CopyDst; vbd.memory = rhi::MemoryLocation::GpuOnly;
    if (m_device->createBuffer(vbd, m_vb) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    rhi::BufferDesc ibd{}; ibd.size = sizeof(indices); ibd.usage = rhi::BufferUsage::Index | rhi::BufferUsage::CopyDst; ibd.memory = rhi::MemoryLocation::GpuOnly;
    if (m_device->createBuffer(ibd, m_ib) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    rhi::TransferBatch* batch = nullptr; m_graphicsQueue->createTransferBatch(batch);
    batch->writeBuffer(m_vb, 0, std::span<const u8>(reinterpret_cast<const u8*>(verts), sizeof(verts)));
    batch->writeBuffer(m_ib, 0, std::span<const u8>(reinterpret_cast<const u8*>(indices), sizeof(indices)));
    batch->submit(); m_graphicsQueue->destroyTransferBatch(batch);

    // Uniform buffer: 4 ObjectData structs (256 bytes each = 1024 total).
    rhi::BufferDesc ubd{}; ubd.size = 256 * 4; ubd.usage = rhi::BufferUsage::Uniform; ubd.memory = rhi::MemoryLocation::CpuToGpu;
    if (m_device->createBuffer(ubd, m_ub) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Initialize UBO data.
    updateUBO();

    // Bind group layout with dynamic offset UBO.
    rhi::BindGroupLayoutEntry entry = rhi::BindGroupLayoutEntry::uniformBuffer(0, rhi::ShaderStage::Vertex | rhi::ShaderStage::Fragment);
    entry.hasDynamicOffset = true;
    rhi::BindGroupLayoutEntry entries[1] = { entry };
    rhi::BindGroupLayoutDesc bgld{}; bgld.entries = std::span<const rhi::BindGroupLayoutEntry>(entries, 1);
    if (m_device->createBindGroupLayout(bgld, m_bgl) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Bind group (bind the whole buffer, dynamic offset selects the slice).
    rhi::BindGroupEntry bgEntries[1] = { rhi::BindGroupEntry::bufferEntry(m_ub, 0, 256) };
    rhi::BindGroupDesc bgd{}; bgd.layout = m_bgl; bgd.entries = std::span<const rhi::BindGroupEntry>(bgEntries, 1);
    if (m_device->createBindGroup(bgd, m_bg) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Pipeline layout.
    rhi::BindGroupLayout* sets[1] = { m_bgl };
    rhi::PipelineLayoutDesc pld{}; pld.bindGroupLayouts = std::span<rhi::BindGroupLayout* const>(sets, 1);
    if (m_device->createPipelineLayout(pld, m_pl) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Pipeline with blend constant support.
    rhi::VertexAttribute attrs[1] = { { rhi::VertexFormat::Float32x3, 0, 0 } };
    rhi::VertexBufferLayout vbl{}; vbl.stride = 12; vbl.attributes = std::span<const rhi::VertexAttribute>(attrs, 1);

    rhi::ColorTargetState ct{}; ct.format = m_swapChain->format(); ct.writeMask = rhi::ColorWriteMask::All;
    ct.blend = rhi::BlendState{
        { rhi::BlendFactor::Constant, rhi::BlendFactor::OneMinusConstant, rhi::BlendOperation::Add },
        { rhi::BlendFactor::One,      rhi::BlendFactor::Zero,             rhi::BlendOperation::Add }
    };

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

void DynamicOffsetSample::updateUBO() {
    void* mapped = m_ub->map();
    if (!mapped) return;

    // 4 objects at different positions with different colors.
    ObjectData objs[4] = {};

    // Red, top-left.
    objs[0].tintColor[0] = 1.0f; objs[0].tintColor[1] = 0.2f; objs[0].tintColor[2] = 0.2f; objs[0].tintColor[3] = 1.0f;
    objs[0].offsetScale[0] = -0.45f; objs[0].offsetScale[1] = 0.45f; objs[0].offsetScale[2] = 0.4f; objs[0].offsetScale[3] = 0.4f;

    // Green, top-right.
    objs[1].tintColor[0] = 0.2f; objs[1].tintColor[1] = 1.0f; objs[1].tintColor[2] = 0.2f; objs[1].tintColor[3] = 1.0f;
    objs[1].offsetScale[0] = 0.45f; objs[1].offsetScale[1] = 0.45f; objs[1].offsetScale[2] = 0.4f; objs[1].offsetScale[3] = 0.4f;

    // Blue, bottom-left.
    objs[2].tintColor[0] = 0.2f; objs[2].tintColor[1] = 0.3f; objs[2].tintColor[2] = 1.0f; objs[2].tintColor[3] = 1.0f;
    objs[2].offsetScale[0] = -0.45f; objs[2].offsetScale[1] = -0.45f; objs[2].offsetScale[2] = 0.4f; objs[2].offsetScale[3] = 0.4f;

    // Yellow, bottom-right.
    objs[3].tintColor[0] = 1.0f; objs[3].tintColor[1] = 1.0f; objs[3].tintColor[2] = 0.2f; objs[3].tintColor[3] = 1.0f;
    objs[3].offsetScale[0] = 0.45f; objs[3].offsetScale[1] = -0.45f; objs[3].offsetScale[2] = 0.4f; objs[3].offsetScale[3] = 0.4f;

    std::memcpy(mapped, objs, sizeof(objs));
    m_ub->unmap();
}

void DynamicOffsetSample::onRender() {
    using draco::f32, draco::u32, std::span;
    if (m_fenceVal > 0) m_fence->wait(m_fenceVal, ~0ull);
    if (m_swapChain->acquireNextImage() != draco::ErrorCode::Ok) return;
    m_pool->reset();
    rhi::CommandEncoder* enc = nullptr;
    if (m_pool->createEncoder(enc) != draco::ErrorCode::Ok || !enc) return;
    enc->transitionTexture(m_swapChain->currentTexture(), rhi::ResourceState::Undefined, rhi::ResourceState::RenderTarget);

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

    // Animate blend constant: pulsing between full visibility and half.
    f32 pulse = 0.5f + 0.5f * std::sin(m_totalTime * 2.0f);
    rp->setBlendConstant(pulse, pulse, pulse, 1.0f);

    // Draw 4 objects, each at a different dynamic offset.
    for (u32 i = 0; i < 4; i++) {
        u32 off[1] = { i * 256 };
        rp->setBindGroup(0, m_bg, std::span<const u32>(off, 1));
        rp->drawIndexed(6);
    }

    rp->end();
    enc->transitionTexture(m_swapChain->currentTexture(), rhi::ResourceState::RenderTarget, rhi::ResourceState::Present);
    rhi::CommandBuffer* cb = enc->finish(); m_fenceVal++;
    rhi::CommandBuffer* cbs[1] = { cb };
    m_graphicsQueue->submit(std::span<rhi::CommandBuffer* const>(cbs, 1), m_fence, m_fenceVal);
    m_swapChain->present(m_graphicsQueue);
    m_pool->destroyEncoder(enc);
}

void DynamicOffsetSample::onShutdown() {
    if (m_fence) m_device->destroyFence(m_fence);
    if (m_pool) m_device->destroyCommandPool(m_pool);
    if (m_pipeline) m_device->destroyRenderPipeline(m_pipeline);
    if (m_pl) m_device->destroyPipelineLayout(m_pl);
    if (m_bg) m_device->destroyBindGroup(m_bg);
    if (m_bgl) m_device->destroyBindGroupLayout(m_bgl);
    if (m_ub) m_device->destroyBuffer(m_ub);
    if (m_ib) m_device->destroyBuffer(m_ib);
    if (m_vb) m_device->destroyBuffer(m_vb);
    if (m_ps) m_device->destroyShaderModule(m_ps);
    if (m_vs) m_device->destroyShaderModule(m_vs);
    if (m_compiler) { m_compiler->destroy(); delete m_compiler; }
}

int main(int argc, char** argv) { DynamicOffsetSample app; return app.run(argc, argv); }
