#include <new>
/// Renders a checkerboard-textured quad using texture, sampler, bind group.

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

class TextureSample : public sf::SampleApp {
public:
    using sf::SampleApp::SampleApp;
    std::u8string_view title() const override { return u8"Sample002 - Textured Quad"; }

protected:
    draco::Status onInit() override;
    void          onRender() override;
    void          onShutdown() override;

private:
    static constexpr const char8_t kShaderSource[] = u8R"(
        Texture2D gTexture : register(t0, space0);
        SamplerState gSampler : register(s0, space0);
        struct VSInput { float3 Position : TEXCOORD0; float2 TexCoord : TEXCOORD1; };
        struct PSInput { float4 Position : SV_POSITION; float2 TexCoord : TEXCOORD0; };
        PSInput VSMain(VSInput input) {
            PSInput output;
            output.Position = float4(input.Position, 1.0);
            output.TexCoord = input.TexCoord;
            return output;
        }
        float4 PSMain(PSInput input) : SV_TARGET {
            return gTexture.Sample(gSampler, input.TexCoord);
        }
    )";

    static constexpr float kVertexData[] = {
        -0.5f,  0.5f, 0.0f,   0.0f, 0.0f,
         0.5f,  0.5f, 0.0f,   1.0f, 0.0f,
         0.5f, -0.5f, 0.0f,   1.0f, 1.0f,
        -0.5f, -0.5f, 0.0f,   0.0f, 1.0f,
    };
    static constexpr draco::u16 kIndexData[] = { 0, 1, 2, 0, 2, 3 };

    shaders::Compiler*        m_compiler  = nullptr;
    rhi::Buffer*          m_vb        = nullptr;
    rhi::Buffer*          m_ib        = nullptr;
    rhi::ShaderModule*    m_vs        = nullptr;
    rhi::ShaderModule*    m_ps        = nullptr;
    rhi::Texture*         m_tex       = nullptr;
    rhi::TextureView*     m_texView   = nullptr;
    rhi::Sampler*         m_sampler   = nullptr;
    rhi::BindGroupLayout* m_bgl       = nullptr;
    rhi::BindGroup*       m_bg        = nullptr;
    rhi::PipelineLayout*  m_pl        = nullptr;
    rhi::RenderPipeline*  m_pipeline  = nullptr;
    rhi::CommandPool*     m_pool      = nullptr;
    rhi::Fence*           m_fence     = nullptr;
    draco::u64           m_fenceVal  = 0;
};

