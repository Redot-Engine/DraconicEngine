#include <new>
/// Renders a rotating icosahedron in wireframe mode.

#include <cmath>
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

class WireframeSample : public sf::SampleApp {
public:
    using sf::SampleApp::SampleApp;
    std::u8string_view title() const override { return u8"Sample012 - Wireframe"; }
protected:
    draco::Status onInit() override;
    void onRender() override;
    void onResize(draco::u32 w, draco::u32 h) override { m_depthBuf.recreate(m_device, w, h); }
    void onShutdown() override;
private:
    static constexpr const char8_t kShader[] = u8R"(
        cbuffer UBO : register(b0, space0) { row_major float4x4 MVP; };
        struct VSInput { float3 Position : TEXCOORD0; float4 Color : TEXCOORD1; };
        struct PSInput { float4 Position : SV_POSITION; float4 Color : COLOR0; };
        PSInput VSMain(VSInput i) { PSInput o; o.Position = mul(float4(i.Position,1), MVP); o.Color = i.Color; return o; }
        float4 PSMain(PSInput i) : SV_TARGET { return i.Color; }
    )";

    shaders::Compiler* m_compiler = nullptr;
    rhi::ShaderModule *m_vs=nullptr, *m_ps=nullptr;
    rhi::Buffer *m_vb=nullptr, *m_ib=nullptr, *m_ub=nullptr;
    void* m_ubMapped = nullptr;
    rhi::BindGroupLayout* m_bgl=nullptr; rhi::BindGroup* m_bg=nullptr;
    rhi::PipelineLayout* m_pl=nullptr;
    rhi::RenderPipeline *m_wirePipe=nullptr;
    rhi::CommandPool* m_pool=nullptr; rhi::Fence* m_fence=nullptr;
    draco::u64 m_fenceVal = 0;
    draco::u32 m_indexCount = 0;
    sf::DepthBuffer m_depthBuf;
};

