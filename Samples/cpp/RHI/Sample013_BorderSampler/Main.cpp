#include <new>
/// Demonstrates sampler border colors: TransparentBlack, OpaqueBlack, OpaqueWhite.
/// Three quads with UVs extending beyond [0,1] to show the border region.

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

class BorderSamplerSample : public sf::SampleApp {
public:
    using sf::SampleApp::SampleApp;
    std::u8string_view title() const override { return u8"Sample013 - Border Sampler"; }
protected:
    draco::Status onInit() override;
    void onRender() override;
    void onShutdown() override;
private:
    static constexpr const char8_t kShader[] = u8R"(
        Texture2D gTexture : register(t0, space0);
        SamplerState gSampler : register(s0, space0);
        cbuffer UBO : register(b0, space1) { float4 QuadOffset; };
        struct VSInput { float3 Position : TEXCOORD0; float2 TexCoord : TEXCOORD1; };
        struct PSInput { float4 Position : SV_POSITION; float2 TexCoord : TEXCOORD0; };
        PSInput VSMain(VSInput input) {
            PSInput output;
            output.Position = float4(input.Position.xy + QuadOffset.xy, input.Position.z, 1.0);
            output.TexCoord = input.TexCoord;
            return output;
        }
        float4 PSMain(PSInput input) : SV_TARGET {
            return gTexture.Sample(gSampler, input.TexCoord);
        }
    )";

    // Quad with UVs from -0.5 to 1.5 to show border region.
    static constexpr float kQuadVerts[] = {
        -0.25f, -0.25f, 0.0f,  -0.5f, -0.5f,
         0.25f, -0.25f, 0.0f,   1.5f, -0.5f,
         0.25f,  0.25f, 0.0f,   1.5f,  1.5f,
        -0.25f,  0.25f, 0.0f,  -0.5f,  1.5f,
    };
    static constexpr draco::u16 kQuadIdx[] = { 0, 1, 2, 0, 2, 3 };

    shaders::Compiler* m_compiler = nullptr;
    rhi::ShaderModule *m_vs = nullptr, *m_ps = nullptr;
    rhi::Buffer *m_vb = nullptr, *m_ib = nullptr, *m_ub = nullptr;
    void* m_ubMapped = nullptr;
    rhi::Texture* m_tex = nullptr; rhi::TextureView* m_texView = nullptr;
    rhi::Sampler *m_sampTransparent = nullptr, *m_sampOpaqueBlack = nullptr, *m_sampOpaqueWhite = nullptr;
    rhi::BindGroupLayout *m_texBgl = nullptr, *m_uboBgl = nullptr;
    rhi::BindGroup *m_bgTransparent = nullptr, *m_bgOpaqueBlack = nullptr, *m_bgOpaqueWhite = nullptr;
    rhi::BindGroup *m_uboBg = nullptr;
    rhi::PipelineLayout *m_pl = nullptr; rhi::RenderPipeline *m_pipeline = nullptr;
    rhi::CommandPool *m_pool = nullptr; rhi::Fence *m_fence = nullptr;
    draco::u64 m_fenceVal = 0;
};

