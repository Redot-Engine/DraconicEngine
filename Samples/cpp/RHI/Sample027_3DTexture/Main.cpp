#include <new>
/// Demonstrates 3D textures and 1D textures.
/// Generates a 3D noise volume, renders slices animated over time.
/// Uses a 1D gradient LUT for color mapping.

#include <algorithm>
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

class Texture3DSample : public sf::SampleApp {
public:
    using sf::SampleApp::SampleApp;
    std::u8string_view title() const override { return u8"Sample027 - 3D Texture & 1D LUT"; }
protected:
    draco::Status onInit() override;
    void onRender() override;
    void onShutdown() override;
private:
    draco::Status createVolumeTexture();
    draco::Status createLUTTexture();

    static constexpr const char8_t kShader[] = u8R"(
        Texture3D<float4> gVolume : register(t0, space0);
        Texture1D<float4> gLUT    : register(t1, space0);
        SamplerState gSampler     : register(s0, space0);

        struct PushConstants
        {
            float SliceZ;
            float Time;
            float2 _pad;
        };

        [[vk::push_constant]] ConstantBuffer<PushConstants> pc : register(b0, space1);

        struct PSInput
        {
            float4 Position : SV_POSITION;
            float2 UV       : TEXCOORD0;
        };

        PSInput VSMain(uint vertexID : SV_VertexID)
        {
            PSInput output;
            float2 uv = float2((vertexID << 1) & 2, vertexID & 2);
            output.Position = float4(uv * 2.0 - 1.0, 0.5, 1.0);
            output.UV = uv;
            return output;
        }

        float4 PSMain(PSInput input) : SV_TARGET
        {
            // Sample 3D volume at current slice
            float3 uvw = float3(input.UV, pc.SliceZ);
            float density = gVolume.Sample(gSampler, uvw).r;

            // Map density through 1D LUT
            float4 color = gLUT.Sample(gSampler, density);
            return color;
        }
    )";

    struct PushData {
        float sliceZ;
        float time;
        float _pad0;
        float _pad1;
    };

    static constexpr draco::u32 kVolumeSize = 32;
    static constexpr draco::u32 kLUTSize = 64;

    shaders::Compiler* m_compiler = nullptr;
    rhi::ShaderModule* m_vs = nullptr;
    rhi::ShaderModule* m_ps = nullptr;

    // 3D volume texture
    rhi::Texture*     m_volumeTexture = nullptr;
    rhi::TextureView* m_volumeView    = nullptr;

    // 1D LUT texture
    rhi::Texture*     m_lutTexture = nullptr;
    rhi::TextureView* m_lutView    = nullptr;

    rhi::Sampler*         m_sampler  = nullptr;
    rhi::BindGroupLayout* m_bgl     = nullptr;
    rhi::BindGroup*       m_bg      = nullptr;
    rhi::PipelineLayout*  m_pl      = nullptr;
    rhi::RenderPipeline*  m_pipeline = nullptr;

    rhi::CommandPool* m_pool     = nullptr;
    rhi::Fence*       m_fence    = nullptr;
    draco::u64       m_fenceVal = 0;
};

