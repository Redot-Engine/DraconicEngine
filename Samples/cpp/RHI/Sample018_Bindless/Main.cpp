#include <new>
/// Demonstrates bindless texture arrays with material index via push constants.
/// Creates 4 procedural textures, binds them in a bindless array, and renders
/// 4 quads each selecting a different texture via push constant index.

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

class BindlessSample : public sf::SampleApp {
public:
    using sf::SampleApp::SampleApp;
    std::u8string_view title() const override { return u8"Sample018 - Bindless Textures"; }
    rhi::DeviceFeatures requiredFeatures() const override {
        rhi::DeviceFeatures f{}; f.bindlessDescriptors = true; return f;
    }
protected:
    draco::Status onInit() override;
    void onRender() override;
    void onShutdown() override;
private:
    draco::Status createTextures();
    void generatePixel(draco::u32 texIndex, draco::u32 x, draco::u32 y, draco::u8* rgba);

    static constexpr const char8_t kShader[] = u8R"(
        Texture2D gTextures[] : register(t0, space0);
        SamplerState gSampler : register(s0, space1);

        struct PushData
        {
            uint TextureIndex;
            float OffsetX;
            float OffsetY;
            float Padding;
        };

        [[vk::push_constant]] ConstantBuffer<PushData> gPush : register(b0, space2);

        struct PSInput
        {
            float4 Position : SV_POSITION;
            float2 TexCoord : TEXCOORD0;
        };

        PSInput VSMain(uint vertexID : SV_VertexID)
        {
            // Fullscreen-quad-style: 4 vertices for a unit quad
            float2 positions[4] = {
                float2(-0.4, 0.4),
                float2( 0.4, 0.4),
                float2(-0.4,-0.4),
                float2( 0.4,-0.4)
            };
            float2 uvs[4] = {
                float2(0, 0), float2(1, 0),
                float2(0, 1), float2(1, 1)
            };

            PSInput output;
            float2 pos = positions[vertexID];
            pos.x += gPush.OffsetX;
            pos.y += gPush.OffsetY;
            output.Position = float4(pos, 0.0, 1.0);
            output.TexCoord = uvs[vertexID];
            return output;
        }

        float4 PSMain(PSInput input) : SV_TARGET
        {
            return gTextures[gPush.TextureIndex].Sample(gSampler, input.TexCoord);
        }
    )";

    static constexpr draco::u32 kTexSize = 64;
    static constexpr draco::u32 kNumTextures = 4;

    shaders::Compiler* m_compiler = nullptr;
    rhi::ShaderModule* m_vs = nullptr;
    rhi::ShaderModule* m_ps = nullptr;

    // Textures
    rhi::Texture*     m_textures[kNumTextures]     = {};
    rhi::TextureView* m_textureViews[kNumTextures] = {};
    rhi::Sampler* m_sampler = nullptr;

    // Bindless bind group (space0: bindless textures)
    rhi::BindGroupLayout* m_bindlessBgl = nullptr;
    rhi::BindGroup*       m_bindlessBg  = nullptr;

    // Sampler bind group (space1: sampler)
    rhi::BindGroupLayout* m_samplerBgl = nullptr;
    rhi::BindGroup*       m_samplerBg  = nullptr;

    rhi::PipelineLayout*  m_pl       = nullptr;
    rhi::RenderPipeline*  m_pipeline = nullptr;
    rhi::CommandPool*     m_pool     = nullptr;
    rhi::Fence*           m_fence    = nullptr;
    draco::u64           m_fenceVal = 0;
};

