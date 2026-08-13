#include <new>
/// Demonstrates cube map textures and comparison samplers.
/// Renders a fullscreen quad that samples a procedural cube map (skybox),
/// plus a second pass with a depth texture sampled via comparison sampler
/// to demonstrate shadow-map-style sampling.

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

class CubeMapSample : public sf::SampleApp {
public:
    using sf::SampleApp::SampleApp;
    std::u8string_view title() const override { return u8"Sample023 - Cube Map & Comparison Sampler"; }
protected:
    draco::Status onInit() override;
    void onRender() override;
    void onShutdown() override;
private:
    draco::Status createCubeMap();
    draco::Status createDepthTexture();

    // Skybox shader: fullscreen quad -> ray direction -> cube map lookup
    static constexpr const char8_t kSkyboxShader[] = u8R"(
        TextureCube<float4> gCubeMap : register(t0, space0);
        SamplerState gSampler : register(s0, space0);

        struct PushConstants
        {
            float Time;
            float AspectRatio;
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
            // Fullscreen triangle
            float2 uv = float2((vertexID << 1) & 2, vertexID & 2);
            output.Position = float4(uv * 2.0 - 1.0, 0.5, 1.0);
            output.UV = uv;
            return output;
        }

        float4 PSMain(PSInput input) : SV_TARGET
        {
            // Convert UV to ray direction
            float2 ndc = input.UV * 2.0 - 1.0;
            ndc.x *= pc.AspectRatio;
            ndc.y = -ndc.y;

            // Simple rotation around Y
            float c = cos(pc.Time * 0.3);
            float s = sin(pc.Time * 0.3);

            float3 dir = normalize(float3(ndc.x, ndc.y, 1.0));
            float3 rotDir = float3(dir.x * c + dir.z * s, dir.y, -dir.x * s + dir.z * c);

            return gCubeMap.Sample(gSampler, rotDir);
        }
    )";

    // Shadow test shader: renders a quad, samples a depth texture with comparison sampler
    static constexpr const char8_t kShadowShader[] = u8R"(
        Texture2D<float> gShadowMap : register(t0, space0);
        SamplerComparisonState gShadowSampler : register(s0, space0);

        struct PushConstants
        {
            float Time;
            float AspectRatio;
            float2 _pad;
        };

        [[vk::push_constant]] ConstantBuffer<PushConstants> pc : register(b0, space1);

        struct VSInput
        {
            float3 Position : TEXCOORD0;
            float2 TexCoord : TEXCOORD1;
        };

        struct PSInput
        {
            float4 Position : SV_POSITION;
            float2 TexCoord : TEXCOORD0;
        };

        PSInput VSMain(VSInput input)
        {
            PSInput output;
            output.Position = float4(input.Position, 1.0);
            output.TexCoord = input.TexCoord;
            return output;
        }

        float4 PSMain(PSInput input) : SV_TARGET
        {
            // Compare at varying depth based on time for animated shadow boundary
            float compareValue = 0.5 + 0.4 * sin(pc.Time);
            float shadow = gShadowMap.SampleCmpLevelZero(gShadowSampler, input.TexCoord, compareValue);
            float3 litColor = float3(0.9, 0.85, 0.7);
            float3 shadowColor = float3(0.1, 0.1, 0.2);
            float3 color = lerp(shadowColor, litColor, shadow);
            return float4(color, 1.0);
        }
    )";

    struct PushData {
        float time;
        float aspectRatio;
        float _pad0;
        float _pad1;
    };

    shaders::Compiler* m_compiler = nullptr;

    // Skybox resources
    rhi::ShaderModule* m_skyboxVs = nullptr;
    rhi::ShaderModule* m_skyboxPs = nullptr;
    rhi::Texture*      m_cubeTexture = nullptr;
    rhi::TextureView*  m_cubeView = nullptr;
    rhi::Sampler*      m_linearSampler = nullptr;
    rhi::BindGroupLayout* m_skyboxBgl = nullptr;
    rhi::BindGroup*       m_skyboxBg = nullptr;
    rhi::PipelineLayout*  m_skyboxPl = nullptr;
    rhi::RenderPipeline*  m_skyboxPipeline = nullptr;

    // Shadow comparison resources
    rhi::ShaderModule* m_shadowVs = nullptr;
    rhi::ShaderModule* m_shadowPs = nullptr;
    rhi::Texture*      m_depthTexture = nullptr;
    rhi::TextureView*  m_depthView = nullptr;
    rhi::Sampler*      m_comparisonSampler = nullptr;
    rhi::Buffer*       m_quadVb = nullptr;
    rhi::BindGroupLayout* m_shadowBgl = nullptr;
    rhi::BindGroup*       m_shadowBg = nullptr;
    rhi::PipelineLayout*  m_shadowPl = nullptr;
    rhi::RenderPipeline*  m_shadowPipeline = nullptr;

    rhi::CommandPool* m_pool = nullptr;
    rhi::Fence*       m_fence = nullptr;
    draco::u64       m_fenceVal = 0;
};

