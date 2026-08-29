#include <new>
/// Demonstrates batched GPU uploads using TransferBatch with async fence signaling.
/// Uploads a vertex buffer, index buffer, and a procedural texture in a single
/// batched transfer with submitAsync, then renders a textured quad once the
/// upload fence signals completion.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
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

class BatchUploadSample : public sf::SampleApp {
public:
    using sf::SampleApp::SampleApp;
    std::u8string_view title() const override { return u8"Sample019 - Batch Upload (Async Transfer)"; }
protected:
    draco::Status onInit() override;
    void onRender() override;
    void onShutdown() override;
private:
    draco::Status doBatchUpload();

    static constexpr const char8_t kShader[] = u8R"(
        Texture2D gTexture : register(t0, space0);
        SamplerState gSampler : register(s0, space0);

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

        cbuffer Transform : register(b0, space0)
        {
            float Time;
            float Pad0;
            float Pad1;
            float Pad2;
        };

        PSInput VSMain(VSInput input)
        {
            PSInput output;
            // Gentle rotation
            float c = cos(Time * 0.5);
            float s = sin(Time * 0.5);
            float3 p = input.Position;
            float x = p.x * c - p.y * s;
            float y = p.x * s + p.y * c;
            output.Position = float4(x, y, p.z, 1.0);
            output.TexCoord = input.TexCoord;
            return output;
        }

        float4 PSMain(PSInput input) : SV_TARGET
        {
            return gTexture.Sample(gSampler, input.TexCoord);
        }
    )";

    static constexpr draco::u32 kTexSize = 128;

    shaders::Compiler* m_compiler = nullptr;
    rhi::ShaderModule* m_vs = nullptr;
    rhi::ShaderModule* m_ps = nullptr;

    rhi::Buffer* m_vb = nullptr;
    rhi::Buffer* m_ib = nullptr;
    rhi::Texture* m_tex = nullptr;
    rhi::TextureView* m_texView = nullptr;
    rhi::Sampler* m_sampler = nullptr;

    rhi::Buffer* m_transformBuf = nullptr;
    void* m_transformMapped = nullptr;

    rhi::BindGroupLayout* m_bgl = nullptr;
    rhi::BindGroup* m_bg = nullptr;
    rhi::PipelineLayout* m_pl = nullptr;
    rhi::RenderPipeline* m_pipeline = nullptr;
    rhi::CommandPool* m_pool = nullptr;
    rhi::Fence* m_frameFence = nullptr;
    draco::u64 m_frameFenceVal = 0;

    // Upload tracking
    rhi::Fence* m_uploadFence = nullptr;
    draco::u64 m_uploadFenceVal = 0;
    bool m_uploadComplete = false;
    float m_uploadStartTime = 0.0f;
};

