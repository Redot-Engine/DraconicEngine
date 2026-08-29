#include <new>
/// Sample004 - Compute Shader (Animated Point Grid).

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

class ComputeSample : public sf::SampleApp {
public:
    using sf::SampleApp::SampleApp;
    std::u8string_view title() const override { return u8"Sample004 - Compute (Animated Point Grid)"; }
protected:
    draco::Status onInit() override;
    void onRender() override;
    void onResize(draco::u32 w, draco::u32 h) override { m_depthBuf.recreate(m_device, w, h); }
    void onShutdown() override;
private:
    static constexpr const char8_t kComputeSrc[] = u8R"(
        cbuffer Params : register(b0, space0) { float Time; uint NumPoints; float Spacing; float Padding; };
        struct Vertex { float PosX, PosY, PosZ, ColR, ColG, ColB; };
        RWStructuredBuffer<Vertex> gVertices : register(u0, space0);
        [numthreads(64, 1, 1)]
        void CSMain(uint3 dtid : SV_DispatchThreadID) {
            uint idx = dtid.x; if (idx >= NumPoints) return;
            uint gridSize = (uint)sqrt((float)NumPoints);
            uint row = idx / gridSize, col = idx % gridSize;
            float fx = ((float)col / (float)(gridSize-1))*2.0 - 1.0;
            float fz = ((float)row / (float)(gridSize-1))*2.0 - 1.0;
            float dist = sqrt(fx*fx + fz*fz);
            float fy = sin(dist*6.0 - Time*2.0) * 0.15;
            gVertices[idx].PosX = fx; gVertices[idx].PosY = fy; gVertices[idx].PosZ = fz;
            gVertices[idx].ColR = fx*0.5+0.5; gVertices[idx].ColG = fy*2.0+0.5; gVertices[idx].ColB = fz*0.5+0.5;
        }
    )";
    static constexpr const char8_t kRenderSrc[] = u8R"(
        cbuffer ViewProj : register(b0, space0) { row_major float4x4 VP; };
        struct VSInput { float3 Position : TEXCOORD0; float3 Color : TEXCOORD1; };
        struct PSInput { float4 Position : SV_POSITION; float3 Color : COLOR0;
                         [[vk::builtin("PointSize")]] float PointSize : PSIZE; };
        PSInput VSMain(VSInput i) { PSInput o; o.Position = mul(float4(i.Position,1), VP); o.Color = i.Color; o.PointSize = 1.0; return o; }
        float4 PSMain(PSInput i) : SV_TARGET { return float4(i.Color, 1.0); }
    )";

    static constexpr draco::u32 kGrid = 64, kNumPts = kGrid*kGrid, kVertSz = 24, kBufSz = kNumPts*kVertSz;

    shaders::Compiler* m_compiler = nullptr;
    rhi::ShaderModule *m_cs = nullptr, *m_vs = nullptr, *m_ps = nullptr;
    rhi::Buffer *m_vtxBuf = nullptr, *m_paramsBuf = nullptr, *m_vpBuf = nullptr;
    void *m_paramsMapped = nullptr, *m_vpMapped = nullptr;
    rhi::BindGroupLayout *m_compBgl = nullptr, *m_renBgl = nullptr;
    rhi::BindGroup *m_compBg = nullptr, *m_renBg = nullptr;
    rhi::PipelineLayout *m_compPl = nullptr, *m_renPl = nullptr;
    rhi::ComputePipeline *m_compPipe = nullptr;
    rhi::RenderPipeline *m_renPipe = nullptr;
    rhi::CommandPool *m_pool = nullptr; rhi::Fence *m_fence = nullptr;
    draco::u64 m_fenceVal = 0;
    sf::DepthBuffer m_depthBuf;
};

