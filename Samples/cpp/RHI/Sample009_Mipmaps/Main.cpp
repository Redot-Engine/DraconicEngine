#include <new>
/// Textured quad that recedes into the distance showing mip level selection.
/// Uses generateMipmaps() to auto-generate mip chain from a base texture.

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
using draco::math::Matrix4;

class MipmapSample : public sf::SampleApp {
public:
    using sf::SampleApp::SampleApp;
    std::u8string_view title() const override { return u8"Sample009 - Mipmaps"; }
protected:
    draco::Status onInit() override;
    void onRender() override;
    void onResize(draco::u32 w, draco::u32 h) override { m_depthBuf.recreate(m_device, w, h); }
    void onShutdown() override;
private:
    static constexpr const char8_t kShader[] = u8R"(
        Texture2D gTexture : register(t0, space0);
        SamplerState gSampler : register(s0, space0);
        cbuffer UBO : register(b0, space1) { row_major float4x4 MVP; };
        struct VSInput { float3 Position : TEXCOORD0; float2 TexCoord : TEXCOORD1; };
        struct PSInput { float4 Position : SV_POSITION; float2 TexCoord : TEXCOORD0; };
        PSInput VSMain(VSInput i) { PSInput o; o.Position = mul(float4(i.Position,1), MVP); o.TexCoord = i.TexCoord; return o; }
        float4 PSMain(PSInput i) : SV_TARGET { return gTexture.Sample(gSampler, i.TexCoord); }
    )";
    // Receding floor plane.
    static constexpr float kVerts[] = {
        -4, 0, 0, 0, 0,   4, 0, 0, 8, 0,   4, 0, -20, 8, 10,   -4, 0, -20, 0, 10
    };
    static constexpr draco::u16 kIdx[] = { 0,1,2, 0,2,3 };

    shaders::Compiler* m_compiler = nullptr;
    rhi::ShaderModule *m_vs = nullptr, *m_ps = nullptr;
    rhi::Buffer *m_vb = nullptr, *m_ib = nullptr, *m_ub = nullptr;
    void* m_ubMapped = nullptr;
    rhi::Texture* m_tex = nullptr; rhi::TextureView* m_texView = nullptr;
    rhi::Sampler* m_sampler = nullptr;
    rhi::BindGroupLayout *m_texBgl = nullptr, *m_uboBgl = nullptr;
    rhi::BindGroup *m_texBg = nullptr, *m_uboBg = nullptr;
    rhi::PipelineLayout* m_pl = nullptr; rhi::RenderPipeline* m_pipeline = nullptr;
    rhi::CommandPool* m_pool = nullptr; rhi::Fence* m_fence = nullptr;
    draco::u64 m_fenceVal = 0;
    sf::DepthBuffer m_depthBuf;
};

