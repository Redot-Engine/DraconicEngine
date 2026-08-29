#include <new>
/// Sample003 - Rotating Cube with Uniform Buffers + Push Constants.

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

class UniformBufferSample : public sf::SampleApp {
public:
    using sf::SampleApp::SampleApp;
    std::u8string_view title() const override { return u8"Sample003 - Rotating Cube (Uniform Buffers)"; }

protected:
    draco::Status onInit() override;
    void          onRender() override;
    void          onResize(draco::u32 w, draco::u32 h) override { m_depthBuf.recreate(m_device, w, h); }
    void          onShutdown() override;

private:
    static constexpr const char8_t kShaderSource[] = u8R"(
        cbuffer UBO : register(b0, space0) { row_major float4x4 MVP; };
        struct PushData { float4 Tint; };
        [[vk::push_constant]] ConstantBuffer<PushData> gPush : register(b0, space1);
        struct VSInput { float3 Position : TEXCOORD0; float3 Color : TEXCOORD1; };
        struct PSInput { float4 Position : SV_POSITION; float3 Color : COLOR0; };
        PSInput VSMain(VSInput input) {
            PSInput o; o.Position = mul(float4(input.Position, 1.0), MVP); o.Color = input.Color; return o;
        }
        float4 PSMain(PSInput input) : SV_TARGET { return float4(input.Color * gPush.Tint.rgb, 1.0); }
    )";

    static constexpr float kCubeVerts[] = {
        -0.5f,-0.5f,-0.5f, 1,0,0,  0.5f,-0.5f,-0.5f, 0,1,0,  0.5f, 0.5f,-0.5f, 0,0,1,  -0.5f, 0.5f,-0.5f, 1,1,0,
        -0.5f,-0.5f, 0.5f, 1,0,1,  0.5f,-0.5f, 0.5f, 0,1,1,  0.5f, 0.5f, 0.5f, 1,1,1,  -0.5f, 0.5f, 0.5f, .5f,.5f,.5f,
    };
    static constexpr draco::u16 kCubeIdx[] = {
        0,2,1, 0,3,2,  4,5,6, 4,6,7,  4,7,3, 4,3,0,  1,2,6, 1,6,5,  3,7,6, 3,6,2,  4,0,1, 4,1,5,
    };

    shaders::Compiler* m_compiler = nullptr;
    rhi::Buffer *m_vb = nullptr, *m_ib = nullptr, *m_ub = nullptr;
    rhi::ShaderModule *m_vs = nullptr, *m_ps = nullptr;
    rhi::BindGroupLayout* m_bgl = nullptr;
    rhi::BindGroup* m_bg = nullptr;
    rhi::PipelineLayout* m_pl = nullptr;
    rhi::RenderPipeline* m_pipeline = nullptr;
    rhi::CommandPool* m_pool = nullptr;
    rhi::Fence* m_fence = nullptr;
    draco::u64 m_fenceVal = 0;
    void* m_ubMapped = nullptr;
    sf::DepthBuffer m_depthBuf;
};