draco::Status ComputeSample::onInit() {
    using draco::Status, std::span, draco::u8, draco::u32;
    if (shaders::createCompiler(shaders::CompilerDesc{}, m_compiler) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (sf::compileToModule(m_compiler, m_device, kComputeSrc, shaders::ShaderStage::Compute, u8"CSMain", u8"CS", m_cs) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (sf::compileToModule(m_compiler, m_device, kRenderSrc, shaders::ShaderStage::Vertex,  u8"VSMain", u8"VS", m_vs) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (sf::compileToModule(m_compiler, m_device, kRenderSrc, shaders::ShaderStage::Fragment,u8"PSMain", u8"PS", m_ps) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Buffers.
    rhi::BufferDesc vbd{}; vbd.size = kBufSz; vbd.usage = rhi::BufferUsage::Storage | rhi::BufferUsage::Vertex; vbd.memory = rhi::MemoryLocation::GpuOnly;
    if (m_device->createBuffer(vbd, m_vtxBuf) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    rhi::BufferDesc pbd{}; pbd.size = 16; pbd.usage = rhi::BufferUsage::Uniform; pbd.memory = rhi::MemoryLocation::CpuToGpu;
    if (m_device->createBuffer(pbd, m_paramsBuf) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    m_paramsMapped = m_paramsBuf->map();
    rhi::BufferDesc vpd{}; vpd.size = 64; vpd.usage = rhi::BufferUsage::Uniform; vpd.memory = rhi::MemoryLocation::CpuToGpu;
    if (m_device->createBuffer(vpd, m_vpBuf) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    m_vpMapped = m_vpBuf->map();

    // Compute BGL + BG + PL + pipeline.
    rhi::BindGroupLayoutEntry cE[2] = { rhi::BindGroupLayoutEntry::uniformBuffer(0, rhi::ShaderStage::Compute),
                                        rhi::BindGroupLayoutEntry::storageBuffer(0, rhi::ShaderStage::Compute, false) };
    rhi::BindGroupLayoutDesc cBgld{}; cBgld.entries = std::span<const rhi::BindGroupLayoutEntry>(cE, 2);
    if (m_device->createBindGroupLayout(cBgld, m_compBgl) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    rhi::BindGroupEntry cBgE[2] = { rhi::BindGroupEntry::bufferEntry(m_paramsBuf, 0, 16),
                                    rhi::BindGroupEntry::bufferEntry(m_vtxBuf, 0, kBufSz) };
    rhi::BindGroupDesc cBgd{}; cBgd.layout = m_compBgl; cBgd.entries = std::span<const rhi::BindGroupEntry>(cBgE, 2);
    if (m_device->createBindGroup(cBgd, m_compBg) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    rhi::BindGroupLayout* cSets[1] = { m_compBgl };
    rhi::PipelineLayoutDesc cPld{}; cPld.bindGroupLayouts = std::span<rhi::BindGroupLayout* const>(cSets, 1);
    if (m_device->createPipelineLayout(cPld, m_compPl) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    rhi::ComputePipelineDesc cpd{}; cpd.layout = m_compPl; cpd.compute = { m_cs, u8"CSMain", rhi::ShaderStage::Compute };
    if (m_device->createComputePipeline(cpd, m_compPipe) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Render BGL + BG + PL + pipeline.
    rhi::BindGroupLayoutEntry rE[1] = { rhi::BindGroupLayoutEntry::uniformBuffer(0, rhi::ShaderStage::Vertex) };
    rhi::BindGroupLayoutDesc rBgld{}; rBgld.entries = std::span<const rhi::BindGroupLayoutEntry>(rE, 1);
    if (m_device->createBindGroupLayout(rBgld, m_renBgl) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    rhi::BindGroupEntry rBgE[1] = { rhi::BindGroupEntry::bufferEntry(m_vpBuf, 0, 64) };
    rhi::BindGroupDesc rBgd{}; rBgd.layout = m_renBgl; rBgd.entries = std::span<const rhi::BindGroupEntry>(rBgE, 1);
    if (m_device->createBindGroup(rBgd, m_renBg) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    rhi::BindGroupLayout* rSets[1] = { m_renBgl };
    rhi::PipelineLayoutDesc rPld{}; rPld.bindGroupLayouts = std::span<rhi::BindGroupLayout* const>(rSets, 1);
    if (m_device->createPipelineLayout(rPld, m_renPl) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    m_depthBuf.recreate(m_device, m_width, m_height);

    rhi::VertexAttribute attrs[2] = { {rhi::VertexFormat::Float32x3, 0, 0}, {rhi::VertexFormat::Float32x3, 12, 1} };
    rhi::VertexBufferLayout vbl{}; vbl.stride = kVertSz; vbl.attributes = std::span<const rhi::VertexAttribute>(attrs, 2);
    rhi::ColorTargetState ct{}; ct.format = m_swapChain->format();
    rhi::RenderPipelineDesc rpd{}; rpd.layout = m_renPl;
    rpd.vertex.shader = { m_vs, u8"VSMain", rhi::ShaderStage::Vertex };
    rpd.vertex.buffers = std::span<const rhi::VertexBufferLayout>(&vbl, 1);
    rpd.fragment = rhi::FragmentState{}; rpd.fragment->shader = { m_ps, u8"PSMain", rhi::ShaderStage::Fragment };
    rpd.fragment->targets = std::span<const rhi::ColorTargetState>(&ct, 1);
    rpd.primitive.topology = rhi::PrimitiveTopology::PointList;
    rpd.depthStencil = rhi::DepthStencilState{}; rpd.depthStencil->format = rhi::TextureFormat::Depth24PlusStencil8;
    rpd.depthStencil->depthCompare = rhi::CompareFunction::Less;
    if (m_device->createRenderPipeline(rpd, m_renPipe) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    if (m_device->createCommandPool(rhi::QueueType::Graphics, m_pool) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (m_device->createFence(0, m_fence) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    return draco::ErrorCode::Ok;
}

void ComputeSample::onRender() {
    using draco::u32, draco::f32, std::span;
    if (m_fenceVal > 0) m_fence->wait(m_fenceVal, ~0ull);
    if (m_swapChain->acquireNextImage() != draco::ErrorCode::Ok) return;

    // Update params.
    u32 numPts = kNumPts;
    f32 params[4] = { m_totalTime, 0, 1.0f, 0 };
    std::memcpy(&params[1], &numPts, 4);
    std::memcpy(m_paramsMapped, params, 16);

    // Update VP.
    f32 aspect = static_cast<f32>(m_width) / static_cast<f32>(m_height);
    f32 camAngle = m_totalTime * 0.3f, camDist = 2.5f;
    Matrix4 view = Matrix4::lookAtRH(draco::math::Vector3{std::sin(camAngle)*camDist, 1.2f, std::cos(camAngle)*camDist}, draco::math::Vector3{ 0,0,0}, draco::math::Vector3{0,1,0});
    Matrix4 proj = Matrix4::perspectiveFovRH(draco::math::degToRad(45.0f), aspect, 0.1f, 100.0f);
    Matrix4 vp = view * proj;
    std::memcpy(m_vpMapped, vp.data(), 64);

    m_pool->reset();
    rhi::CommandEncoder* enc = nullptr;
    if (m_pool->createEncoder(enc) != draco::ErrorCode::Ok || !enc) return;

    // Compute pass.
    rhi::BufferBarrier bb{}; bb.buffer = m_vtxBuf; bb.oldState = rhi::ResourceState::VertexBuffer; bb.newState = rhi::ResourceState::ShaderWrite;
    rhi::BarrierGroup bg1{}; bg1.bufferBarriers = std::span<const rhi::BufferBarrier>(&bb, 1);
    enc->barrier(bg1);
    auto* cp = enc->beginComputePass(u8"GenerateVertices");
    cp->setPipeline(m_compPipe); cp->setBindGroup(0, m_compBg);
    cp->dispatch((kNumPts + 63) / 64); cp->end();
    bb.oldState = rhi::ResourceState::ShaderWrite; bb.newState = rhi::ResourceState::VertexBuffer;
    enc->barrier(bg1);

    // Render pass.
    enc->transitionTexture(m_swapChain->currentTexture(), rhi::ResourceState::Undefined, rhi::ResourceState::RenderTarget);
    enc->transitionTexture(m_depthBuf.texture, rhi::ResourceState::Undefined, rhi::ResourceState::DepthStencilWrite);
    rhi::ColorAttachment ca{}; ca.view = m_swapChain->currentTextureView();
    ca.loadOp = rhi::LoadOp::Clear; ca.storeOp = rhi::StoreOp::Store;
    ca.clearValue = rhi::ClearColor(0.05f, 0.05f, 0.08f, 1.0f);
    rhi::DepthStencilAttachment dsa{}; dsa.view = m_depthBuf.view;
    dsa.depthLoadOp = rhi::LoadOp::Clear; dsa.depthStoreOp = rhi::StoreOp::Store; dsa.depthClearValue = 1.0f;
    rhi::RenderPassDesc rpd{}; rpd.colorAttachments.push_back(ca); rpd.depthStencilAttachment = dsa;
    auto* rp = enc->beginRenderPass(rpd);
    rp->setPipeline(m_renPipe); rp->setBindGroup(0, m_renBg);
    rp->setViewport(0, 0, static_cast<f32>(m_width), static_cast<f32>(m_height), 0, 1);
    rp->setScissor(0, 0, m_width, m_height);
    rp->setVertexBuffer(0, m_vtxBuf, 0);
    rp->draw(kNumPts); rp->end();

    enc->transitionTexture(m_swapChain->currentTexture(), rhi::ResourceState::RenderTarget, rhi::ResourceState::Present);
    rhi::CommandBuffer* cb = enc->finish(); m_fenceVal++;
    rhi::CommandBuffer* cbs[1] = { cb };
    m_graphicsQueue->submit(std::span<rhi::CommandBuffer* const>(cbs, 1), m_fence, m_fenceVal);
    m_swapChain->present(m_graphicsQueue);
    m_pool->destroyEncoder(enc);
}

void ComputeSample::onShutdown() {
    m_depthBuf.destroy(m_device);
    if (m_fence) m_device->destroyFence(m_fence);
    if (m_pool) m_device->destroyCommandPool(m_pool);
    if (m_renPipe) m_device->destroyRenderPipeline(m_renPipe);
    if (m_renPl) m_device->destroyPipelineLayout(m_renPl);
    if (m_renBg) m_device->destroyBindGroup(m_renBg);
    if (m_renBgl) m_device->destroyBindGroupLayout(m_renBgl);
    if (m_compPipe) m_device->destroyComputePipeline(m_compPipe);
    if (m_compPl) m_device->destroyPipelineLayout(m_compPl);
    if (m_compBg) m_device->destroyBindGroup(m_compBg);
    if (m_compBgl) m_device->destroyBindGroupLayout(m_compBgl);
    if (m_vpBuf) m_device->destroyBuffer(m_vpBuf);
    if (m_paramsBuf) m_device->destroyBuffer(m_paramsBuf);
    if (m_vtxBuf) m_device->destroyBuffer(m_vtxBuf);
    if (m_ps) m_device->destroyShaderModule(m_ps);
    if (m_vs) m_device->destroyShaderModule(m_vs);
    if (m_cs) m_device->destroyShaderModule(m_cs);
    if (m_compiler) { m_compiler->destroy(); delete m_compiler; }
}

int main(int argc, char** argv) { ComputeSample app; return app.run(argc, argv); }