draco::Status BindlessSample::onInit() {
    using draco::Status, std::span, draco::u8, draco::u32;

    if (shaders::createCompiler(shaders::CompilerDesc{}, m_compiler) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (sf::compileToModule(m_compiler, m_device, kShader, shaders::ShaderStage::Vertex,   u8"VSMain", u8"BindlessVS", m_vs) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (sf::compileToModule(m_compiler, m_device, kShader, shaders::ShaderStage::Fragment, u8"PSMain", u8"BindlessPS", m_ps) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Create 4 procedural textures with different patterns
    if (createTextures() != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Sampler
    rhi::SamplerDesc sd{}; sd.minFilter = rhi::FilterMode::Linear; sd.magFilter = rhi::FilterMode::Linear;
    sd.addressU = rhi::AddressMode::Repeat; sd.addressV = rhi::AddressMode::Repeat;
    sd.label = u8"BindlessSampler";
    if (m_device->createSampler(sd, m_sampler) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Bindless BGL (space0): unbounded texture array
    rhi::BindGroupLayoutEntry bindlessEntry{};
    bindlessEntry.binding    = 0;
    bindlessEntry.visibility = rhi::ShaderStage::Fragment;
    bindlessEntry.type       = rhi::BindingType::BindlessTextures;
    bindlessEntry.textureDimension = rhi::TextureViewDimension::Texture2D;
    bindlessEntry.count      = 0xFFFFFFFF;
    rhi::BindGroupLayoutEntry blEntries[1] = { bindlessEntry };
    rhi::BindGroupLayoutDesc blBgld{}; blBgld.entries = std::span<const rhi::BindGroupLayoutEntry>(blEntries, 1);
    blBgld.label = u8"BindlessBGL";
    if (m_device->createBindGroupLayout(blBgld, m_bindlessBgl) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Create bindless bind group (no entries at creation - populated via updateBindless)
    rhi::BindGroupDesc blBgd{}; blBgd.layout = m_bindlessBgl; blBgd.label = u8"BindlessBG";
    if (m_device->createBindGroup(blBgd, m_bindlessBg) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Populate bindless slots
    rhi::BindlessUpdateEntry bindlessUpdates[kNumTextures];
    for (u32 i = 0; i < kNumTextures; ++i) {
        bindlessUpdates[i] = {};
        bindlessUpdates[i].layoutIndex = 0;
        bindlessUpdates[i].arrayIndex  = i;
        bindlessUpdates[i].textureView = m_textureViews[i];
    }
    m_bindlessBg->updateBindless(std::span<const rhi::BindlessUpdateEntry>(bindlessUpdates, kNumTextures));

    // Sampler BGL (space1)
    rhi::BindGroupLayoutEntry samplerEntry = rhi::BindGroupLayoutEntry::sampler(0, rhi::ShaderStage::Fragment);
    rhi::BindGroupLayoutEntry sEntries[1] = { samplerEntry };
    rhi::BindGroupLayoutDesc sBgld{}; sBgld.entries = std::span<const rhi::BindGroupLayoutEntry>(sEntries, 1);
    sBgld.label = u8"SamplerBGL";
    if (m_device->createBindGroupLayout(sBgld, m_samplerBgl) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    rhi::BindGroupEntry sBgEntries[1] = { rhi::BindGroupEntry::samplerEntry(m_sampler) };
    rhi::BindGroupDesc sBgd{}; sBgd.layout = m_samplerBgl;
    sBgd.entries = std::span<const rhi::BindGroupEntry>(sBgEntries, 1); sBgd.label = u8"SamplerBG";
    if (m_device->createBindGroup(sBgd, m_samplerBg) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Pipeline layout: group 0 = bindless textures, group 1 = sampler, push constants
    rhi::BindGroupLayout* sets[2] = { m_bindlessBgl, m_samplerBgl };
    rhi::PushConstantRange pcr{}; pcr.stages = rhi::ShaderStage::Vertex | rhi::ShaderStage::Fragment;
    pcr.offset = 0; pcr.size = 16;
    rhi::PushConstantRange pushRanges[1] = { pcr };
    rhi::PipelineLayoutDesc pld{}; pld.bindGroupLayouts = std::span<rhi::BindGroupLayout* const>(sets, 2);
    pld.pushConstantRanges = std::span<const rhi::PushConstantRange>(pushRanges, 1);
    pld.label = u8"BindlessPL";
    if (m_device->createPipelineLayout(pld, m_pl) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Render pipeline (no vertex buffers - SV_VertexID driven)
    rhi::ColorTargetState ct{}; ct.format = m_swapChain->format();
    rhi::RenderPipelineDesc rpd{}; rpd.layout = m_pl;
    rpd.vertex.shader = { m_vs, u8"VSMain", rhi::ShaderStage::Vertex };
    rpd.fragment = rhi::FragmentState{}; rpd.fragment->shader = { m_ps, u8"PSMain", rhi::ShaderStage::Fragment };
    rpd.fragment->targets = std::span<const rhi::ColorTargetState>(&ct, 1);
    rpd.primitive.topology = rhi::PrimitiveTopology::TriangleStrip;
    rpd.label = u8"BindlessPipeline";
    if (m_device->createRenderPipeline(rpd, m_pipeline) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    if (m_device->createCommandPool(rhi::QueueType::Graphics, m_pool) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (m_device->createFence(0, m_fence) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    return draco::ErrorCode::Ok;
}

draco::Status BindlessSample::createTextures() {
    using draco::Status, std::span, draco::u8, draco::u32;

    constexpr u32 rowBytes = kTexSize * 4;
    constexpr u32 texBytes = rowBytes * kTexSize;
    u8 pixels[texBytes];

    rhi::TransferBatch* batch = nullptr;
    m_graphicsQueue->createTransferBatch(batch);

    for (u32 t = 0; t < kNumTextures; ++t) {
        // Generate pattern
        for (u32 y = 0; y < kTexSize; ++y) {
            for (u32 x = 0; x < kTexSize; ++x) {
                u32 offset = (y * kTexSize + x) * 4;
                generatePixel(t, x, y, &pixels[offset]);
            }
        }

        rhi::TextureDesc td{}; td.format = rhi::TextureFormat::RGBA8Unorm;
        td.width = kTexSize; td.height = kTexSize;
        td.mipLevelCount = 1; td.usage = rhi::TextureUsage::Sampled | rhi::TextureUsage::CopyDst;
        td.label = u8"BindlessTex";
        if (m_device->createTexture(td, m_textures[t]) != draco::ErrorCode::Ok) {
            m_graphicsQueue->destroyTransferBatch(batch); return draco::ErrorCode::Unknown;
        }

        rhi::TextureDataLayout layout{}; layout.bytesPerRow = rowBytes; layout.rowsPerImage = kTexSize;
        batch->writeTexture(m_textures[t], std::span<const u8>(pixels, texBytes),
            layout, rhi::Extent3D{kTexSize, kTexSize, 1});

        rhi::TextureViewDesc tvd{}; tvd.format = rhi::TextureFormat::RGBA8Unorm;
        tvd.mipLevelCount = 1; tvd.arrayLayerCount = 1;
        if (m_device->createTextureView(m_textures[t], tvd, m_textureViews[t]) != draco::ErrorCode::Ok) {
            m_graphicsQueue->destroyTransferBatch(batch); return draco::ErrorCode::Unknown;
        }
    }

    batch->submit();
    m_graphicsQueue->destroyTransferBatch(batch);
    return draco::ErrorCode::Ok;
}

void BindlessSample::generatePixel(draco::u32 texIndex, draco::u32 x, draco::u32 y, draco::u8* rgba) {
    float fx = static_cast<float>(x) / static_cast<float>(kTexSize);
    float fy = static_cast<float>(y) / static_cast<float>(kTexSize);

    switch (texIndex) {
    case 0: { // Red/white checkerboard
        bool check = ((x / 8) + (y / 8)) % 2 == 0;
        rgba[0] = check ? 220 : 255;
        rgba[1] = check ? 30  : 255;
        rgba[2] = check ? 30  : 255;
        rgba[3] = 255;
        break;
    }
    case 1: { // Green gradient with stripes
        auto g = static_cast<draco::u8>(fx * 255.0f);
        bool stripe = (y % 16) < 8;
        rgba[0] = stripe ? 30 : 10;
        rgba[1] = stripe ? g  : static_cast<draco::u8>(g / 2);
        rgba[2] = stripe ? 50 : 30;
        rgba[3] = 255;
        break;
    }
    case 2: { // Blue circles
        float cx = fx - 0.5f, cy = fy - 0.5f;
        float dist = std::sqrt(cx * cx + cy * cy);
        float rings = std::sin(dist * 30.0f) * 0.5f + 0.5f;
        rgba[0] = static_cast<draco::u8>(rings * 60);
        rgba[1] = static_cast<draco::u8>(rings * 100);
        rgba[2] = static_cast<draco::u8>(rings * 255);
        rgba[3] = 255;
        break;
    }
    default: { // Yellow/purple diagonal
        float diag = std::sin((fx + fy) * 10.0f) * 0.5f + 0.5f;
        rgba[0] = static_cast<draco::u8>(diag * 255 + (1.0f - diag) * 120);
        rgba[1] = static_cast<draco::u8>(diag * 220);
        rgba[2] = static_cast<draco::u8>((1.0f - diag) * 200);
        rgba[3] = 255;
        break;
    }
    }
}

void BindlessSample::onRender() {
    using draco::f32, draco::u32, std::span;
    if (m_fenceVal > 0) m_fence->wait(m_fenceVal, ~0ull);
    if (m_swapChain->acquireNextImage() != draco::ErrorCode::Ok) return;
    m_pool->reset();
    rhi::CommandEncoder* enc = nullptr;
    if (m_pool->createEncoder(enc) != draco::ErrorCode::Ok || !enc) return;
    enc->transitionTexture(m_swapChain->currentTexture(), rhi::ResourceState::Undefined, rhi::ResourceState::RenderTarget);

    rhi::ColorAttachment ca{}; ca.view = m_swapChain->currentTextureView();
    ca.loadOp = rhi::LoadOp::Clear; ca.storeOp = rhi::StoreOp::Store;
    ca.clearValue = rhi::ClearColor(0.08f, 0.06f, 0.12f, 1.0f);
    rhi::RenderPassDesc rpd{}; rpd.colorAttachments.push_back(ca);
    auto* rp = enc->beginRenderPass(rpd);

    rp->setPipeline(m_pipeline);
    rp->setBindGroup(0, m_bindlessBg);
    rp->setBindGroup(1, m_samplerBg);
    rp->setViewport(0, 0, static_cast<f32>(m_width), static_cast<f32>(m_height), 0.0f, 1.0f);
    rp->setScissor(0, 0, m_width, m_height);

    // Draw 4 quads, each with a different texture index via push constants
    // Layout: 2x2 grid
    float offsets[8] = { -0.45f, 0.45f, 0.45f, 0.45f, -0.45f, -0.45f, 0.45f, -0.45f };

    for (u32 i = 0; i < kNumTextures; ++i) {
        u32 pushData[4] = { i, 0, 0, 0 };
        std::memcpy(&pushData[1], &offsets[i * 2],     4);
        std::memcpy(&pushData[2], &offsets[i * 2 + 1], 4);
        rp->setPushConstants(rhi::ShaderStage::Vertex | rhi::ShaderStage::Fragment, 0, 16, pushData);
        rp->draw(4);
    }

    rp->end();
    enc->transitionTexture(m_swapChain->currentTexture(), rhi::ResourceState::RenderTarget, rhi::ResourceState::Present);
    rhi::CommandBuffer* cb = enc->finish(); m_fenceVal++;
    rhi::CommandBuffer* cbs[1] = { cb };
    m_graphicsQueue->submit(std::span<rhi::CommandBuffer* const>(cbs, 1), m_fence, m_fenceVal);
    m_swapChain->present(m_graphicsQueue);
    m_pool->destroyEncoder(enc);
}

void BindlessSample::onShutdown() {
    if (m_fence) m_device->destroyFence(m_fence);
    if (m_pool) m_device->destroyCommandPool(m_pool);
    if (m_pipeline) m_device->destroyRenderPipeline(m_pipeline);
    if (m_pl) m_device->destroyPipelineLayout(m_pl);
    if (m_samplerBg) m_device->destroyBindGroup(m_samplerBg);
    if (m_samplerBgl) m_device->destroyBindGroupLayout(m_samplerBgl);
    if (m_bindlessBg) m_device->destroyBindGroup(m_bindlessBg);
    if (m_bindlessBgl) m_device->destroyBindGroupLayout(m_bindlessBgl);
    if (m_sampler) m_device->destroySampler(m_sampler);
    for (int i = kNumTextures - 1; i >= 0; --i) {
        if (m_textureViews[i]) m_device->destroyTextureView(m_textureViews[i]);
        if (m_textures[i]) m_device->destroyTexture(m_textures[i]);
    }
    if (m_ps) m_device->destroyShaderModule(m_ps);
    if (m_vs) m_device->destroyShaderModule(m_vs);
    if (m_compiler) { m_compiler->destroy(); delete m_compiler; }
}

int main(int argc, char** argv) { BindlessSample app; return app.run(argc, argv); }