draco::Status Texture3DSample::onInit() {
    using draco::Status, std::span, draco::u8, draco::u32;

    if (shaders::createCompiler(shaders::CompilerDesc{}, m_compiler) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (sf::compileToModule(m_compiler, m_device, kShader, shaders::ShaderStage::Vertex,   u8"VSMain", u8"Vol3DVS", m_vs) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (sf::compileToModule(m_compiler, m_device, kShader, shaders::ShaderStage::Fragment, u8"PSMain", u8"Vol3DPS", m_ps) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    if (createVolumeTexture() != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (createLUTTexture() != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Sampler
    {
        rhi::SamplerDesc sd{};
        sd.minFilter = rhi::FilterMode::Linear;
        sd.magFilter = rhi::FilterMode::Linear;
        sd.addressU = rhi::AddressMode::Repeat;
        sd.addressV = rhi::AddressMode::Repeat;
        sd.addressW = rhi::AddressMode::Repeat;
        sd.label = u8"VolSampler";
        if (m_device->createSampler(sd, m_sampler) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    }

    // Bind group layout: 3D tex, 1D tex, sampler
    {
        rhi::BindGroupLayoutEntry entries[3] = {
            rhi::BindGroupLayoutEntry::sampledTexture(0, rhi::ShaderStage::Fragment, rhi::TextureViewDimension::Texture3D),
            rhi::BindGroupLayoutEntry::sampledTexture(1, rhi::ShaderStage::Fragment, rhi::TextureViewDimension::Texture1D),
            rhi::BindGroupLayoutEntry::sampler(0, rhi::ShaderStage::Fragment)
        };
        rhi::BindGroupLayoutDesc bgld{};
        bgld.entries = std::span<const rhi::BindGroupLayoutEntry>(entries, 3);
        bgld.label = u8"VolBGL";
        if (m_device->createBindGroupLayout(bgld, m_bgl) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    }

    // Bind group
    {
        rhi::BindGroupEntry entries[3] = {
            rhi::BindGroupEntry::textureEntry(m_volumeView),
            rhi::BindGroupEntry::textureEntry(m_lutView),
            rhi::BindGroupEntry::samplerEntry(m_sampler)
        };
        rhi::BindGroupDesc bgd{};
        bgd.layout = m_bgl;
        bgd.entries = std::span<const rhi::BindGroupEntry>(entries, 3);
        bgd.label = u8"VolBG";
        if (m_device->createBindGroup(bgd, m_bg) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    }

    // Pipeline layout with push constants
    {
        rhi::BindGroupLayout* sets[1] = { m_bgl };
        rhi::PushConstantRange pcr{};
        pcr.stages = rhi::ShaderStage::Fragment;
        pcr.offset = 0;
        pcr.size = sizeof(PushData);
        rhi::PushConstantRange pushRanges[1] = { pcr };
        rhi::PipelineLayoutDesc pld{};
        pld.bindGroupLayouts = std::span<rhi::BindGroupLayout* const>(sets, 1);
        pld.pushConstantRanges = std::span<const rhi::PushConstantRange>(pushRanges, 1);
        pld.label = u8"VolPL";
        if (m_device->createPipelineLayout(pld, m_pl) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    }

    // Render pipeline (fullscreen triangle, no vertex input)
    {
        rhi::ColorTargetState ct{};
        ct.format = m_swapChain->format();
        rhi::RenderPipelineDesc rpd{};
        rpd.layout = m_pl;
        rpd.vertex.shader = { m_vs, u8"VSMain", rhi::ShaderStage::Vertex };
        rpd.fragment = rhi::FragmentState{};
        rpd.fragment->shader = { m_ps, u8"PSMain", rhi::ShaderStage::Fragment };
        rpd.fragment->targets = std::span<const rhi::ColorTargetState>(&ct, 1);
        rpd.primitive.topology = rhi::PrimitiveTopology::TriangleList;
        rpd.label = u8"VolPipeline";
        if (m_device->createRenderPipeline(rpd, m_pipeline) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    }

    if (m_device->createCommandPool(rhi::QueueType::Graphics, m_pool) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (m_device->createFence(0, m_fence) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    return draco::ErrorCode::Ok;
}

draco::Status Texture3DSample::createVolumeTexture() {
    using draco::Status, std::span, draco::u8, draco::u32;

    rhi::TextureDesc td{};
    td.dimension = rhi::TextureDimension::Texture3D;
    td.format = rhi::TextureFormat::R8Unorm;
    td.width = kVolumeSize;
    td.height = kVolumeSize;
    td.depth = kVolumeSize;
    td.mipLevelCount = 1;
    td.sampleCount = 1;
    td.usage = rhi::TextureUsage::Sampled | rhi::TextureUsage::CopyDst;
    td.label = u8"VolumeTex3D";
    if (m_device->createTexture(td, m_volumeTexture) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    rhi::TextureViewDesc tvd{};
    tvd.format = rhi::TextureFormat::R8Unorm;
    tvd.dimension = rhi::TextureViewDimension::Texture3D;
    if (m_device->createTextureView(m_volumeTexture, tvd, m_volumeView) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Generate procedural 3D noise data
    constexpr u32 dataSize = kVolumeSize * kVolumeSize * kVolumeSize;
    u8 data[dataSize];

    for (u32 z = 0; z < kVolumeSize; z++) {
        for (u32 y = 0; y < kVolumeSize; y++) {
            for (u32 x = 0; x < kVolumeSize; x++) {
                float fx = static_cast<float>(x) / static_cast<float>(kVolumeSize);
                float fy = static_cast<float>(y) / static_cast<float>(kVolumeSize);
                float fz = static_cast<float>(z) / static_cast<float>(kVolumeSize);

                // Simple 3D pattern: spherical blobs + frequency pattern
                float cx = fx - 0.5f, cy = fy - 0.5f, cz = fz - 0.5f;
                float dist = std::sqrt(cx * cx + cy * cy + cz * cz);
                float sphere = std::max(0.0f, 1.0f - dist * 3.0f);
                float pattern = std::sin(fx * 12.0f) * std::sin(fy * 12.0f) * std::sin(fz * 12.0f);
                float v = std::clamp(sphere + pattern * 0.3f, 0.0f, 1.0f);

                u32 idx = z * kVolumeSize * kVolumeSize + y * kVolumeSize + x;
                data[idx] = static_cast<u8>(v * 255.0f);
            }
        }
    }

    rhi::TransferBatch* batch = nullptr;
    m_graphicsQueue->createTransferBatch(batch);
    rhi::TextureDataLayout layout{};
    layout.bytesPerRow = kVolumeSize;
    layout.rowsPerImage = kVolumeSize;
    batch->writeTexture(m_volumeTexture,
        std::span<const u8>(data, dataSize),
        layout, rhi::Extent3D{kVolumeSize, kVolumeSize, kVolumeSize});
    batch->submit();
    m_graphicsQueue->destroyTransferBatch(batch);

    return draco::ErrorCode::Ok;
}

draco::Status Texture3DSample::createLUTTexture() {
    using draco::Status, std::span, draco::u8, draco::u32;

    rhi::TextureDesc td{};
    td.dimension = rhi::TextureDimension::Texture1D;
    td.format = rhi::TextureFormat::RGBA8UnormSrgb;
    td.width = kLUTSize;
    td.height = 1;
    td.arrayLayerCount = 1;
    td.mipLevelCount = 1;
    td.sampleCount = 1;
    td.usage = rhi::TextureUsage::Sampled | rhi::TextureUsage::CopyDst;
    td.label = u8"LUTTex1D";
    if (m_device->createTexture(td, m_lutTexture) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    rhi::TextureViewDesc tvd{};
    tvd.format = rhi::TextureFormat::RGBA8UnormSrgb;
    tvd.dimension = rhi::TextureViewDimension::Texture1D;
    if (m_device->createTextureView(m_lutTexture, tvd, m_lutView) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Generate gradient LUT: dark blue -> cyan -> green -> yellow -> red -> white
    u8 data[kLUTSize * 4];
    for (u32 i = 0; i < kLUTSize; i++) {
        float t = static_cast<float>(i) / static_cast<float>(kLUTSize - 1);
        float r, g, b;
        if (t < 0.2f) {
            float s = t / 0.2f;
            r = 0.05f; g = 0.05f + s * 0.4f; b = 0.3f + s * 0.5f;
        } else if (t < 0.4f) {
            float s = (t - 0.2f) / 0.2f;
            r = 0.05f; g = 0.45f + s * 0.5f; b = 0.8f - s * 0.5f;
        } else if (t < 0.6f) {
            float s = (t - 0.4f) / 0.2f;
            r = s * 0.8f; g = 0.95f; b = 0.3f - s * 0.3f;
        } else if (t < 0.8f) {
            float s = (t - 0.6f) / 0.2f;
            r = 0.8f + s * 0.2f; g = 0.95f - s * 0.6f; b = 0.0f;
        } else {
            float s = (t - 0.8f) / 0.2f;
            r = 1.0f; g = 0.35f + s * 0.65f; b = s * 0.8f;
        }

        u32 idx = i * 4;
        data[idx + 0] = static_cast<u8>(r * 255.0f);
        data[idx + 1] = static_cast<u8>(g * 255.0f);
        data[idx + 2] = static_cast<u8>(b * 255.0f);
        data[idx + 3] = 255;
    }

    rhi::TransferBatch* batch = nullptr;
    m_graphicsQueue->createTransferBatch(batch);
    rhi::TextureDataLayout layout{};
    layout.bytesPerRow = kLUTSize * 4;
    layout.rowsPerImage = 1;
    batch->writeTexture(m_lutTexture,
        std::span<const u8>(data, kLUTSize * 4),
        layout, rhi::Extent3D{kLUTSize, 1, 1});
    batch->submit();
    m_graphicsQueue->destroyTransferBatch(batch);

    return draco::ErrorCode::Ok;
}

void Texture3DSample::onRender() {
    using draco::f32, std::span;

    if (m_fenceVal > 0) m_fence->wait(m_fenceVal, ~0ull);
    if (m_swapChain->acquireNextImage() != draco::ErrorCode::Ok) return;

    m_pool->reset();
    rhi::CommandEncoder* enc = nullptr;
    if (m_pool->createEncoder(enc) != draco::ErrorCode::Ok || !enc) return;

    enc->transitionTexture(m_swapChain->currentTexture(), rhi::ResourceState::Undefined, rhi::ResourceState::RenderTarget);

    rhi::ColorAttachment ca{};
    ca.view = m_swapChain->currentTextureView();
    ca.loadOp = rhi::LoadOp::Clear;
    ca.storeOp = rhi::StoreOp::Store;
    ca.clearValue = rhi::ClearColor(0.02f, 0.02f, 0.05f, 1.0f);
    rhi::RenderPassDesc rpd{};
    rpd.colorAttachments.push_back(ca);

    auto* rp = enc->beginRenderPass(rpd);

    rp->setPipeline(m_pipeline);
    rp->setBindGroup(0, m_bg);
    rp->setViewport(0, 0, static_cast<f32>(m_width), static_cast<f32>(m_height), 0.0f, 1.0f);
    rp->setScissor(0, 0, m_width, m_height);

    // Animate slice through 3D volume
    float sliceZ = 0.5f + 0.5f * std::sin(m_totalTime * 0.5f);
    PushData pc{};
    pc.sliceZ = sliceZ;
    pc.time = m_totalTime;
    rp->setPushConstants(rhi::ShaderStage::Fragment, 0, sizeof(PushData), &pc);

    rp->draw(3); // Fullscreen triangle

    rp->end();

    enc->transitionTexture(m_swapChain->currentTexture(), rhi::ResourceState::RenderTarget, rhi::ResourceState::Present);

    rhi::CommandBuffer* cb = enc->finish();
    m_fenceVal++;
    rhi::CommandBuffer* cbs[1] = { cb };
    m_graphicsQueue->submit(std::span<rhi::CommandBuffer* const>(cbs, 1), m_fence, m_fenceVal);
    m_swapChain->present(m_graphicsQueue);
    m_pool->destroyEncoder(enc);
}

void Texture3DSample::onShutdown() {
    if (m_fence) m_device->destroyFence(m_fence);
    if (m_pool) m_device->destroyCommandPool(m_pool);
    if (m_pipeline) m_device->destroyRenderPipeline(m_pipeline);
    if (m_pl) m_device->destroyPipelineLayout(m_pl);
    if (m_bg) m_device->destroyBindGroup(m_bg);
    if (m_bgl) m_device->destroyBindGroupLayout(m_bgl);
    if (m_sampler) m_device->destroySampler(m_sampler);
    if (m_lutView) m_device->destroyTextureView(m_lutView);
    if (m_lutTexture) m_device->destroyTexture(m_lutTexture);
    if (m_volumeView) m_device->destroyTextureView(m_volumeView);
    if (m_volumeTexture) m_device->destroyTexture(m_volumeTexture);
    if (m_ps) m_device->destroyShaderModule(m_ps);
    if (m_vs) m_device->destroyShaderModule(m_vs);
    if (m_compiler) { m_compiler->destroy(); delete m_compiler; }
}

int main(int argc, char** argv) { Texture3DSample app; return app.run(argc, argv); }
