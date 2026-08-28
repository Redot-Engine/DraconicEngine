#include <new>
/// Pass 1: Renders triangles to 2 render targets (color + brightness).
/// Pass 2: Composites both side-by-side via fullscreen triangle.

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

class MRTSample : public sf::SampleApp {
public:
    using sf::SampleApp::SampleApp;
    std::u8string_view title() const override { return u8"Sample011 - MRT"; }
protected:
    draco::Status onInit() override;
    void onRender() override;
    void onResize(draco::u32 /*w*/, draco::u32 /*h*/) override { createRenderTargets(); }
    void onShutdown() override;
private:
    static constexpr const char8_t kGBufShader[] = u8R"(
        struct VSInput { float3 Position : TEXCOORD0; float4 Color : TEXCOORD1; };
        struct PSInput { float4 Position : SV_POSITION; float4 Color : COLOR0; };
        struct PSOutput { float4 Color : SV_TARGET0; float4 Brightness : SV_TARGET1; };
        PSInput VSMain(VSInput i) { PSInput o; o.Position = float4(i.Position,1); o.Color = i.Color; return o; }
        PSOutput PSMain(PSInput i) { PSOutput o; o.Color = i.Color;
            float lum = dot(i.Color.rgb, float3(0.299,0.587,0.114));
            o.Brightness = float4(lum,lum,lum,1); return o; }
    )";
    static constexpr const char8_t kCompShader[] = u8R"(
        Texture2D gColorTex : register(t0, space0);
        Texture2D gBrightTex : register(t1, space0);
        SamplerState gSampler : register(s0, space0);
        struct PSInput { float4 Position : SV_POSITION; float2 TexCoord : TEXCOORD0; };
        PSInput VSMain(uint vid : SV_VertexID) { PSInput o;
            float2 uv = float2((vid << 1) & 2, vid & 2);
            o.Position = float4(uv * 2.0 - 1.0, 0, 1); o.TexCoord = float2(uv.x, 1.0 - uv.y); return o; }
        float4 PSMain(PSInput i) : SV_TARGET {
            float2 uv = i.TexCoord;
            if (uv.x < 0.5) return gColorTex.Sample(gSampler, float2(uv.x*2, uv.y));
            else return gBrightTex.Sample(gSampler, float2((uv.x-0.5)*2, uv.y)); }
    )";
    static constexpr float kVerts[] = {
        -.5f,-.5f,0, 1,.2f,.2f,1,  .5f,-.5f,0, 1,.2f,.2f,1,  0,.6f,0, 1,.8f,.2f,1,
        -.3f,-.3f,0, .2f,.3f,1,1,  .7f,-.1f,0, .2f,.3f,1,1,  .2f,.5f,0, .2f,.8f,1,1,
    };
    static constexpr draco::u16 kIdx[] = { 0,1,2, 3,4,5 };

    void createRenderTargets();

    shaders::Compiler* m_compiler = nullptr;
    rhi::ShaderModule *m_gbVs=nullptr, *m_gbPs=nullptr, *m_compVs=nullptr, *m_compPs=nullptr;
    rhi::Buffer *m_vb=nullptr, *m_ib=nullptr;
    rhi::Sampler* m_sampler = nullptr;
    rhi::PipelineLayout *m_gbPl=nullptr, *m_compPl=nullptr;
    rhi::RenderPipeline *m_gbPipe=nullptr, *m_compPipe=nullptr;
    rhi::BindGroupLayout* m_compBgl = nullptr;
    rhi::BindGroup* m_compBg = nullptr;
    rhi::Texture *m_colorRT=nullptr, *m_brightRT=nullptr;
    rhi::TextureView *m_colorRTView=nullptr, *m_brightRTView=nullptr;
    rhi::CommandPool* m_pool=nullptr; rhi::Fence* m_fence=nullptr;
    draco::u64 m_fenceVal = 0;
};