draco::Status CubeMapSample::onInit() {
    using draco::Status, std::span, draco::u8, draco::u32;

    if (shaders::createCompiler(shaders::CompilerDesc{}, m_compiler) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Compile skybox shaders
    if (sf::compileToModule(m_compiler, m_device, kSkyboxShader, shaders::ShaderStage::Vertex,   u8"VSMain", u8"SkyboxVS", m_skyboxVs) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (sf::compileToModule(m_compiler, m_device, kSkyboxShader, shaders::ShaderStage::Fragment, u8"PSMain", u8"SkyboxPS", m_skyboxPs) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Compile shadow shaders
    if (sf::compileToModule(m_compiler, m_device, kShadowShader, shaders::ShaderStage::Vertex,   u8"VSMain", u8"ShadowVS", m_shadowVs) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (sf::compileToModule(m_compiler, m_device, kShadowShader, shaders::ShaderStage::Fragment, u8"PSMain", u8"ShadowPS", m_shadowPs) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Create procedural cube map (6 faces, 64x64, each a solid color)
    if (createCubeMap() != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Create depth texture for comparison sampler (gradient)
    if (createDepthTexture() != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Create samplers
    {
        rhi::SamplerDesc sd{};
        sd.minFilter = rhi::FilterMode::Linear;
        sd.magFilter = rhi::FilterMode::Linear;
        sd.label = u8"LinearSampler";
        if (m_device->createSampler(sd, m_linearSampler) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    }
    {
        rhi::SamplerDesc sd{};
        sd.minFilter = rhi::FilterMode::Linear;
        sd.magFilter = rhi::FilterMode::Linear;
        sd.compare = rhi::CompareFunction::LessEqual;
        sd.label = u8"ComparisonSampler";
        if (m_device->createSampler(sd, m_comparisonSampler) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    }

    // Skybox bind group layout: cube texture + sampler
    {
        rhi::BindGroupLayoutEntry entries[2] = {
            rhi::BindGroupLayoutEntry::sampledTexture(0, rhi::ShaderStage::Vertex | rhi::ShaderStage::Fragment, rhi::TextureViewDimension::TextureCube),
            rhi::BindGroupLayoutEntry::sampler(0, rhi::ShaderStage::Fragment)
        };
        rhi::BindGroupLayoutDesc bgld{};
        bgld.entries = std::span<const rhi::BindGroupLayoutEntry>(entries, 2);
        bgld.label = u8"SkyboxBGL";
        if (m_device->createBindGroupLayout(bgld, m_skyboxBgl) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    }

    // Skybox bind group
    {
        rhi::BindGroupEntry entries[2] = {
            rhi::BindGroupEntry::textureEntry(m_cubeView),
            rhi::BindGroupEntry::samplerEntry(m_linearSampler)
        };
        rhi::BindGroupDesc bgd{};
        bgd.layout = m_skyboxBgl;
        bgd.entries = std::span<const rhi::BindGroupEntry>(entries, 2);
        bgd.label = u8"SkyboxBG";
        if (m_device->createBindGroup(bgd, m_skyboxBg) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    }

    // Skybox pipeline layout
    {
        rhi::BindGroupLayout* sets[1] = { m_skyboxBgl };
        rhi::PushConstantRange pcr{};
        pcr.stages = rhi::ShaderStage::Vertex | rhi::ShaderStage::Fragment;
        pcr.offset = 0;
        pcr.size = sizeof(PushData);
        rhi::PushConstantRange pushRanges[1] = { pcr };
        rhi::PipelineLayoutDesc pld{};
        pld.bindGroupLayouts = std::span<rhi::BindGroupLayout* const>(sets, 1);
        pld.pushConstantRanges = std::span<const rhi::PushConstantRange>(pushRanges, 1);
        pld.label = u8"SkyboxPL";
        if (m_device->createPipelineLayout(pld, m_skyboxPl) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    }

    // Skybox pipeline (fullscreen triangle, no vertex input)
    {
        rhi::ColorTargetState ct{};
        ct.format = m_swapChain->format();
        rhi::RenderPipelineDesc rpd{};
        rpd.layout = m_skyboxPl;
        rpd.vertex.shader = { m_skyboxVs, u8"VSMain", rhi::ShaderStage::Vertex };
        rpd.fragment = rhi::FragmentState{};
        rpd.fragment->shader = { m_skyboxPs, u8"PSMain", rhi::ShaderStage::Fragment };
        rpd.fragment->targets = std::span<const rhi::ColorTargetState>(&ct, 1);
        rpd.primitive.topology = rhi::PrimitiveTopology::TriangleList;
        rpd.label = u8"SkyboxPipeline";
        if (m_device->createRenderPipeline(rpd, m_skyboxPipeline) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    }

    // Shadow test quad vertices (bottom-right corner overlay)
    {
        float quadVerts[] = {
            // pos xyz, uv
             0.3f, -0.9f, 0.0f,   0.0f, 1.0f,
             0.9f, -0.9f, 0.0f,   1.0f, 1.0f,
             0.9f, -0.3f, 0.0f,   1.0f, 0.0f,
             0.3f, -0.9f, 0.0f,   0.0f, 1.0f,
             0.9f, -0.3f, 0.0f,   1.0f, 0.0f,
             0.3f, -0.3f, 0.0f,   0.0f, 0.0f
        };

        u32 vbSize = sizeof(quadVerts);
        rhi::BufferDesc bd{};
        bd.size = vbSize;
        bd.usage = rhi::BufferUsage::Vertex | rhi::BufferUsage::CopyDst;
        bd.memory = rhi::MemoryLocation::GpuOnly;
        bd.label = u8"ShadowQuadVB";
        if (m_device->createBuffer(bd, m_quadVb) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

        rhi::TransferBatch* batch = nullptr;
        m_graphicsQueue->createTransferBatch(batch);
        batch->writeBuffer(m_quadVb, 0, std::span<const u8>(reinterpret_cast<const u8*>(quadVerts), vbSize));
        batch->submit();
        m_graphicsQueue->destroyTransferBatch(batch);
    }

    // Shadow bind group layout: depth texture + comparison sampler
    {
        rhi::BindGroupLayoutEntry entries[2];
        entries[0] = rhi::BindGroupLayoutEntry::sampledTexture(0, rhi::ShaderStage::Fragment, rhi::TextureViewDimension::Texture2D);
        entries[1] = {};
        entries[1].binding = 0;
        entries[1].visibility = rhi::ShaderStage::Fragment;
        entries[1].type = rhi::BindingType::ComparisonSampler;
        rhi::BindGroupLayoutDesc bgld{};
        bgld.entries = std::span<const rhi::BindGroupLayoutEntry>(entries, 2);
        bgld.label = u8"ShadowBGL";
        if (m_device->createBindGroupLayout(bgld, m_shadowBgl) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    }

    // Shadow bind group
    {
        rhi::BindGroupEntry entries[2] = {
            rhi::BindGroupEntry::textureEntry(m_depthView),
            rhi::BindGroupEntry::samplerEntry(m_comparisonSampler)
        };
        rhi::BindGroupDesc bgd{};
        bgd.layout = m_shadowBgl;
        bgd.entries = std::span<const rhi::BindGroupEntry>(entries, 2);
        bgd.label = u8"ShadowBG";
        if (m_device->createBindGroup(bgd, m_shadowBg) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    }

    // Shadow pipeline layout
    {
        rhi::BindGroupLayout* sets[1] = { m_shadowBgl };
        rhi::PushConstantRange pcr{};
        pcr.stages = rhi::ShaderStage::Vertex | rhi::ShaderStage::Fragment;
        pcr.offset = 0;
        pcr.size = sizeof(PushData);
        rhi::PushConstantRange pushRanges[1] = { pcr };
        rhi::PipelineLayoutDesc pld{};
        pld.bindGroupLayouts = std::span<rhi::BindGroupLayout* const>(sets, 1);
        pld.pushConstantRanges = std::span<const rhi::PushConstantRange>(pushRanges, 1);
        pld.label = u8"ShadowPL";
        if (m_device->createPipelineLayout(pld, m_shadowPl) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    }

    // Shadow pipeline
    {
        rhi::VertexAttribute attrs[2] = {
            { rhi::VertexFormat::Float32x3, 0, 0 },
            { rhi::VertexFormat::Float32x2, 12, 1 }
        };
        rhi::VertexBufferLayout vbl{};
        vbl.stride = 20;
        vbl.attributes = std::span<const rhi::VertexAttribute>(attrs, 2);

        rhi::ColorTargetState ct{};
        ct.format = m_swapChain->format();

        rhi::RenderPipelineDesc rpd{};
        rpd.layout = m_shadowPl;
        rpd.vertex.shader = { m_shadowVs, u8"VSMain", rhi::ShaderStage::Vertex };
        rpd.vertex.buffers = std::span<const rhi::VertexBufferLayout>(&vbl, 1);
        rpd.fragment = rhi::FragmentState{};
        rpd.fragment->shader = { m_shadowPs, u8"PSMain", rhi::ShaderStage::Fragment };
        rpd.fragment->targets = std::span<const rhi::ColorTargetState>(&ct, 1);
        rpd.primitive.topology = rhi::PrimitiveTopology::TriangleList;
        rpd.label = u8"ShadowPipeline";
        if (m_device->createRenderPipeline(rpd, m_shadowPipeline) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    }

    if (m_device->createCommandPool(rhi::QueueType::Graphics, m_pool) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (m_device->createFence(0, m_fence) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    return draco::ErrorCode::Ok;
}

draco::Status CubeMapSample::createCubeMap() {
    using draco::Status, std::span, draco::u8, draco::u32;

    constexpr u32 faceSize = 64;
    constexpr u32 BytesPerPixel = 4;
    constexpr u32 faceBytes = faceSize * faceSize * BytesPerPixel;

    // Create cube map texture: 2D with 6 array layers
    rhi::TextureDesc td{};
    td.dimension = rhi::TextureDimension::Texture2D;
    td.format = rhi::TextureFormat::RGBA8UnormSrgb;
    td.width = faceSize;
    td.height = faceSize;
    td.arrayLayerCount = 6;
    td.mipLevelCount = 1;
    td.sampleCount = 1;
    td.usage = rhi::TextureUsage::Sampled | rhi::TextureUsage::CopyDst;
    td.label = u8"CubeMapTex";
    if (m_device->createTexture(td, m_cubeTexture) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Create cube view
    rhi::TextureViewDesc tvd{};
    tvd.format = rhi::TextureFormat::RGBA8UnormSrgb;
    tvd.dimension = rhi::TextureViewDimension::TextureCube;
    tvd.baseMipLevel = 0;
    tvd.mipLevelCount = 1;
    tvd.baseArrayLayer = 0;
    tvd.arrayLayerCount = 6;
    if (m_device->createTextureView(m_cubeTexture, tvd, m_cubeView) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Generate 6 face colors: +X red, -X cyan, +Y green, -Y magenta, +Z blue, -Z yellow
    u8 faceColors[6][4] = {
        {200, 60, 60, 255},    // +X: red
        {60, 200, 200, 255},   // -X: cyan
        {60, 200, 60, 255},    // +Y: green
        {200, 60, 200, 255},   // -Y: magenta
        {60, 60, 200, 255},    // +Z: blue
        {200, 200, 60, 255}    // -Z: yellow
    };

    u8 stagingBuf[faceBytes];
    rhi::TransferBatch* batch = nullptr;
    m_graphicsQueue->createTransferBatch(batch);

    for (int face = 0; face < 6; face++) {
        // Fill face with gradient from face color to white at center
        for (u32 y = 0; y < faceSize; y++) {
            for (u32 x = 0; x < faceSize; x++) {
                float fx = (static_cast<float>(x) / static_cast<float>(faceSize)) * 2.0f - 1.0f;
                float fy = (static_cast<float>(y) / static_cast<float>(faceSize)) * 2.0f - 1.0f;
                float dist = std::min(1.0f, std::sqrt(fx * fx + fy * fy));
                float t = 1.0f - dist * 0.5f;

                u32 idx = (y * faceSize + x) * BytesPerPixel;
                stagingBuf[idx + 0] = static_cast<u8>(faceColors[face][0] * t + 40 * (1.0f - t));
                stagingBuf[idx + 1] = static_cast<u8>(faceColors[face][1] * t + 40 * (1.0f - t));
                stagingBuf[idx + 2] = static_cast<u8>(faceColors[face][2] * t + 40 * (1.0f - t));
                stagingBuf[idx + 3] = 255;
            }
        }

        rhi::TextureDataLayout layout{};
        layout.bytesPerRow = faceSize * BytesPerPixel;
        layout.rowsPerImage = faceSize;
        batch->writeTexture(m_cubeTexture,
            std::span<const u8>(stagingBuf, faceBytes),
            layout, rhi::Extent3D{faceSize, faceSize, 1},
            0, static_cast<u32>(face));
    }

    batch->submit();
    m_graphicsQueue->destroyTransferBatch(batch);
    return draco::ErrorCode::Ok;
}

draco::Status CubeMapSample::createDepthTexture() {
    using draco::Status;

    constexpr draco::u32 texSize = 64;

    rhi::TextureDesc td{};
    td.dimension = rhi::TextureDimension::Texture2D;
    td.format = rhi::TextureFormat::Depth32Float;
    td.width = texSize;
    td.height = texSize;
    td.arrayLayerCount = 1;
    td.mipLevelCount = 1;
    td.sampleCount = 1;
    td.usage = rhi::TextureUsage::DepthStencil | rhi::TextureUsage::Sampled;
    td.label = u8"ShadowDepthTex";
    if (m_device->createTexture(td, m_depthTexture) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    rhi::TextureViewDesc tvd{};
    tvd.format = rhi::TextureFormat::Depth32Float;
    tvd.dimension = rhi::TextureViewDimension::Texture2D;
    if (m_device->createTextureView(m_depthTexture, tvd, m_depthView) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // We'll render a gradient depth in a render pass
    // For simplicity, just clear to 0.5 so the comparison sampler has something to compare against
    // (A real sample would render shadow casters here)

    return draco::ErrorCode::Ok;
}

void CubeMapSample::onRender() {
    using draco::f32, draco::u32, std::span;

    if (m_fenceVal > 0) m_fence->wait(m_fenceVal, ~0ull);
    if (m_swapChain->acquireNextImage() != draco::ErrorCode::Ok) return;

    m_pool->reset();
    rhi::CommandEncoder* enc = nullptr;
    if (m_pool->createEncoder(enc) != draco::ErrorCode::Ok || !enc) return;

    f32 aspect = static_cast<f32>(m_width) / static_cast<f32>(m_height);

    // Render a depth value into the shadow depth texture
    enc->transitionTexture(m_depthTexture, rhi::ResourceState::Undefined, rhi::ResourceState::DepthStencilWrite);
    {
        rhi::DepthStencilAttachment dsa{};
        dsa.view = m_depthView;
        dsa.depthLoadOp = rhi::LoadOp::Clear;
        dsa.depthStoreOp = rhi::StoreOp::Store;
        dsa.depthClearValue = 0.5f;
        rhi::RenderPassDesc rpd{};
        rpd.depthStencilAttachment = dsa;
        auto* rp = enc->beginRenderPass(rpd);
        rp->end();
    }

    // Transition depth texture from DepthStencilWrite -> ShaderRead for sampling
    enc->transitionTexture(m_depthTexture, rhi::ResourceState::DepthStencilWrite, rhi::ResourceState::ShaderRead);

    // Transition swapchain
    enc->transitionTexture(m_swapChain->currentTexture(), rhi::ResourceState::Undefined, rhi::ResourceState::RenderTarget);

    rhi::ColorAttachment ca{};
    ca.view = m_swapChain->currentTextureView();
    ca.loadOp = rhi::LoadOp::Clear;
    ca.storeOp = rhi::StoreOp::Store;
    ca.clearValue = rhi::ClearColor(0.05f, 0.05f, 0.08f, 1.0f);
    rhi::RenderPassDesc rpd{};
    rpd.colorAttachments.push_back(ca);
    auto* rp = enc->beginRenderPass(rpd);

    rp->setViewport(0, 0, static_cast<f32>(m_width), static_cast<f32>(m_height), 0.0f, 1.0f);
    rp->setScissor(0, 0, m_width, m_height);

    // Draw skybox (fullscreen triangle, no VB needed - SV_VertexID)
    rp->setPipeline(m_skyboxPipeline);
    rp->setBindGroup(0, m_skyboxBg);
    PushData pc{};
    pc.time = m_totalTime;
    pc.aspectRatio = aspect;
    rp->setPushConstants(rhi::ShaderStage::Vertex | rhi::ShaderStage::Fragment, 0, sizeof(PushData), &pc);
    rp->draw(3);

    // Draw shadow comparison overlay quad
    rp->setPipeline(m_shadowPipeline);
    rp->setBindGroup(0, m_shadowBg);
    rp->setPushConstants(rhi::ShaderStage::Vertex | rhi::ShaderStage::Fragment, 0, sizeof(PushData), &pc);
    rp->setVertexBuffer(0, m_quadVb, 0);
    rp->draw(6);

    rp->end();

    enc->transitionTexture(m_swapChain->currentTexture(), rhi::ResourceState::RenderTarget, rhi::ResourceState::Present);

    // Transition depth texture back to DepthStencilWrite for next frame
    enc->transitionTexture(m_depthTexture, rhi::ResourceState::ShaderRead, rhi::ResourceState::DepthStencilWrite);

    rhi::CommandBuffer* cb = enc->finish();
    m_fenceVal++;
    rhi::CommandBuffer* cbs[1] = { cb };
    m_graphicsQueue->submit(std::span<rhi::CommandBuffer* const>(cbs, 1), m_fence, m_fenceVal);
    m_swapChain->present(m_graphicsQueue);
    m_pool->destroyEncoder(enc);
}

void CubeMapSample::onShutdown() {
    if (m_fence) m_device->destroyFence(m_fence);
    if (m_pool) m_device->destroyCommandPool(m_pool);
    if (m_shadowPipeline) m_device->destroyRenderPipeline(m_shadowPipeline);
    if (m_skyboxPipeline) m_device->destroyRenderPipeline(m_skyboxPipeline);
    if (m_shadowPl) m_device->destroyPipelineLayout(m_shadowPl);
    if (m_skyboxPl) m_device->destroyPipelineLayout(m_skyboxPl);
    if (m_shadowBg) m_device->destroyBindGroup(m_shadowBg);
    if (m_skyboxBg) m_device->destroyBindGroup(m_skyboxBg);
    if (m_shadowBgl) m_device->destroyBindGroupLayout(m_shadowBgl);
    if (m_skyboxBgl) m_device->destroyBindGroupLayout(m_skyboxBgl);
    if (m_comparisonSampler) m_device->destroySampler(m_comparisonSampler);
    if (m_linearSampler) m_device->destroySampler(m_linearSampler);
    if (m_quadVb) m_device->destroyBuffer(m_quadVb);
    if (m_depthView) m_device->destroyTextureView(m_depthView);
    if (m_depthTexture) m_device->destroyTexture(m_depthTexture);
    if (m_cubeView) m_device->destroyTextureView(m_cubeView);
    if (m_cubeTexture) m_device->destroyTexture(m_cubeTexture);
    if (m_shadowPs) m_device->destroyShaderModule(m_shadowPs);
    if (m_shadowVs) m_device->destroyShaderModule(m_shadowVs);
    if (m_skyboxPs) m_device->destroyShaderModule(m_skyboxPs);
    if (m_skyboxVs) m_device->destroyShaderModule(m_skyboxVs);
    if (m_compiler) { m_compiler->destroy(); delete m_compiler; }
}

int main(int argc, char** argv) { CubeMapSample app; return app.run(argc, argv); }
