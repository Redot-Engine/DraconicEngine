#include <new>
/// Renders a colored triangle to a small offscreen texture, copies it to a
/// readback buffer, then reads pixel values on the CPU and prints them.

#include <cstdint>
#include <cstdio>
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

class ReadbackSample : public sf::SampleApp {
public:
    using sf::SampleApp::SampleApp;
    std::u8string_view title() const override { return u8"Sample016 - GPU Readback"; }
protected:
    draco::Status onInit() override;
    void onRender() override;
    void onShutdown() override;
private:
    static constexpr const char8_t kShader[] = u8R"(
        struct VSInput { float3 Position : TEXCOORD0; float4 Color : TEXCOORD1; };
        struct PSInput { float4 Position : SV_POSITION; float4 Color : COLOR0; };
        PSInput VSMain(VSInput i) { PSInput o; o.Position = float4(i.Position,1); o.Color = i.Color; return o; }
        float4 PSMain(PSInput i) : SV_TARGET { return i.Color; }
    )";
    static constexpr draco::u32 kTexSize = 16;
    static constexpr float kVerts[] = {
         0.0f,  1.0f, 0.0f,   1.0f, 0.0f, 0.0f, 1.0f,
         1.0f, -1.0f, 0.0f,   0.0f, 1.0f, 0.0f, 1.0f,
        -1.0f, -1.0f, 0.0f,   0.0f, 0.0f, 1.0f, 1.0f,
    };

    void readbackPixels();

    shaders::Compiler* m_compiler = nullptr;
    rhi::ShaderModule *m_vs = nullptr, *m_ps = nullptr;
    rhi::Buffer *m_vb = nullptr;
    rhi::PipelineLayout *m_pl = nullptr;
    rhi::RenderPipeline *m_offPipeline = nullptr, *m_swapPipeline = nullptr;
    rhi::Texture *m_offTex = nullptr; rhi::TextureView *m_offView = nullptr;
    rhi::Buffer *m_readbackBuf = nullptr;
    rhi::CommandPool *m_pool = nullptr; rhi::Fence *m_fence = nullptr;
    draco::u64 m_fenceVal = 0;
    bool m_hasReadback = false;
    float m_lastReportTime = 0.0f;
};