draco::Status BatchUploadSample::onInit() {
    using draco::Status, std::span, draco::u8, draco::u32;

    if (shaders::createCompiler(shaders::CompilerDesc{}, m_compiler) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (sf::compileToModule(m_compiler, m_device, kShader, shaders::ShaderStage::Vertex,   u8"VSMain", u8"BatchVS", m_vs) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (sf::compileToModule(m_compiler, m_device, kShader, shaders::ShaderStage::Fragment, u8"PSMain", u8"BatchPS", m_ps) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Vertex buffer: 4 vertices x (pos3 + uv2) x 4 = 80 bytes
    rhi::BufferDesc vbd{}; vbd.size = 80; vbd.usage = rhi::BufferUsage::Vertex | rhi::BufferUsage::CopyDst; vbd.memory = rhi::MemoryLocation::GpuOnly; vbd.label = u8"BatchVB";
    if (m_device->createBuffer(vbd, m_vb) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Index buffer: 6 uint16 = 12 bytes
    rhi::BufferDesc ibd{}; ibd.size = 12; ibd.usage = rhi::BufferUsage::Index | rhi::BufferUsage::CopyDst; ibd.memory = rhi::MemoryLocation::GpuOnly; ibd.label = u8"BatchIB";
    if (m_device->createBuffer(ibd, m_ib) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Texture
    rhi::TextureDesc td{}; td.format = rhi::TextureFormat::RGBA8Unorm; td.width = kTexSize; td.height = kTexSize;
    td.mipLevelCount = 1; td.usage = rhi::TextureUsage::Sampled | rhi::TextureUsage::CopyDst; td.label = u8"BatchTex";
    if (m_device->createTexture(td, m_tex) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    rhi::TextureViewDesc tvd{}; tvd.format = rhi::TextureFormat::RGBA8Unorm; tvd.mipLevelCount = 1; tvd.arrayLayerCount = 1;
    if (m_device->createTextureView(m_tex, tvd, m_texView) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    rhi::SamplerDesc sd{}; sd.minFilter = rhi::FilterMode::Linear; sd.magFilter = rhi::FilterMode::Linear;
    sd.addressU = rhi::AddressMode::Repeat; sd.addressV = rhi::AddressMode::Repeat; sd.label = u8"BatchSampler";
    if (m_device->createSampler(sd, m_sampler) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Transform UBO
    rhi::BufferDesc tbd{}; tbd.size = 16; tbd.usage = rhi::BufferUsage::Uniform; tbd.memory = rhi::MemoryLocation::CpuToGpu; tbd.label = u8"BatchTransform";
    if (m_device->createBuffer(tbd, m_transformBuf) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    m_transformMapped = m_transformBuf->map();

    // Bind group layout: UBO + texture + sampler
    rhi::BindGroupLayoutEntry bglEntries[3] = {
        rhi::BindGroupLayoutEntry::uniformBuffer(0, rhi::ShaderStage::Vertex),
        rhi::BindGroupLayoutEntry::sampledTexture(0, rhi::ShaderStage::Fragment),
        rhi::BindGroupLayoutEntry::sampler(0, rhi::ShaderStage::Fragment),
    };
    rhi::BindGroupLayoutDesc bgld{}; bgld.entries = std::span<const rhi::BindGroupLayoutEntry>(bglEntries, 3); bgld.label = u8"BatchBGL";
    if (m_device->createBindGroupLayout(bgld, m_bgl) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    rhi::BindGroupEntry bgEntries[3] = {
        rhi::BindGroupEntry::bufferEntry(m_transformBuf, 0, 16),
        rhi::BindGroupEntry::textureEntry(m_texView),
        rhi::BindGroupEntry::samplerEntry(m_sampler),
    };
    rhi::BindGroupDesc bgd{}; bgd.layout = m_bgl; bgd.entries = std::span<const rhi::BindGroupEntry>(bgEntries, 3); bgd.label = u8"BatchBG";
    if (m_device->createBindGroup(bgd, m_bg) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Pipeline layout
    rhi::BindGroupLayout* bgls[1] = { m_bgl };
    rhi::PipelineLayoutDesc pld{}; pld.bindGroupLayouts = std::span<rhi::BindGroupLayout* const>(bgls, 1); pld.label = u8"BatchPL";
    if (m_device->createPipelineLayout(pld, m_pl) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Render pipeline
    rhi::VertexAttribute attrs[2] = { {rhi::VertexFormat::Float32x3, 0, 0}, {rhi::VertexFormat::Float32x2, 12, 1} };
    rhi::VertexBufferLayout vbl{}; vbl.stride = 20; vbl.attributes = std::span<const rhi::VertexAttribute>(attrs, 2);
    rhi::ColorTargetState ct{}; ct.format = m_swapChain->format();
    rhi::RenderPipelineDesc rpd{}; rpd.layout = m_pl;
    rpd.vertex.shader = { m_vs, u8"VSMain", rhi::ShaderStage::Vertex };
    rpd.vertex.buffers = std::span<const rhi::VertexBufferLayout>(&vbl, 1);
    rpd.fragment = rhi::FragmentState{}; rpd.fragment->shader = { m_ps, u8"PSMain", rhi::ShaderStage::Fragment };
    rpd.fragment->targets = std::span<const rhi::ColorTargetState>(&ct, 1);
    rpd.primitive.topology = rhi::PrimitiveTopology::TriangleList;
    rpd.label = u8"BatchPipeline";
    if (m_device->createRenderPipeline(rpd, m_pipeline) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    if (m_device->createCommandPool(rhi::QueueType::Graphics, m_pool) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (m_device->createFence(0, m_frameFence) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Upload fence
    if (m_device->createFence(0, m_uploadFence) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // === Batch upload: VB + IB + texture in one submission ===
    if (doBatchUpload() != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    return draco::ErrorCode::Ok;
}

draco::Status BatchUploadSample::doBatchUpload() {
    using draco::Status, std::span, draco::u8, draco::u32;

    m_uploadStartTime = m_totalTime;

    rhi::TransferBatch* transfer = nullptr;
    if (m_graphicsQueue->createTransferBatch(transfer) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Vertex data: quad
    float verts[20] = {
        -0.6f,  0.6f, 0.0f,   0.0f, 0.0f,
         0.6f,  0.6f, 0.0f,   1.0f, 0.0f,
         0.6f, -0.6f, 0.0f,   1.0f, 1.0f,
        -0.6f, -0.6f, 0.0f,   0.0f, 1.0f,
    };
    transfer->writeBuffer(m_vb, 0, std::span<const u8>(reinterpret_cast<const u8*>(verts), 80));

    // Index data
    draco::u16 indices[6] = { 0, 1, 2, 0, 2, 3 };
    transfer->writeBuffer(m_ib, 0, std::span<const u8>(reinterpret_cast<const u8*>(indices), 12));

    // Texture data: procedural mandelbrot-ish pattern
    u32 texBytes = kTexSize * kTexSize * 4;
    auto* pixels = new u8[texBytes];

    for (u32 y = 0; y < kTexSize; y++) {
        for (u32 x = 0; x < kTexSize; x++) {
            float cr = static_cast<float>(x) / static_cast<float>(kTexSize) * 3.0f - 2.0f;
            float ci = static_cast<float>(y) / static_cast<float>(kTexSize) * 2.4f - 1.2f;
            float zr = 0, zi = 0;
            int iter = 0;
            for (iter = 0; iter < 64; iter++) {
                float zr2 = zr * zr - zi * zi + cr;
                float zi2 = 2.0f * zr * zi + ci;
                zr = zr2; zi = zi2;
                if (zr * zr + zi * zi > 4.0f) break;
            }

            u32 off = (y * kTexSize + x) * 4;
            if (iter == 64) {
                pixels[off] = 10; pixels[off + 1] = 10; pixels[off + 2] = 30; pixels[off + 3] = 255;
            } else {
                float t = static_cast<float>(iter) / 64.0f;
                pixels[off]     = static_cast<u8>(t * 200 + 55);
                pixels[off + 1] = static_cast<u8>(t * t * 255);
                pixels[off + 2] = static_cast<u8>(std::sqrt(t) * 255);
                pixels[off + 3] = 255;
            }
        }
    }

    rhi::TextureDataLayout layout{}; layout.bytesPerRow = kTexSize * 4; layout.rowsPerImage = kTexSize;
    transfer->writeTexture(m_tex, std::span<const u8>(pixels, texBytes), layout, rhi::Extent3D{kTexSize, kTexSize, 1});

    delete[] pixels;

    // Async submit - signals fence when GPU transfer completes
    m_uploadFenceVal = 1;
    if (transfer->submitAsync(m_uploadFence, m_uploadFenceVal) != draco::ErrorCode::Ok) {
        m_graphicsQueue->destroyTransferBatch(transfer);
        return draco::ErrorCode::Unknown;
    }

    std::printf("Batch upload submitted asynchronously (VB: 80B, IB: 12B, Tex: %uB)\n", texBytes);
    m_graphicsQueue->destroyTransferBatch(transfer);
    return draco::ErrorCode::Ok;
}

void BatchUploadSample::onRender() {
    using draco::f32, std::span;

    if (m_frameFenceVal > 0) m_frameFence->wait(m_frameFenceVal, ~0ull);

    // Check if async upload has completed
    if (!m_uploadComplete) {
        if (m_uploadFence->completedValue() >= m_uploadFenceVal) {
            m_uploadComplete = true;
            std::printf("Batch upload completed! Rendering enabled.\n");
        }
    }

    if (m_swapChain->acquireNextImage() != draco::ErrorCode::Ok) return;

    // Update transform
    float transform[4] = { m_totalTime, 0, 0, 0 };
    std::memcpy(m_transformMapped, transform, 16);

    m_pool->reset();
    rhi::CommandEncoder* enc = nullptr;
    if (m_pool->createEncoder(enc) != draco::ErrorCode::Ok || !enc) return;

    enc->transitionTexture(m_swapChain->currentTexture(), rhi::ResourceState::Undefined, rhi::ResourceState::RenderTarget);

    rhi::ColorAttachment ca{}; ca.view = m_swapChain->currentTextureView();
    ca.loadOp = rhi::LoadOp::Clear; ca.storeOp = rhi::StoreOp::Store;
    ca.clearValue = rhi::ClearColor(0.05f, 0.05f, 0.08f, 1.0f);
    rhi::RenderPassDesc rpd{}; rpd.colorAttachments.push_back(ca);
    auto* rp = enc->beginRenderPass(rpd);

    if (m_uploadComplete) {
        rp->setPipeline(m_pipeline);
        rp->setBindGroup(0, m_bg);
        rp->setViewport(0, 0, static_cast<f32>(m_width), static_cast<f32>(m_height), 0.0f, 1.0f);
        rp->setScissor(0, 0, m_width, m_height);
        rp->setVertexBuffer(0, m_vb, 0);
        rp->setIndexBuffer(m_ib, rhi::IndexFormat::UInt16, 0);
        rp->drawIndexed(6);
    }

    rp->end();

    enc->transitionTexture(m_swapChain->currentTexture(), rhi::ResourceState::RenderTarget, rhi::ResourceState::Present);

    rhi::CommandBuffer* cb = enc->finish(); m_frameFenceVal++;
    rhi::CommandBuffer* cbs[1] = { cb };
    m_graphicsQueue->submit(std::span<rhi::CommandBuffer* const>(cbs, 1), m_frameFence, m_frameFenceVal);
    m_swapChain->present(m_graphicsQueue);
    m_pool->destroyEncoder(enc);
}

void BatchUploadSample::onShutdown() {
    if (m_transformBuf && m_transformMapped) m_transformBuf->unmap();

    if (m_uploadFence) m_device->destroyFence(m_uploadFence);
    if (m_frameFence) m_device->destroyFence(m_frameFence);
    if (m_pool) m_device->destroyCommandPool(m_pool);
    if (m_pipeline) m_device->destroyRenderPipeline(m_pipeline);
    if (m_pl) m_device->destroyPipelineLayout(m_pl);
    if (m_bg) m_device->destroyBindGroup(m_bg);
    if (m_bgl) m_device->destroyBindGroupLayout(m_bgl);
    if (m_sampler) m_device->destroySampler(m_sampler);
    if (m_texView) m_device->destroyTextureView(m_texView);
    if (m_tex) m_device->destroyTexture(m_tex);
    if (m_transformBuf) m_device->destroyBuffer(m_transformBuf);
    if (m_ib) m_device->destroyBuffer(m_ib);
    if (m_vb) m_device->destroyBuffer(m_vb);
    if (m_ps) m_device->destroyShaderModule(m_ps);
    if (m_vs) m_device->destroyShaderModule(m_vs);
    if (m_compiler) { m_compiler->destroy(); delete m_compiler; }
}

int main(int argc, char** argv) { BatchUploadSample app; return app.run(argc, argv); }