draco::Status TextureSample::onInit() {
    using draco::Status, std::span, draco::u8, draco::u32;

    if (shaders::createCompiler(shaders::CompilerDesc{}, m_compiler) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (sf::compileToModule(m_compiler, m_device, kShaderSource, shaders::ShaderStage::Vertex,   u8"VSMain", u8"QuadVS", m_vs) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (sf::compileToModule(m_compiler, m_device, kShaderSource, shaders::ShaderStage::Fragment, u8"PSMain", u8"QuadPS", m_ps) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Vertex + index buffers.
    rhi::BufferDesc vbd{}; vbd.size = sizeof(kVertexData); vbd.usage = rhi::BufferUsage::Vertex | rhi::BufferUsage::CopyDst; vbd.memory = rhi::MemoryLocation::GpuOnly;
    if (m_device->createBuffer(vbd, m_vb) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    rhi::BufferDesc ibd{}; ibd.size = sizeof(kIndexData); ibd.usage = rhi::BufferUsage::Index | rhi::BufferUsage::CopyDst; ibd.memory = rhi::MemoryLocation::GpuOnly;
    if (m_device->createBuffer(ibd, m_ib) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Checkerboard texture 64x64 RGBA8.
    constexpr u32 tw = 64, th = 64;
    u8 texPixels[tw * th * 4];
    for (u32 y = 0; y < th; ++y)
        for (u32 x = 0; x < tw; ++x) {
            bool checker = ((x / 8) + (y / 8)) % 2 == 0;
            u32 i = (y * tw + x) * 4;
            texPixels[i] = checker ? 255 : 50; texPixels[i+1] = checker ? 255 : 50;
            texPixels[i+2] = checker ? 255 : 200; texPixels[i+3] = 255;
        }

    rhi::TextureDesc td{}; td.format = rhi::TextureFormat::RGBA8Unorm; td.width = tw; td.height = th;
    td.usage = rhi::TextureUsage::Sampled | rhi::TextureUsage::CopyDst;
    if (m_device->createTexture(td, m_tex) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Upload.
    rhi::TransferBatch* batch = nullptr;
    m_graphicsQueue->createTransferBatch(batch);
    batch->writeBuffer(m_vb, 0, std::span<const u8>(reinterpret_cast<const u8*>(kVertexData), sizeof(kVertexData)));
    batch->writeBuffer(m_ib, 0, std::span<const u8>(reinterpret_cast<const u8*>(kIndexData), sizeof(kIndexData)));
    rhi::TextureDataLayout layout{}; layout.bytesPerRow = tw * 4; layout.rowsPerImage = th;
    batch->writeTexture(m_tex, std::span<const u8>(texPixels, sizeof(texPixels)), layout, rhi::Extent3D{tw, th, 1});
    batch->submit();
    m_graphicsQueue->destroyTransferBatch(batch);

    // Texture view + sampler.
    rhi::TextureViewDesc tvd{}; tvd.format = rhi::TextureFormat::RGBA8Unorm; tvd.mipLevelCount = 1; tvd.arrayLayerCount = 1;
    if (m_device->createTextureView(m_tex, tvd, m_texView) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    rhi::SamplerDesc sd{}; sd.minFilter = rhi::FilterMode::Nearest; sd.magFilter = rhi::FilterMode::Nearest;
    if (m_device->createSampler(sd, m_sampler) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Bind group layout + bind group.
    rhi::BindGroupLayoutEntry bglEntries[2] = {
        rhi::BindGroupLayoutEntry::sampledTexture(0, rhi::ShaderStage::Fragment),
        rhi::BindGroupLayoutEntry::sampler(0, rhi::ShaderStage::Fragment),
    };
    rhi::BindGroupLayoutDesc bgld{}; bgld.entries = std::span<const rhi::BindGroupLayoutEntry>(bglEntries, 2);
    if (m_device->createBindGroupLayout(bgld, m_bgl) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    rhi::BindGroupEntry bgEntries[2] = {
        rhi::BindGroupEntry::textureEntry(m_texView),
        rhi::BindGroupEntry::samplerEntry(m_sampler),
    };
    rhi::BindGroupDesc bgd{}; bgd.layout = m_bgl; bgd.entries = std::span<const rhi::BindGroupEntry>(bgEntries, 2);
    if (m_device->createBindGroup(bgd, m_bg) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Pipeline layout.
    rhi::BindGroupLayout* sets[1] = { m_bgl };
    rhi::PipelineLayoutDesc pld{}; pld.bindGroupLayouts = std::span<rhi::BindGroupLayout* const>(sets, 1);
    if (m_device->createPipelineLayout(pld, m_pl) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Render pipeline.
    rhi::VertexAttribute attrs[2] = { {rhi::VertexFormat::Float32x3, 0, 0}, {rhi::VertexFormat::Float32x2, 12, 1} };
    rhi::VertexBufferLayout vbl{}; vbl.stride = 20; vbl.attributes = std::span<const rhi::VertexAttribute>(attrs, 2);
    rhi::ColorTargetState ct{}; ct.format = m_swapChain->format(); ct.writeMask = rhi::ColorWriteMask::All;

    rhi::RenderPipelineDesc rpd{};
    rpd.layout = m_pl;
    rpd.vertex.shader = { m_vs, u8"VSMain", rhi::ShaderStage::Vertex };
    rpd.vertex.buffers = std::span<const rhi::VertexBufferLayout>(&vbl, 1);
    rpd.fragment = rhi::FragmentState{}; rpd.fragment->shader = { m_ps, u8"PSMain", rhi::ShaderStage::Fragment };
    rpd.fragment->targets = std::span<const rhi::ColorTargetState>(&ct, 1);
    if (m_device->createRenderPipeline(rpd, m_pipeline) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    if (m_device->createCommandPool(rhi::QueueType::Graphics, m_pool) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (m_device->createFence(0, m_fence) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    return draco::ErrorCode::Ok;
}

void TextureSample::onRender() {
    if (m_fenceVal > 0) m_fence->wait(m_fenceVal, ~0ull);
    if (m_swapChain->acquireNextImage() != draco::ErrorCode::Ok) return;
    m_pool->reset();

    rhi::CommandEncoder* enc = nullptr;
    if (m_pool->createEncoder(enc) != draco::ErrorCode::Ok || !enc) return;

    enc->transitionTexture(m_swapChain->currentTexture(), rhi::ResourceState::Undefined, rhi::ResourceState::RenderTarget);

    rhi::ColorAttachment ca{}; ca.view = m_swapChain->currentTextureView();
    ca.loadOp = rhi::LoadOp::Clear; ca.storeOp = rhi::StoreOp::Store;
    ca.clearValue = rhi::ClearColor(0.2f, 0.2f, 0.25f, 1.0f);
    rhi::RenderPassDesc rpd{}; rpd.colorAttachments.push_back(ca);

    auto* rp = enc->beginRenderPass(rpd);
    rp->setPipeline(m_pipeline);
    rp->setBindGroup(0, m_bg);
    rp->setViewport(0, 0, static_cast<draco::f32>(m_width), static_cast<draco::f32>(m_height), 0, 1);
    rp->setScissor(0, 0, m_width, m_height);
    rp->setVertexBuffer(0, m_vb, 0);
    rp->setIndexBuffer(m_ib, rhi::IndexFormat::UInt16, 0);
    rp->drawIndexed(6);
    rp->end();

    enc->transitionTexture(m_swapChain->currentTexture(), rhi::ResourceState::RenderTarget, rhi::ResourceState::Present);

    rhi::CommandBuffer* cb = enc->finish();
    m_fenceVal++;
    rhi::CommandBuffer* cbs[1] = { cb };
    m_graphicsQueue->submit(std::span<rhi::CommandBuffer* const>(cbs, 1), m_fence, m_fenceVal);
    m_swapChain->present(m_graphicsQueue);
    m_pool->destroyEncoder(enc);
}

void TextureSample::onShutdown() {
    if (m_fence)    m_device->destroyFence(m_fence);
    if (m_pool)     m_device->destroyCommandPool(m_pool);
    if (m_pipeline) m_device->destroyRenderPipeline(m_pipeline);
    if (m_pl)       m_device->destroyPipelineLayout(m_pl);
    if (m_bg)       m_device->destroyBindGroup(m_bg);
    if (m_bgl)      m_device->destroyBindGroupLayout(m_bgl);
    if (m_sampler)  m_device->destroySampler(m_sampler);
    if (m_texView)  m_device->destroyTextureView(m_texView);
    if (m_tex)      m_device->destroyTexture(m_tex);
    if (m_ps)       m_device->destroyShaderModule(m_ps);
    if (m_vs)       m_device->destroyShaderModule(m_vs);
    if (m_ib)       m_device->destroyBuffer(m_ib);
    if (m_vb)       m_device->destroyBuffer(m_vb);
    if (m_compiler) { m_compiler->destroy(); delete m_compiler; }
}

int main(int argc, char** argv) { TextureSample app; return app.run(argc, argv); }