void MRTSample::createRenderTargets() {
    if (m_compBg) { m_device->destroyBindGroup(m_compBg); m_compBg = nullptr; }
    if (m_colorRTView) { m_device->destroyTextureView(m_colorRTView); m_colorRTView = nullptr; }
    if (m_colorRT) { m_device->destroyTexture(m_colorRT); m_colorRT = nullptr; }
    if (m_brightRTView) { m_device->destroyTextureView(m_brightRTView); m_brightRTView = nullptr; }
    if (m_brightRT) { m_device->destroyTexture(m_brightRT); m_brightRT = nullptr; }

    rhi::TextureDesc td{}; td.format = rhi::TextureFormat::RGBA8Unorm; td.width = m_width; td.height = m_height;
    td.usage = rhi::TextureUsage::RenderTarget | rhi::TextureUsage::Sampled;
    m_device->createTexture(td, m_colorRT);
    m_device->createTexture(td, m_brightRT);
    rhi::TextureViewDesc tvd{}; tvd.format = rhi::TextureFormat::RGBA8Unorm; tvd.mipLevelCount = 1; tvd.arrayLayerCount = 1;
    m_device->createTextureView(m_colorRT, tvd, m_colorRTView);
    m_device->createTextureView(m_brightRT, tvd, m_brightRTView);

    rhi::BindGroupEntry bgE[3] = { rhi::BindGroupEntry::textureEntry(m_colorRTView),
                                   rhi::BindGroupEntry::textureEntry(m_brightRTView),
                                   rhi::BindGroupEntry::samplerEntry(m_sampler) };
    rhi::BindGroupDesc bgd{}; bgd.layout = m_compBgl; bgd.entries = std::span<const rhi::BindGroupEntry>(bgE, 3);
    m_device->createBindGroup(bgd, m_compBg);
}