draco::Status MipmapSample::onInit() {
    using draco::Status, std::span, draco::u8, draco::u32;
    if (shaders::createCompiler(shaders::CompilerDesc{}, m_compiler) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (sf::compileToModule(m_compiler, m_device, kShader, shaders::ShaderStage::Vertex,   u8"VSMain", u8"VS", m_vs) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (sf::compileToModule(m_compiler, m_device, kShader, shaders::ShaderStage::Fragment, u8"PSMain", u8"PS", m_ps) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Buffers.
    rhi::BufferDesc vbd{}; vbd.size = sizeof(kVerts); vbd.usage = rhi::BufferUsage::Vertex | rhi::BufferUsage::CopyDst; vbd.memory = rhi::MemoryLocation::GpuOnly;
    if (m_device->createBuffer(vbd, m_vb) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    rhi::BufferDesc ibd{}; ibd.size = sizeof(kIdx); ibd.usage = rhi::BufferUsage::Index | rhi::BufferUsage::CopyDst; ibd.memory = rhi::MemoryLocation::GpuOnly;
    if (m_device->createBuffer(ibd, m_ib) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    rhi::BufferDesc ubd{}; ubd.size = 64; ubd.usage = rhi::BufferUsage::Uniform; ubd.memory = rhi::MemoryLocation::CpuToGpu;
    if (m_device->createBuffer(ubd, m_ub) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    m_ubMapped = m_ub->map();

    // Checkerboard base texture 256x256 with 9 mip levels.
    constexpr u32 tw = 256, th = 256, mipCount = 9;
    u8 texPixels[tw * th * 4];
    for (u32 y = 0; y < th; ++y) for (u32 x = 0; x < tw; ++x) {
        bool c = ((x/16) + (y/16)) % 2 == 0;
        u32 i = (y*tw+x)*4;
        texPixels[i]=c?255:30; texPixels[i+1]=c?255:30; texPixels[i+2]=c?255:200; texPixels[i+3]=255;
    }
    rhi::TextureDesc td{}; td.format = rhi::TextureFormat::RGBA8Unorm; td.width = tw; td.height = th;
    td.mipLevelCount = mipCount;
    td.usage = rhi::TextureUsage::Sampled | rhi::TextureUsage::CopySrc | rhi::TextureUsage::CopyDst | rhi::TextureUsage::RenderTarget;
    if (m_device->createTexture(td, m_tex) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Upload base mip + generate mips.
    rhi::TransferBatch* batch = nullptr; m_graphicsQueue->createTransferBatch(batch);
    batch->writeBuffer(m_vb, 0, std::span<const u8>(reinterpret_cast<const u8*>(kVerts), sizeof(kVerts)));
    batch->writeBuffer(m_ib, 0, std::span<const u8>(reinterpret_cast<const u8*>(kIdx), sizeof(kIdx)));
    rhi::TextureDataLayout layout{}; layout.bytesPerRow = tw*4; layout.rowsPerImage = th;
    batch->writeTexture(m_tex, std::span<const u8>(texPixels, sizeof(texPixels)), layout, rhi::Extent3D{tw,th,1});
    batch->submit(); m_graphicsQueue->destroyTransferBatch(batch);

    // Generate mipmaps then transition to shader-read.
    rhi::CommandPool* tmpPool = nullptr; m_device->createCommandPool(rhi::QueueType::Graphics, tmpPool);
    rhi::CommandEncoder* enc = nullptr; tmpPool->createEncoder(enc);
    enc->generateMipmaps(m_tex);
    // generateMipmaps leaves all mips in TRANSFER_SRC.
    // Transition all mips to SHADER_READ for sampling.
    rhi::TextureBarrier tb{}; tb.texture = m_tex;
    tb.oldState = rhi::ResourceState::CopySrc; tb.newState = rhi::ResourceState::ShaderRead;
    tb.baseMipLevel = 0; tb.mipLevelCount = mipCount;
    tb.baseArrayLayer = 0; tb.arrayLayerCount = 1;
    rhi::BarrierGroup bg{}; bg.textureBarriers = std::span<const rhi::TextureBarrier>(&tb, 1);
    enc->barrier(bg);
    rhi::CommandBuffer* cb = enc->finish();
    rhi::CommandBuffer* cbs[1] = { cb };
    m_graphicsQueue->submit(std::span<rhi::CommandBuffer* const>(cbs, 1));
    m_graphicsQueue->waitIdle();
    tmpPool->destroyEncoder(enc); m_device->destroyCommandPool(tmpPool);

    rhi::TextureViewDesc tvd{}; tvd.format = rhi::TextureFormat::RGBA8Unorm; tvd.mipLevelCount = mipCount; tvd.arrayLayerCount = 1;
    if (m_device->createTextureView(m_tex, tvd, m_texView) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    rhi::SamplerDesc sd{}; sd.minFilter = rhi::FilterMode::Linear; sd.magFilter = rhi::FilterMode::Linear;
    sd.mipmapFilter = rhi::MipmapFilterMode::Linear; sd.maxLod = static_cast<draco::f32>(mipCount);
    if (m_device->createSampler(sd, m_sampler) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Bind groups: set 0 = texture+sampler, set 1 = UBO.
    rhi::BindGroupLayoutEntry tE[2] = { rhi::BindGroupLayoutEntry::sampledTexture(0, rhi::ShaderStage::Fragment),
                                        rhi::BindGroupLayoutEntry::sampler(0, rhi::ShaderStage::Fragment) };
    rhi::BindGroupLayoutDesc tBgld{}; tBgld.entries = std::span<const rhi::BindGroupLayoutEntry>(tE, 2);
    if (m_device->createBindGroupLayout(tBgld, m_texBgl) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    rhi::BindGroupEntry tBgE[2] = { rhi::BindGroupEntry::textureEntry(m_texView), rhi::BindGroupEntry::samplerEntry(m_sampler) };
    rhi::BindGroupDesc tBgd{}; tBgd.layout = m_texBgl; tBgd.entries = std::span<const rhi::BindGroupEntry>(tBgE, 2);
    if (m_device->createBindGroup(tBgd, m_texBg) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    rhi::BindGroupLayoutEntry uE[1] = { rhi::BindGroupLayoutEntry::uniformBuffer(0, rhi::ShaderStage::Vertex) };
    rhi::BindGroupLayoutDesc uBgld{}; uBgld.entries = std::span<const rhi::BindGroupLayoutEntry>(uE, 1);
    if (m_device->createBindGroupLayout(uBgld, m_uboBgl) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    rhi::BindGroupEntry uBgE[1] = { rhi::BindGroupEntry::bufferEntry(m_ub, 0, 64) };
    rhi::BindGroupDesc uBgd{}; uBgd.layout = m_uboBgl; uBgd.entries = std::span<const rhi::BindGroupEntry>(uBgE, 1);
    if (m_device->createBindGroup(uBgd, m_uboBg) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    rhi::BindGroupLayout* sets[2] = { m_texBgl, m_uboBgl };
    rhi::PipelineLayoutDesc pld{}; pld.bindGroupLayouts = std::span<rhi::BindGroupLayout* const>(sets, 2);
    if (m_device->createPipelineLayout(pld, m_pl) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    m_depthBuf.recreate(m_device, m_width, m_height);

    rhi::VertexAttribute attrs[2] = { {rhi::VertexFormat::Float32x3, 0, 0}, {rhi::VertexFormat::Float32x2, 12, 1} };
    rhi::VertexBufferLayout vbl{}; vbl.stride = 20; vbl.attributes = std::span<const rhi::VertexAttribute>(attrs, 2);
    rhi::ColorTargetState ct{}; ct.format = m_swapChain->format();
    rhi::RenderPipelineDesc rpd{}; rpd.layout = m_pl;
    rpd.vertex.shader = { m_vs, u8"VSMain", rhi::ShaderStage::Vertex };
    rpd.vertex.buffers = std::span<const rhi::VertexBufferLayout>(&vbl, 1);
    rpd.fragment = rhi::FragmentState{}; rpd.fragment->shader = { m_ps, u8"PSMain", rhi::ShaderStage::Fragment };
    rpd.fragment->targets = std::span<const rhi::ColorTargetState>(&ct, 1);
    rpd.depthStencil = rhi::DepthStencilState{}; rpd.depthStencil->format = rhi::TextureFormat::Depth24PlusStencil8;
    rpd.depthStencil->depthCompare = rhi::CompareFunction::Less;
    if (m_device->createRenderPipeline(rpd, m_pipeline) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    if (m_device->createCommandPool(rhi::QueueType::Graphics, m_pool) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (m_device->createFence(0, m_fence) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    return draco::ErrorCode::Ok;
}

void MipmapSample::onRender() {
    using draco::f32, std::span;
    if (m_fenceVal > 0) m_fence->wait(m_fenceVal, ~0ull);
    if (m_swapChain->acquireNextImage() != draco::ErrorCode::Ok) return;
    f32 aspect = static_cast<f32>(m_width) / static_cast<f32>(m_height);
    Matrix4 view = Matrix4::lookAtRH(draco::math::Vector3{0, 2, 2}, draco::math::Vector3{ 0, 0, -5}, draco::math::Vector3{0,1,0});
    Matrix4 proj = Matrix4::perspectiveFovRH(draco::math::degToRad(60.0f), aspect, 0.1f, 100.0f);
    Matrix4 mvp = view * proj;
    std::memcpy(m_ubMapped, mvp.data(), 64);

    m_pool->reset();
    rhi::CommandEncoder* enc = nullptr;
    if (m_pool->createEncoder(enc) != draco::ErrorCode::Ok || !enc) return;
    enc->transitionTexture(m_swapChain->currentTexture(), rhi::ResourceState::Undefined, rhi::ResourceState::RenderTarget);
    enc->transitionTexture(m_depthBuf.texture, rhi::ResourceState::Undefined, rhi::ResourceState::DepthStencilWrite);
    rhi::ColorAttachment ca{}; ca.view = m_swapChain->currentTextureView();
    ca.loadOp = rhi::LoadOp::Clear; ca.storeOp = rhi::StoreOp::Store; ca.clearValue = rhi::ClearColor(0.1f,0.1f,0.15f,1);
    rhi::DepthStencilAttachment dsa{}; dsa.view = m_depthBuf.view;
    dsa.depthLoadOp = rhi::LoadOp::Clear; dsa.depthStoreOp = rhi::StoreOp::Store; dsa.depthClearValue = 1.0f;
    rhi::RenderPassDesc rpd{}; rpd.colorAttachments.push_back(ca); rpd.depthStencilAttachment = dsa;
    auto* rp = enc->beginRenderPass(rpd);
    rp->setPipeline(m_pipeline); rp->setBindGroup(0, m_texBg); rp->setBindGroup(1, m_uboBg);
    rp->setViewport(0,0,static_cast<f32>(m_width),static_cast<f32>(m_height),0,1);
    rp->setScissor(0,0,m_width,m_height);
    rp->setVertexBuffer(0, m_vb, 0); rp->setIndexBuffer(m_ib, rhi::IndexFormat::UInt16, 0);
    rp->drawIndexed(6); rp->end();
    enc->transitionTexture(m_swapChain->currentTexture(), rhi::ResourceState::RenderTarget, rhi::ResourceState::Present);
    rhi::CommandBuffer* cb = enc->finish(); m_fenceVal++;
    rhi::CommandBuffer* cbs[1] = { cb };
    m_graphicsQueue->submit(std::span<rhi::CommandBuffer* const>(cbs, 1), m_fence, m_fenceVal);
    m_swapChain->present(m_graphicsQueue); m_pool->destroyEncoder(enc);
}

void MipmapSample::onShutdown() {
    m_depthBuf.destroy(m_device);
    if (m_fence) m_device->destroyFence(m_fence); if (m_pool) m_device->destroyCommandPool(m_pool);
    if (m_pipeline) m_device->destroyRenderPipeline(m_pipeline); if (m_pl) m_device->destroyPipelineLayout(m_pl);
    if (m_uboBg) m_device->destroyBindGroup(m_uboBg); if (m_uboBgl) m_device->destroyBindGroupLayout(m_uboBgl);
    if (m_texBg) m_device->destroyBindGroup(m_texBg); if (m_texBgl) m_device->destroyBindGroupLayout(m_texBgl);
    if (m_sampler) m_device->destroySampler(m_sampler); if (m_texView) m_device->destroyTextureView(m_texView);
    if (m_tex) m_device->destroyTexture(m_tex); if (m_ub) m_device->destroyBuffer(m_ub);
    if (m_ib) m_device->destroyBuffer(m_ib); if (m_vb) m_device->destroyBuffer(m_vb);
    if (m_ps) m_device->destroyShaderModule(m_ps); if (m_vs) m_device->destroyShaderModule(m_vs);
    if (m_compiler) { m_compiler->destroy(); delete m_compiler; }
}

int main(int argc, char** argv) { MipmapSample app; return app.run(argc, argv); }