draco::Status ReadbackSample::onInit() {
    using draco::Status, std::span, draco::u8, draco::u32;
    if (shaders::createCompiler(shaders::CompilerDesc{}, m_compiler) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (sf::compileToModule(m_compiler, m_device, kShader, shaders::ShaderStage::Vertex,   u8"VSMain", u8"VS", m_vs) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (sf::compileToModule(m_compiler, m_device, kShader, shaders::ShaderStage::Fragment, u8"PSMain", u8"PS", m_ps) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    rhi::BufferDesc vbd{}; vbd.size = sizeof(kVerts); vbd.usage = rhi::BufferUsage::Vertex | rhi::BufferUsage::CopyDst; vbd.memory = rhi::MemoryLocation::GpuOnly;
    if (m_device->createBuffer(vbd, m_vb) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    rhi::TransferBatch* batch = nullptr; m_graphicsQueue->createTransferBatch(batch);
    batch->writeBuffer(m_vb, 0, std::span<const u8>(reinterpret_cast<const u8*>(kVerts), sizeof(kVerts)));
    batch->submit(); m_graphicsQueue->destroyTransferBatch(batch);

    rhi::PipelineLayoutDesc pld{};
    if (m_device->createPipelineLayout(pld, m_pl) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Small offscreen RGBA8 texture.
    rhi::TextureDesc td{}; td.format = rhi::TextureFormat::RGBA8Unorm; td.width = kTexSize; td.height = kTexSize;
    td.mipLevelCount = 1; td.usage = rhi::TextureUsage::RenderTarget | rhi::TextureUsage::CopySrc;
    if (m_device->createTexture(td, m_offTex) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    rhi::TextureViewDesc tvd{}; tvd.format = rhi::TextureFormat::RGBA8Unorm; tvd.mipLevelCount = 1; tvd.arrayLayerCount = 1;
    if (m_device->createTextureView(m_offTex, tvd, m_offView) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Readback buffer with row alignment (256 bytes for DX12 compat).
    u32 bytesPerRow = ((kTexSize * 4 + 255) / 256) * 256;
    rhi::BufferDesc rbd{}; rbd.size = bytesPerRow * kTexSize; rbd.usage = rhi::BufferUsage::CopyDst; rbd.memory = rhi::MemoryLocation::GpuToCpu;
    if (m_device->createBuffer(rbd, m_readbackBuf) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    rhi::VertexAttribute attrs[2] = { {rhi::VertexFormat::Float32x3, 0, 0}, {rhi::VertexFormat::Float32x4, 12, 1} };
    rhi::VertexBufferLayout vbl{}; vbl.stride = 28; vbl.attributes = std::span<const rhi::VertexAttribute>(attrs, 2);

    // Pipeline for offscreen (RGBA8Unorm).
    rhi::ColorTargetState ct{}; ct.format = rhi::TextureFormat::RGBA8Unorm;
    rhi::RenderPipelineDesc rpd{}; rpd.layout = m_pl;
    rpd.vertex.shader = { m_vs, u8"VSMain", rhi::ShaderStage::Vertex };
    rpd.vertex.buffers = std::span<const rhi::VertexBufferLayout>(&vbl, 1);
    rpd.fragment = rhi::FragmentState{}; rpd.fragment->shader = { m_ps, u8"PSMain", rhi::ShaderStage::Fragment };
    rpd.fragment->targets = std::span<const rhi::ColorTargetState>(&ct, 1);
    if (m_device->createRenderPipeline(rpd, m_offPipeline) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Pipeline for swapchain (different format).
    ct.format = m_swapChain->format();
    rpd.fragment->targets = std::span<const rhi::ColorTargetState>(&ct, 1);
    if (m_device->createRenderPipeline(rpd, m_swapPipeline) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    if (m_device->createCommandPool(rhi::QueueType::Graphics, m_pool) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (m_device->createFence(0, m_fence) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    return draco::ErrorCode::Ok;
}

void ReadbackSample::readbackPixels() {
    void* mapped = m_readbackBuf->map();
    if (!mapped) return;
    draco::u32 bytesPerRow = ((kTexSize * 4 + 255) / 256) * 256;
    auto* data = static_cast<draco::u8*>(mapped);

    std::printf("=== Readback: %ux%u RGBA8 texture ===\n", kTexSize, kTexSize);
    auto printPixel = [&](draco::u32 x, draco::u32 y, const char* label) {
        draco::u32 off = y * bytesPerRow + x * 4;
        std::printf("  %s (%u,%u): R=%u G=%u B=%u A=%u\n", label, x, y,
            data[off], data[off+1], data[off+2], data[off+3]);
    };
    printPixel(0, 0, "Top-left");
    printPixel(kTexSize-1, 0, "Top-right");
    printPixel(kTexSize/2, kTexSize/2, "Center");
    printPixel(0, kTexSize-1, "Bottom-left");
    printPixel(kTexSize-1, kTexSize-1, "Bottom-right");

    int nonBlack = 0;
    for (draco::u32 y = 0; y < kTexSize; ++y)
        for (draco::u32 x = 0; x < kTexSize; ++x) {
            draco::u32 off = y * bytesPerRow + x * 4;
            if (data[off] > 0 || data[off+1] > 0 || data[off+2] > 0) nonBlack++;
        }
    std::printf("Non-black pixels: %d / %u (%.0f%%)\n", nonBlack, kTexSize * kTexSize,
        100.0f * static_cast<float>(nonBlack) / static_cast<float>(kTexSize * kTexSize));
    m_readbackBuf->unmap();
}

void ReadbackSample::onRender() {
    using draco::f32, std::span;
    if (m_fenceVal > 0) m_fence->wait(m_fenceVal, ~0ull);

    if (m_hasReadback && (m_totalTime - m_lastReportTime >= 3.0f)) {
        readbackPixels();
        m_lastReportTime = m_totalTime;
    }

    if (m_swapChain->acquireNextImage() != draco::ErrorCode::Ok) return;
    m_pool->reset();
    rhi::CommandEncoder* enc = nullptr;
    if (m_pool->createEncoder(enc) != draco::ErrorCode::Ok || !enc) return;

    // Render triangle to offscreen texture.
    enc->transitionTexture(m_offTex, rhi::ResourceState::Undefined, rhi::ResourceState::RenderTarget);
    {
        rhi::ColorAttachment ca{}; ca.view = m_offView;
        ca.loadOp = rhi::LoadOp::Clear; ca.storeOp = rhi::StoreOp::Store;
        ca.clearValue = rhi::ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        rhi::RenderPassDesc rpd{}; rpd.colorAttachments.push_back(ca);
        auto* rp = enc->beginRenderPass(rpd);
        rp->setPipeline(m_offPipeline);
        rp->setViewport(0, 0, static_cast<f32>(kTexSize), static_cast<f32>(kTexSize), 0, 1);
        rp->setScissor(0, 0, kTexSize, kTexSize);
        rp->setVertexBuffer(0, m_vb, 0);
        rp->draw(3);
        rp->end();
    }
    enc->transitionTexture(m_offTex, rhi::ResourceState::RenderTarget, rhi::ResourceState::CopySrc);

    // Copy texture to readback buffer.
    draco::u32 bytesPerRow = ((kTexSize * 4 + 255) / 256) * 256;
    rhi::BufferTextureCopyRegion region{};
    region.bufferOffset = 0; region.bytesPerRow = bytesPerRow; region.rowsPerImage = kTexSize;
    region.textureExtent = rhi::Extent3D{ kTexSize, kTexSize, 1 };
    enc->copyTextureToBuffer(m_offTex, m_readbackBuf, region);

    // Also render to swapchain so we see something.
    enc->transitionTexture(m_swapChain->currentTexture(), rhi::ResourceState::Undefined, rhi::ResourceState::RenderTarget);
    {
        rhi::ColorAttachment ca{}; ca.view = m_swapChain->currentTextureView();
        ca.loadOp = rhi::LoadOp::Clear; ca.storeOp = rhi::StoreOp::Store;
        ca.clearValue = rhi::ClearColor(0.08f, 0.08f, 0.12f, 1.0f);
        rhi::RenderPassDesc rpd{}; rpd.colorAttachments.push_back(ca);
        auto* rp = enc->beginRenderPass(rpd);
        rp->setPipeline(m_swapPipeline);
        rp->setViewport(0, 0, static_cast<f32>(m_width), static_cast<f32>(m_height), 0, 1);
        rp->setScissor(0, 0, m_width, m_height);
        rp->setVertexBuffer(0, m_vb, 0);
        rp->draw(3);
        rp->end();
    }
    enc->transitionTexture(m_swapChain->currentTexture(), rhi::ResourceState::RenderTarget, rhi::ResourceState::Present);

    rhi::CommandBuffer* cb = enc->finish(); m_fenceVal++;
    rhi::CommandBuffer* cbs[1] = { cb };
    m_graphicsQueue->submit(std::span<rhi::CommandBuffer* const>(cbs, 1), m_fence, m_fenceVal);
    m_swapChain->present(m_graphicsQueue);
    m_pool->destroyEncoder(enc);
    m_hasReadback = true;
}

void ReadbackSample::onShutdown() {
    if (m_fence) m_device->destroyFence(m_fence); if (m_pool) m_device->destroyCommandPool(m_pool);
    if (m_swapPipeline) m_device->destroyRenderPipeline(m_swapPipeline);
    if (m_offPipeline) m_device->destroyRenderPipeline(m_offPipeline);
    if (m_pl) m_device->destroyPipelineLayout(m_pl);
    if (m_readbackBuf) m_device->destroyBuffer(m_readbackBuf);
    if (m_offView) m_device->destroyTextureView(m_offView);
    if (m_offTex) m_device->destroyTexture(m_offTex);
    if (m_vb) m_device->destroyBuffer(m_vb);
    if (m_ps) m_device->destroyShaderModule(m_ps); if (m_vs) m_device->destroyShaderModule(m_vs);
    if (m_compiler) { m_compiler->destroy(); delete m_compiler; }
}

int main(int argc, char** argv) { ReadbackSample app; return app.run(argc, argv); }