draco::Status BorderSamplerSample::onInit() {
    using draco::Status, std::span, draco::u8, draco::u32;
    if (shaders::createCompiler(shaders::CompilerDesc{}, m_compiler) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (sf::compileToModule(m_compiler, m_device, kShader, shaders::ShaderStage::Vertex,   u8"VSMain", u8"VS", m_vs) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (sf::compileToModule(m_compiler, m_device, kShader, shaders::ShaderStage::Fragment, u8"PSMain", u8"PS", m_ps) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Buffers.
    rhi::BufferDesc vbd{}; vbd.size = sizeof(kQuadVerts); vbd.usage = rhi::BufferUsage::Vertex | rhi::BufferUsage::CopyDst; vbd.memory = rhi::MemoryLocation::GpuOnly;
    if (m_device->createBuffer(vbd, m_vb) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    rhi::BufferDesc ibd{}; ibd.size = sizeof(kQuadIdx); ibd.usage = rhi::BufferUsage::Index | rhi::BufferUsage::CopyDst; ibd.memory = rhi::MemoryLocation::GpuOnly;
    if (m_device->createBuffer(ibd, m_ib) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Uniform buffer: 3 slots * 256 bytes (DX12 CBV alignment).
    rhi::BufferDesc ubd{}; ubd.size = 768; ubd.usage = rhi::BufferUsage::Uniform; ubd.memory = rhi::MemoryLocation::CpuToGpu;
    if (m_device->createBuffer(ubd, m_ub) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    m_ubMapped = m_ub->map();
    // Write all 3 offsets upfront.
    float off0[4] = { -0.55f, 0.0f, 0.0f, 0.0f };
    float off1[4] = {  0.0f,  0.0f, 0.0f, 0.0f };
    float off2[4] = {  0.55f, 0.0f, 0.0f, 0.0f };
    std::memcpy(static_cast<u8*>(m_ubMapped),       off0, 16);
    std::memcpy(static_cast<u8*>(m_ubMapped) + 256, off1, 16);
    std::memcpy(static_cast<u8*>(m_ubMapped) + 512, off2, 16);

    // 8x8 checkerboard texture (red/white).
    constexpr u32 tw = 8, th = 8;
    u8 texData[tw * th * 4];
    for (u32 y = 0; y < th; ++y) for (u32 x = 0; x < tw; ++x) {
        u32 i = (y * tw + x) * 4;
        bool white = ((x + y) % 2) == 0;
        texData[i+0] = white ? 255 : 220; texData[i+1] = white ? 255 : 60;
        texData[i+2] = white ? 255 : 60;  texData[i+3] = 255;
    }
    rhi::TextureDesc td{}; td.format = rhi::TextureFormat::RGBA8Unorm; td.width = tw; td.height = th;
    td.mipLevelCount = 1; td.usage = rhi::TextureUsage::Sampled | rhi::TextureUsage::CopyDst;
    if (m_device->createTexture(td, m_tex) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    rhi::TransferBatch* batch = nullptr; m_graphicsQueue->createTransferBatch(batch);
    batch->writeBuffer(m_vb, 0, std::span<const u8>(reinterpret_cast<const u8*>(kQuadVerts), sizeof(kQuadVerts)));
    batch->writeBuffer(m_ib, 0, std::span<const u8>(reinterpret_cast<const u8*>(kQuadIdx), sizeof(kQuadIdx)));
    rhi::TextureDataLayout layout{}; layout.bytesPerRow = tw * 4; layout.rowsPerImage = th;
    batch->writeTexture(m_tex, std::span<const u8>(texData, sizeof(texData)), layout, rhi::Extent3D{tw, th, 1});
    batch->submit(); m_graphicsQueue->destroyTransferBatch(batch);

    rhi::TextureViewDesc tvd{}; tvd.format = rhi::TextureFormat::RGBA8Unorm; tvd.mipLevelCount = 1; tvd.arrayLayerCount = 1;
    if (m_device->createTextureView(m_tex, tvd, m_texView) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Three samplers with ClampToBorder and different border colors.
    auto makeSampler = [&](rhi::SamplerBorderColor bc, rhi::Sampler*& out) -> draco::Status {
        rhi::SamplerDesc sd{}; sd.minFilter = rhi::FilterMode::Nearest; sd.magFilter = rhi::FilterMode::Nearest;
        sd.addressU = rhi::AddressMode::ClampToBorder; sd.addressV = rhi::AddressMode::ClampToBorder;
        sd.addressW = rhi::AddressMode::ClampToBorder; sd.borderColor = bc;
        return m_device->createSampler(sd, out);
    };
    if (makeSampler(rhi::SamplerBorderColor::TransparentBlack, m_sampTransparent) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (makeSampler(rhi::SamplerBorderColor::OpaqueBlack,      m_sampOpaqueBlack) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (makeSampler(rhi::SamplerBorderColor::OpaqueWhite,      m_sampOpaqueWhite) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Bind group layout: set 0 = texture + sampler.
    rhi::BindGroupLayoutEntry tE[2] = { rhi::BindGroupLayoutEntry::sampledTexture(0, rhi::ShaderStage::Fragment),
                                        rhi::BindGroupLayoutEntry::sampler(0, rhi::ShaderStage::Fragment) };
    rhi::BindGroupLayoutDesc tBgld{}; tBgld.entries = std::span<const rhi::BindGroupLayoutEntry>(tE, 2);
    if (m_device->createBindGroupLayout(tBgld, m_texBgl) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Three bind groups, one per sampler.
    auto makeBG = [&](rhi::Sampler* s, rhi::BindGroup*& out) -> draco::Status {
        rhi::BindGroupEntry e[2] = { rhi::BindGroupEntry::textureEntry(m_texView), rhi::BindGroupEntry::samplerEntry(s) };
        rhi::BindGroupDesc bgd{}; bgd.layout = m_texBgl; bgd.entries = std::span<const rhi::BindGroupEntry>(e, 2);
        return m_device->createBindGroup(bgd, out);
    };
    if (makeBG(m_sampTransparent, m_bgTransparent) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (makeBG(m_sampOpaqueBlack, m_bgOpaqueBlack) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (makeBG(m_sampOpaqueWhite, m_bgOpaqueWhite) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Bind group layout: set 1 = uniform buffer with dynamic offset.
    rhi::BindGroupLayoutEntry uEntry = rhi::BindGroupLayoutEntry::uniformBuffer(0, rhi::ShaderStage::Vertex);
    uEntry.hasDynamicOffset = true;
    rhi::BindGroupLayoutEntry uE[1] = { uEntry };
    rhi::BindGroupLayoutDesc uBgld{}; uBgld.entries = std::span<const rhi::BindGroupLayoutEntry>(uE, 1);
    if (m_device->createBindGroupLayout(uBgld, m_uboBgl) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    rhi::BindGroupEntry uBgE[1] = { rhi::BindGroupEntry::bufferEntry(m_ub, 0, 16) };
    rhi::BindGroupDesc uBgd{}; uBgd.layout = m_uboBgl; uBgd.entries = std::span<const rhi::BindGroupEntry>(uBgE, 1);
    if (m_device->createBindGroup(uBgd, m_uboBg) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Pipeline layout.
    rhi::BindGroupLayout* sets[2] = { m_texBgl, m_uboBgl };
    rhi::PipelineLayoutDesc pld{}; pld.bindGroupLayouts = std::span<rhi::BindGroupLayout* const>(sets, 2);
    if (m_device->createPipelineLayout(pld, m_pl) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    rhi::VertexAttribute attrs[2] = { {rhi::VertexFormat::Float32x3, 0, 0}, {rhi::VertexFormat::Float32x2, 12, 1} };
    rhi::VertexBufferLayout vbl{}; vbl.stride = 20; vbl.attributes = std::span<const rhi::VertexAttribute>(attrs, 2);
    rhi::ColorTargetState ct{}; ct.format = m_swapChain->format();
    ct.blend = rhi::BlendState::alphaBlend();
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

void BorderSamplerSample::onRender() {
    using draco::f32, draco::u32, std::span;
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
    rp->setViewport(0, 0, static_cast<f32>(m_width), static_cast<f32>(m_height), 0, 1);
    rp->setScissor(0, 0, m_width, m_height);
    rp->setVertexBuffer(0, m_vb, 0);
    rp->setIndexBuffer(m_ib, rhi::IndexFormat::UInt16, 0);

    // Draw 3 quads side by side with different samplers and dynamic UBO offsets.
    rhi::BindGroup* texBGs[3] = { m_bgTransparent, m_bgOpaqueBlack, m_bgOpaqueWhite };
    u32 dynOffsets[3] = { 0, 256, 512 };
    for (int i = 0; i < 3; ++i) {
        rp->setBindGroup(0, texBGs[i]);
        u32 off[1] = { dynOffsets[i] };
        rp->setBindGroup(1, m_uboBg, std::span<const u32>(off, 1));
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

void BorderSamplerSample::onShutdown() {
    if (m_ub && m_ubMapped) m_ub->unmap();
    if (m_fence) m_device->destroyFence(m_fence); if (m_pool) m_device->destroyCommandPool(m_pool);
    if (m_pipeline) m_device->destroyRenderPipeline(m_pipeline); if (m_pl) m_device->destroyPipelineLayout(m_pl);
    if (m_uboBg) m_device->destroyBindGroup(m_uboBg); if (m_uboBgl) m_device->destroyBindGroupLayout(m_uboBgl);
    if (m_bgOpaqueWhite) m_device->destroyBindGroup(m_bgOpaqueWhite);
    if (m_bgOpaqueBlack) m_device->destroyBindGroup(m_bgOpaqueBlack);
    if (m_bgTransparent) m_device->destroyBindGroup(m_bgTransparent);
    if (m_texBgl) m_device->destroyBindGroupLayout(m_texBgl);
    if (m_sampOpaqueWhite) m_device->destroySampler(m_sampOpaqueWhite);
    if (m_sampOpaqueBlack) m_device->destroySampler(m_sampOpaqueBlack);
    if (m_sampTransparent) m_device->destroySampler(m_sampTransparent);
    if (m_texView) m_device->destroyTextureView(m_texView); if (m_tex) m_device->destroyTexture(m_tex);
    if (m_ub) m_device->destroyBuffer(m_ub); if (m_ib) m_device->destroyBuffer(m_ib); if (m_vb) m_device->destroyBuffer(m_vb);
    if (m_ps) m_device->destroyShaderModule(m_ps); if (m_vs) m_device->destroyShaderModule(m_vs);
    if (m_compiler) { m_compiler->destroy(); delete m_compiler; }
}

int main(int argc, char** argv) { BorderSamplerSample app; return app.run(argc, argv); }