draco::Status MRTSample::onInit() {
    using draco::Status, std::span, draco::u8;
    if (shaders::createCompiler(shaders::CompilerDesc{}, m_compiler) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (sf::compileToModule(m_compiler, m_device, kGBufShader, shaders::ShaderStage::Vertex,   u8"VSMain", u8"GBufVS", m_gbVs) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (sf::compileToModule(m_compiler, m_device, kGBufShader, shaders::ShaderStage::Fragment, u8"PSMain", u8"GBufPS", m_gbPs) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (sf::compileToModule(m_compiler, m_device, kCompShader, shaders::ShaderStage::Vertex,   u8"VSMain", u8"CompVS", m_compVs) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (sf::compileToModule(m_compiler, m_device, kCompShader, shaders::ShaderStage::Fragment, u8"PSMain", u8"CompPS", m_compPs) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    rhi::BufferDesc vbd{}; vbd.size = sizeof(kVerts); vbd.usage = rhi::BufferUsage::Vertex | rhi::BufferUsage::CopyDst; vbd.memory = rhi::MemoryLocation::GpuOnly;
    if (m_device->createBuffer(vbd, m_vb) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    rhi::BufferDesc ibd{}; ibd.size = sizeof(kIdx); ibd.usage = rhi::BufferUsage::Index | rhi::BufferUsage::CopyDst; ibd.memory = rhi::MemoryLocation::GpuOnly;
    if (m_device->createBuffer(ibd, m_ib) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    rhi::TransferBatch* batch = nullptr; m_graphicsQueue->createTransferBatch(batch);
    batch->writeBuffer(m_vb, 0, std::span<const u8>(reinterpret_cast<const u8*>(kVerts), sizeof(kVerts)));
    batch->writeBuffer(m_ib, 0, std::span<const u8>(reinterpret_cast<const u8*>(kIdx), sizeof(kIdx)));
    batch->submit(); m_graphicsQueue->destroyTransferBatch(batch);

    rhi::SamplerDesc sd{}; sd.minFilter = rhi::FilterMode::Nearest; sd.magFilter = rhi::FilterMode::Nearest;
    sd.addressU = rhi::AddressMode::ClampToEdge; sd.addressV = rhi::AddressMode::ClampToEdge;
    if (m_device->createSampler(sd, m_sampler) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // GBuffer pipeline (empty layout, 2 color targets).
    rhi::PipelineLayoutDesc gpld{}; if (m_device->createPipelineLayout(gpld, m_gbPl) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    rhi::VertexAttribute attrs[2] = { {rhi::VertexFormat::Float32x3, 0, 0}, {rhi::VertexFormat::Float32x4, 12, 1} };
    rhi::VertexBufferLayout vbl{}; vbl.stride = 28; vbl.attributes = std::span<const rhi::VertexAttribute>(attrs, 2);
    rhi::ColorTargetState gbCt[2] = { {.format = rhi::TextureFormat::RGBA8Unorm, .blend = {}, .writeMask = rhi::ColorWriteMask::All},
                                     {.format = rhi::TextureFormat::RGBA8Unorm, .blend = {}, .writeMask = rhi::ColorWriteMask::All} };
    rhi::RenderPipelineDesc grpd{}; grpd.layout = m_gbPl;
    grpd.vertex.shader = { m_gbVs, u8"VSMain", rhi::ShaderStage::Vertex };
    grpd.vertex.buffers = std::span<const rhi::VertexBufferLayout>(&vbl, 1);
    grpd.fragment = rhi::FragmentState{}; grpd.fragment->shader = { m_gbPs, u8"PSMain", rhi::ShaderStage::Fragment };
    grpd.fragment->targets = std::span<const rhi::ColorTargetState>(gbCt, 2);
    if (m_device->createRenderPipeline(grpd, m_gbPipe) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Composite BGL + pipeline (3 bindings: 2 textures + 1 sampler).
    rhi::BindGroupLayoutEntry cE[3] = {
        rhi::BindGroupLayoutEntry::sampledTexture(0, rhi::ShaderStage::Fragment),
        rhi::BindGroupLayoutEntry::sampledTexture(1, rhi::ShaderStage::Fragment),
        rhi::BindGroupLayoutEntry::sampler(0, rhi::ShaderStage::Fragment),
    };
    rhi::BindGroupLayoutDesc cBgld{}; cBgld.entries = std::span<const rhi::BindGroupLayoutEntry>(cE, 3);
    if (m_device->createBindGroupLayout(cBgld, m_compBgl) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    rhi::BindGroupLayout* cSets[1] = { m_compBgl };
    rhi::PipelineLayoutDesc cpld{}; cpld.bindGroupLayouts = std::span<rhi::BindGroupLayout* const>(cSets, 1);
    if (m_device->createPipelineLayout(cpld, m_compPl) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    rhi::ColorTargetState compCt{}; compCt.format = m_swapChain->format();
    rhi::RenderPipelineDesc crpd{}; crpd.layout = m_compPl;
    crpd.vertex.shader = { m_compVs, u8"VSMain", rhi::ShaderStage::Vertex };
    crpd.fragment = rhi::FragmentState{}; crpd.fragment->shader = { m_compPs, u8"PSMain", rhi::ShaderStage::Fragment };
    crpd.fragment->targets = std::span<const rhi::ColorTargetState>(&compCt, 1);
    if (m_device->createRenderPipeline(crpd, m_compPipe) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    createRenderTargets();
    if (m_device->createCommandPool(rhi::QueueType::Graphics, m_pool) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (m_device->createFence(0, m_fence) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    return draco::ErrorCode::Ok;
}

void MRTSample::onRender() {
    using draco::f32, std::span;
    if (m_fenceVal > 0) m_fence->wait(m_fenceVal, ~0ull);
    if (m_swapChain->acquireNextImage() != draco::ErrorCode::Ok) return;
    m_pool->reset();
    rhi::CommandEncoder* enc = nullptr;
    if (m_pool->createEncoder(enc) != draco::ErrorCode::Ok || !enc) return;

    // Pass 1: render to 2 RTs.
    enc->transitionTexture(m_colorRT, rhi::ResourceState::Undefined, rhi::ResourceState::RenderTarget);
    enc->transitionTexture(m_brightRT, rhi::ResourceState::Undefined, rhi::ResourceState::RenderTarget);
    rhi::ColorAttachment ca2[2];
    ca2[0].view = m_colorRTView; ca2[0].loadOp = rhi::LoadOp::Clear; ca2[0].storeOp = rhi::StoreOp::Store; ca2[0].clearValue = rhi::ClearColor(0.1f,0.1f,0.15f,1);
    ca2[1].view = m_brightRTView; ca2[1].loadOp = rhi::LoadOp::Clear; ca2[1].storeOp = rhi::StoreOp::Store; ca2[1].clearValue = rhi::ClearColor::black();
    rhi::RenderPassDesc rpd1{}; rpd1.colorAttachments.push_back(ca2[0]); rpd1.colorAttachments.push_back(ca2[1]);
    auto* rp1 = enc->beginRenderPass(rpd1);
    rp1->setPipeline(m_gbPipe);
    rp1->setViewport(0,0,static_cast<f32>(m_width),static_cast<f32>(m_height),0,1);
    rp1->setScissor(0,0,m_width,m_height);
    rp1->setVertexBuffer(0, m_vb, 0); rp1->setIndexBuffer(m_ib, rhi::IndexFormat::UInt16, 0);
    rp1->drawIndexed(6); rp1->end();
    enc->transitionTexture(m_colorRT, rhi::ResourceState::RenderTarget, rhi::ResourceState::ShaderRead);
    enc->transitionTexture(m_brightRT, rhi::ResourceState::RenderTarget, rhi::ResourceState::ShaderRead);

    // Pass 2: composite to swap chain.
    enc->transitionTexture(m_swapChain->currentTexture(), rhi::ResourceState::Undefined, rhi::ResourceState::RenderTarget);
    rhi::ColorAttachment ca1{}; ca1.view = m_swapChain->currentTextureView();
    ca1.loadOp = rhi::LoadOp::Clear; ca1.storeOp = rhi::StoreOp::Store; ca1.clearValue = rhi::ClearColor::black();
    rhi::RenderPassDesc rpd2{}; rpd2.colorAttachments.push_back(ca1);
    auto* rp2 = enc->beginRenderPass(rpd2);
    rp2->setPipeline(m_compPipe); rp2->setBindGroup(0, m_compBg);
    rp2->setViewport(0,0,static_cast<f32>(m_width),static_cast<f32>(m_height),0,1);
    rp2->setScissor(0,0,m_width,m_height);
    rp2->draw(3); rp2->end();
    enc->transitionTexture(m_swapChain->currentTexture(), rhi::ResourceState::RenderTarget, rhi::ResourceState::Present);

    rhi::CommandBuffer* cb = enc->finish(); m_fenceVal++;
    rhi::CommandBuffer* cbs[1] = { cb };
    m_graphicsQueue->submit(std::span<rhi::CommandBuffer* const>(cbs, 1), m_fence, m_fenceVal);
    m_swapChain->present(m_graphicsQueue); m_pool->destroyEncoder(enc);
}

void MRTSample::onShutdown() {
    if (m_fence) m_device->destroyFence(m_fence); if (m_pool) m_device->destroyCommandPool(m_pool);
    if (m_compPipe) m_device->destroyRenderPipeline(m_compPipe); if (m_compPl) m_device->destroyPipelineLayout(m_compPl);
    if (m_compBg) m_device->destroyBindGroup(m_compBg); if (m_compBgl) m_device->destroyBindGroupLayout(m_compBgl);
    if (m_gbPipe) m_device->destroyRenderPipeline(m_gbPipe); if (m_gbPl) m_device->destroyPipelineLayout(m_gbPl);
    if (m_brightRTView) m_device->destroyTextureView(m_brightRTView); if (m_brightRT) m_device->destroyTexture(m_brightRT);
    if (m_colorRTView) m_device->destroyTextureView(m_colorRTView); if (m_colorRT) m_device->destroyTexture(m_colorRT);
    if (m_sampler) m_device->destroySampler(m_sampler);
    if (m_ib) m_device->destroyBuffer(m_ib); if (m_vb) m_device->destroyBuffer(m_vb);
    if (m_compPs) m_device->destroyShaderModule(m_compPs); if (m_compVs) m_device->destroyShaderModule(m_compVs);
    if (m_gbPs) m_device->destroyShaderModule(m_gbPs); if (m_gbVs) m_device->destroyShaderModule(m_gbVs);
    if (m_compiler) { m_compiler->destroy(); delete m_compiler; }
}

int main(int argc, char** argv) { MRTSample app; return app.run(argc, argv); }