draco::Status UniformBufferSample::onInit() {
    using draco::Status, std::span, draco::u8;

    if (shaders::createCompiler(shaders::CompilerDesc{}, m_compiler) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (sf::compileToModule(m_compiler, m_device, kShaderSource, shaders::ShaderStage::Vertex,   u8"VSMain", u8"CubeVS", m_vs) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (sf::compileToModule(m_compiler, m_device, kShaderSource, shaders::ShaderStage::Fragment, u8"PSMain", u8"CubePS", m_ps) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Buffers.
    rhi::BufferDesc vbd{}; vbd.size = sizeof(kCubeVerts); vbd.usage = rhi::BufferUsage::Vertex | rhi::BufferUsage::CopyDst; vbd.memory = rhi::MemoryLocation::GpuOnly;
    if (m_device->createBuffer(vbd, m_vb) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    rhi::BufferDesc ibd{}; ibd.size = sizeof(kCubeIdx); ibd.usage = rhi::BufferUsage::Index | rhi::BufferUsage::CopyDst; ibd.memory = rhi::MemoryLocation::GpuOnly;
    if (m_device->createBuffer(ibd, m_ib) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    rhi::BufferDesc ubd{}; ubd.size = 64; ubd.usage = rhi::BufferUsage::Uniform; ubd.memory = rhi::MemoryLocation::CpuToGpu;
    if (m_device->createBuffer(ubd, m_ub) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    m_ubMapped = m_ub->map();

    // Upload.
    rhi::TransferBatch* batch = nullptr; m_graphicsQueue->createTransferBatch(batch);
    batch->writeBuffer(m_vb, 0, std::span<const u8>(reinterpret_cast<const u8*>(kCubeVerts), sizeof(kCubeVerts)));
    batch->writeBuffer(m_ib, 0, std::span<const u8>(reinterpret_cast<const u8*>(kCubeIdx), sizeof(kCubeIdx)));
    batch->submit(); m_graphicsQueue->destroyTransferBatch(batch);

    // Bind group layout + group.
    rhi::BindGroupLayoutEntry bglE[1] = { rhi::BindGroupLayoutEntry::uniformBuffer(0, rhi::ShaderStage::Vertex | rhi::ShaderStage::Fragment) };
    rhi::BindGroupLayoutDesc bgld{}; bgld.entries = std::span<const rhi::BindGroupLayoutEntry>(bglE, 1);
    if (m_device->createBindGroupLayout(bgld, m_bgl) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    rhi::BindGroupEntry bgE[1] = { rhi::BindGroupEntry::bufferEntry(m_ub, 0, 64) };
    rhi::BindGroupDesc bgd{}; bgd.layout = m_bgl; bgd.entries = std::span<const rhi::BindGroupEntry>(bgE, 1);
    if (m_device->createBindGroup(bgd, m_bg) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Pipeline layout with push constants.
    rhi::BindGroupLayout* sets[1] = { m_bgl };
    rhi::PushConstantRange pc{ rhi::ShaderStage::Vertex | rhi::ShaderStage::Fragment, 0, 16 };
    rhi::PipelineLayoutDesc pld{};
    pld.bindGroupLayouts = std::span<rhi::BindGroupLayout* const>(sets, 1);
    pld.pushConstantRanges = std::span<const rhi::PushConstantRange>(&pc, 1);
    if (m_device->createPipelineLayout(pld, m_pl) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Depth buffer.
    m_depthBuf.recreate(m_device, m_width, m_height);

    // Render pipeline.
    rhi::VertexAttribute attrs[2] = { {rhi::VertexFormat::Float32x3, 0, 0}, {rhi::VertexFormat::Float32x3, 12, 1} };
    rhi::VertexBufferLayout vbl{}; vbl.stride = 24; vbl.attributes = std::span<const rhi::VertexAttribute>(attrs, 2);
    rhi::ColorTargetState ct{}; ct.format = m_swapChain->format();

    rhi::RenderPipelineDesc rpd{};
    rpd.layout = m_pl;
    rpd.vertex.shader = { m_vs, u8"VSMain", rhi::ShaderStage::Vertex };
    rpd.vertex.buffers = std::span<const rhi::VertexBufferLayout>(&vbl, 1);
    rpd.fragment = rhi::FragmentState{}; rpd.fragment->shader = { m_ps, u8"PSMain", rhi::ShaderStage::Fragment };
    rpd.fragment->targets = std::span<const rhi::ColorTargetState>(&ct, 1);
    rpd.primitive = { rhi::PrimitiveTopology::TriangleList, rhi::FrontFace::CW, rhi::CullMode::Back };
    rpd.depthStencil = rhi::DepthStencilState{}; rpd.depthStencil->format = rhi::TextureFormat::Depth24PlusStencil8;
    rpd.depthStencil->depthCompare = rhi::CompareFunction::Less;
    if (m_device->createRenderPipeline(rpd, m_pipeline) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    if (m_device->createCommandPool(rhi::QueueType::Graphics, m_pool) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (m_device->createFence(0, m_fence) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    return draco::ErrorCode::Ok;
}

void UniformBufferSample::onRender() {
    if (m_fenceVal > 0) m_fence->wait(m_fenceVal, ~0ull);
    if (m_swapChain->acquireNextImage() != draco::ErrorCode::Ok) return;

    // Update MVP.
    draco::f32 aspect = static_cast<draco::f32>(m_width) / static_cast<draco::f32>(m_height);
    draco::f32 angle = m_totalTime * 1.2f;
    Matrix4 model = (Matrix4::rotationY(angle) * Matrix4::rotationX( angle * 0.7f));
    Matrix4 view  = Matrix4::lookAtRH(draco::math::Vector3{0, 1.5f, -3}, draco::math::Vector3{ 0, 0, 0}, draco::math::Vector3{ 0, 1, 0});
    Matrix4 proj  = Matrix4::perspectiveFovRH(draco::math::degToRad(45.0f), aspect, 0.1f, 100.0f);
    Matrix4 mvp   = model * view * proj;
    std::memcpy(m_ubMapped, mvp.data(), 64);

    m_pool->reset();
    rhi::CommandEncoder* enc = nullptr;
    if (m_pool->createEncoder(enc) != draco::ErrorCode::Ok || !enc) return;

    enc->transitionTexture(m_swapChain->currentTexture(), rhi::ResourceState::Undefined, rhi::ResourceState::RenderTarget);
    enc->transitionTexture(m_depthBuf.texture, rhi::ResourceState::Undefined, rhi::ResourceState::DepthStencilWrite);

    rhi::ColorAttachment ca{}; ca.view = m_swapChain->currentTextureView();
    ca.loadOp = rhi::LoadOp::Clear; ca.storeOp = rhi::StoreOp::Store;
    ca.clearValue = rhi::ClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    rhi::DepthStencilAttachment dsa{}; dsa.view = m_depthBuf.view;
    dsa.depthLoadOp = rhi::LoadOp::Clear; dsa.depthStoreOp = rhi::StoreOp::Store; dsa.depthClearValue = 1.0f;
    rhi::RenderPassDesc rpd{}; rpd.colorAttachments.push_back(ca); rpd.depthStencilAttachment = dsa;

    auto* rp = enc->beginRenderPass(rpd);
    rp->setPipeline(m_pipeline);
    rp->setBindGroup(0, m_bg);
    draco::f32 pulse = (std::sin(m_totalTime * 2.0f) * 0.3f + 0.7f);
    draco::f32 tint[4] = { pulse, pulse, pulse, 1.0f };
    rp->setPushConstants(rhi::ShaderStage::Vertex | rhi::ShaderStage::Fragment, 0, 16, tint);
    rp->setViewport(0, 0, static_cast<draco::f32>(m_width), static_cast<draco::f32>(m_height), 0, 1);
    rp->setScissor(0, 0, m_width, m_height);
    rp->setVertexBuffer(0, m_vb, 0);
    rp->setIndexBuffer(m_ib, rhi::IndexFormat::UInt16, 0);
    rp->drawIndexed(36);
    rp->end();

    enc->transitionTexture(m_swapChain->currentTexture(), rhi::ResourceState::RenderTarget, rhi::ResourceState::Present);

    rhi::CommandBuffer* cb = enc->finish(); m_fenceVal++;
    rhi::CommandBuffer* cbs[1] = { cb };
    m_graphicsQueue->submit(std::span<rhi::CommandBuffer* const>(cbs, 1), m_fence, m_fenceVal);
    m_swapChain->present(m_graphicsQueue);
    m_pool->destroyEncoder(enc);
}

void UniformBufferSample::onShutdown() {
    m_depthBuf.destroy(m_device);
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

int main(int argc, char** argv) { UniformBufferSample app; return app.run(argc, argv); }