draco::Status WireframeSample::onInit() {
    using draco::Status, std::span, draco::u8, draco::f32;
    if (shaders::createCompiler(shaders::CompilerDesc{}, m_compiler) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (sf::compileToModule(m_compiler, m_device, kShader, shaders::ShaderStage::Vertex,   u8"VSMain", u8"VS", m_vs) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (sf::compileToModule(m_compiler, m_device, kShader, shaders::ShaderStage::Fragment, u8"PSMain", u8"PS", m_ps) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Icosahedron.
    f32 t = (1.0f + std::sqrt(5.0f)) / 2.0f;
    f32 s = 1.0f / std::sqrt(1.0f + t*t);
    f32 a = s, b = t*s;
    f32 vertData[84] = {
        -a,b,0, 1,.3f,.3f,1,  a,b,0, .3f,1,.3f,1,  -a,-b,0, .3f,.3f,1,1,  a,-b,0, 1,1,.3f,1,
        0,-a,b, 1,.3f,1,1,  0,a,b, .3f,1,1,1,  0,-a,-b, 1,.6f,.3f,1,  0,a,-b, .6f,.3f,1,1,
        b,0,-a, .3f,1,.6f,1,  b,0,a, 1,.6f,.6f,1,  -b,0,-a, .6f,1,.3f,1,  -b,0,a, .6f,.3f,.6f,1,
    };
    draco::u16 idxData[60] = {
        0,11,5, 0,5,1, 0,1,7, 0,7,10, 0,10,11,
        1,5,9, 5,11,4, 11,10,2, 10,7,6, 7,1,8,
        3,9,4, 3,4,2, 3,2,6, 3,6,8, 3,8,9,
        4,9,5, 2,4,11, 6,2,10, 8,6,7, 9,8,1,
    };
    m_indexCount = 60;

    rhi::BufferDesc vbd{}; vbd.size = sizeof(vertData); vbd.usage = rhi::BufferUsage::Vertex | rhi::BufferUsage::CopyDst; vbd.memory = rhi::MemoryLocation::GpuOnly;
    if (m_device->createBuffer(vbd, m_vb) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    rhi::BufferDesc ibd{}; ibd.size = sizeof(idxData); ibd.usage = rhi::BufferUsage::Index | rhi::BufferUsage::CopyDst; ibd.memory = rhi::MemoryLocation::GpuOnly;
    if (m_device->createBuffer(ibd, m_ib) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    rhi::TransferBatch* batch = nullptr; m_graphicsQueue->createTransferBatch(batch);
    batch->writeBuffer(m_vb, 0, std::span<const u8>(reinterpret_cast<const u8*>(vertData), sizeof(vertData)));
    batch->writeBuffer(m_ib, 0, std::span<const u8>(reinterpret_cast<const u8*>(idxData), sizeof(idxData)));
    batch->submit(); m_graphicsQueue->destroyTransferBatch(batch);

    rhi::BufferDesc ubd{}; ubd.size = 256; ubd.usage = rhi::BufferUsage::Uniform; ubd.memory = rhi::MemoryLocation::CpuToGpu;
    if (m_device->createBuffer(ubd, m_ub) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    m_ubMapped = m_ub->map();

    rhi::BindGroupLayoutEntry bglE[1] = { rhi::BindGroupLayoutEntry::uniformBuffer(0, rhi::ShaderStage::Vertex) };
    rhi::BindGroupLayoutDesc bgld{}; bgld.entries = std::span<const rhi::BindGroupLayoutEntry>(bglE, 1);
    if (m_device->createBindGroupLayout(bgld, m_bgl) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    rhi::BindGroupEntry bgE[1] = { rhi::BindGroupEntry::bufferEntry(m_ub, 0, 64) };
    rhi::BindGroupDesc bgd{}; bgd.layout = m_bgl; bgd.entries = std::span<const rhi::BindGroupEntry>(bgE, 1);
    if (m_device->createBindGroup(bgd, m_bg) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    rhi::BindGroupLayout* sets[1] = { m_bgl };
    rhi::PipelineLayoutDesc pld{}; pld.bindGroupLayouts = std::span<rhi::BindGroupLayout* const>(sets, 1);
    if (m_device->createPipelineLayout(pld, m_pl) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    m_depthBuf.recreate(m_device, m_width, m_height);

    rhi::VertexAttribute attrs[2] = { {rhi::VertexFormat::Float32x3, 0, 0}, {rhi::VertexFormat::Float32x4, 12, 1} };
    rhi::VertexBufferLayout vbl{}; vbl.stride = 28; vbl.attributes = std::span<const rhi::VertexAttribute>(attrs, 2);
    rhi::ColorTargetState ct{}; ct.format = m_swapChain->format();
    rhi::RenderPipelineDesc rpd{}; rpd.layout = m_pl;
    rpd.vertex.shader = { m_vs, u8"VSMain", rhi::ShaderStage::Vertex };
    rpd.vertex.buffers = std::span<const rhi::VertexBufferLayout>(&vbl, 1);
    rpd.fragment = rhi::FragmentState{}; rpd.fragment->shader = { m_ps, u8"PSMain", rhi::ShaderStage::Fragment };
    rpd.fragment->targets = std::span<const rhi::ColorTargetState>(&ct, 1);
    rpd.primitive = { rhi::PrimitiveTopology::TriangleList, rhi::FrontFace::CCW, rhi::CullMode::None, rhi::FillMode::Wireframe };
    rpd.depthStencil = rhi::DepthStencilState{}; rpd.depthStencil->format = rhi::TextureFormat::Depth24PlusStencil8;
    rpd.depthStencil->depthCompare = rhi::CompareFunction::LessEqual;
    rpd.depthStencil->depthWriteEnabled = false;
    if (m_device->createRenderPipeline(rpd, m_wirePipe) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    if (m_device->createCommandPool(rhi::QueueType::Graphics, m_pool) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (m_device->createFence(0, m_fence) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    return draco::ErrorCode::Ok;
}

void WireframeSample::onRender() {
    using draco::f32, std::span;
    if (m_fenceVal > 0) m_fence->wait(m_fenceVal, ~0ull);
    if (m_swapChain->acquireNextImage() != draco::ErrorCode::Ok) return;
    f32 aspect = static_cast<f32>(m_width) / static_cast<f32>(m_height);
    Matrix4 model = Matrix4::rotationY(m_totalTime * 0.8f);
    // Row-vector view: identity rotation, camera 3 units along +Z (RH: looking toward -Z).
    // The view is the inverse of the camera transform, so the translation is the NEGATED
    // eye position: m[3][2] = -3 (puts the object at view-space z=-3, in front of the camera).
    // PerspectiveFovRH gives clip.w = -viewZ, so geometry must have negative view z.
    f32 view[16] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,-3,1 };
    Matrix4 proj = Matrix4::perspectiveFovRH(draco::math::degToRad(45.0f), aspect, 0.1f, 100.0f);
    Matrix4 vMat; std::memcpy(vMat.data(), view, 64);
    Matrix4 mvp = model * vMat * proj;
    std::memcpy(m_ubMapped, mvp.data(), 64);

    m_pool->reset();
    rhi::CommandEncoder* enc = nullptr;
    if (m_pool->createEncoder(enc) != draco::ErrorCode::Ok || !enc) return;
    enc->transitionTexture(m_swapChain->currentTexture(), rhi::ResourceState::Undefined, rhi::ResourceState::RenderTarget);
    enc->transitionTexture(m_depthBuf.texture, rhi::ResourceState::Undefined, rhi::ResourceState::DepthStencilWrite);
    rhi::ColorAttachment ca{}; ca.view = m_swapChain->currentTextureView();
    ca.loadOp = rhi::LoadOp::Clear; ca.storeOp = rhi::StoreOp::Store; ca.clearValue = rhi::ClearColor(0.06f,0.06f,0.1f,1);
    rhi::DepthStencilAttachment dsa{}; dsa.view = m_depthBuf.view;
    dsa.depthLoadOp = rhi::LoadOp::Clear; dsa.depthStoreOp = rhi::StoreOp::Store; dsa.depthClearValue = 1.0f;
    rhi::RenderPassDesc rpd{}; rpd.colorAttachments.push_back(ca); rpd.depthStencilAttachment = dsa;
    auto* rp = enc->beginRenderPass(rpd);
    rp->setPipeline(m_wirePipe); rp->setBindGroup(0, m_bg);
    rp->setViewport(0,0,static_cast<f32>(m_width),static_cast<f32>(m_height),0,1);
    rp->setScissor(0,0,m_width,m_height);
    rp->setVertexBuffer(0, m_vb, 0); rp->setIndexBuffer(m_ib, rhi::IndexFormat::UInt16, 0);
    rp->drawIndexed(m_indexCount); rp->end();
    enc->transitionTexture(m_swapChain->currentTexture(), rhi::ResourceState::RenderTarget, rhi::ResourceState::Present);
    rhi::CommandBuffer* cb = enc->finish(); m_fenceVal++;
    rhi::CommandBuffer* cbs[1] = { cb };
    m_graphicsQueue->submit(std::span<rhi::CommandBuffer* const>(cbs, 1), m_fence, m_fenceVal);
    m_swapChain->present(m_graphicsQueue); m_pool->destroyEncoder(enc);
}

void WireframeSample::onShutdown() {
    m_depthBuf.destroy(m_device);
    if (m_fence) m_device->destroyFence(m_fence); if (m_pool) m_device->destroyCommandPool(m_pool);
    if (m_wirePipe) m_device->destroyRenderPipeline(m_wirePipe); if (m_pl) m_device->destroyPipelineLayout(m_pl);
    if (m_bg) m_device->destroyBindGroup(m_bg); if (m_bgl) m_device->destroyBindGroupLayout(m_bgl);
    if (m_ub) m_device->destroyBuffer(m_ub); if (m_ib) m_device->destroyBuffer(m_ib); if (m_vb) m_device->destroyBuffer(m_vb);
    if (m_ps) m_device->destroyShaderModule(m_ps); if (m_vs) m_device->destroyShaderModule(m_vs);
    if (m_compiler) { m_compiler->destroy(); delete m_compiler; }
}

int main(int argc, char** argv) { WireframeSample app; return app.run(argc, argv); }
