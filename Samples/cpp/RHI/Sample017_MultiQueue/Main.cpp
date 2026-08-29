#include <new>
/// A compute shader generates an animated vertex grid on the compute queue,
/// then the graphics queue waits on the compute fence and renders the result.

#include <cmath>
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
using draco::math::Matrix4;

class MultiQueueSample : public sf::SampleApp {
public:
    using sf::SampleApp::SampleApp;
    std::u8string_view title() const override { return u8"Sample017 - MultiQueue (Async Compute)"; }
protected:
    draco::Status onInit() override;
    void onRender() override;
    void onResize(draco::u32 w, draco::u32 h) override { recreateDepth(w, h); }
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
            float fy = sin(dist*8.0 - Time*3.0) * 0.2;
            gVertices[idx].PosX = fx; gVertices[idx].PosY = fy; gVertices[idx].PosZ = fz;
            gVertices[idx].ColR = 0.5 + 0.5*sin(Time + fx*3.0);
            gVertices[idx].ColG = 0.5 + 0.5*cos(Time + fz*3.0);
            gVertices[idx].ColB = 0.5 + 0.5*sin(Time*0.7 + dist*4.0);
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

    void recreateDepth(draco::u32 w, draco::u32 h);

    shaders::Compiler* m_compiler = nullptr;
    rhi::ShaderModule *m_cs = nullptr, *m_vs = nullptr, *m_ps = nullptr;

    // Compute resources.
    rhi::Queue* m_computeQueue = nullptr;
    rhi::CommandPool *m_computePool = nullptr;
    rhi::BindGroupLayout *m_compBgl = nullptr; rhi::BindGroup *m_compBg = nullptr;
    rhi::PipelineLayout *m_compPl = nullptr; rhi::ComputePipeline *m_compPipe = nullptr;
    rhi::Buffer *m_paramsBuf = nullptr; void *m_paramsMapped = nullptr;

    // Graphics resources.
    rhi::CommandPool *m_gfxPool = nullptr;
    rhi::BindGroupLayout *m_renBgl = nullptr; rhi::BindGroup *m_renBg = nullptr;
    rhi::PipelineLayout *m_renPl = nullptr; rhi::RenderPipeline *m_renPipe = nullptr;
    rhi::Buffer *m_vpBuf = nullptr; void *m_vpMapped = nullptr;

    // Shared.
    rhi::Buffer *m_vtxBuf = nullptr;
    sf::DepthBuffer m_depthBuf;

    // Synchronization.
    rhi::Fence *m_compFence = nullptr, *m_gfxFence = nullptr;
    draco::u64 m_compFenceVal = 0, m_gfxFenceVal = 0;
    bool m_hasDedicatedCompute = false;
    float m_lastReportTime = 0.0f;
};

void MultiQueueSample::recreateDepth(draco::u32 w, draco::u32 h) {
    m_depthBuf.recreate(m_device, w, h);
}

draco::Status MultiQueueSample::onInit() {
    using draco::Status, std::span, draco::u8, draco::u32;

    // Check for dedicated compute queue.
    if (m_device->getQueueCount(rhi::QueueType::Compute) == 0) {
        m_computeQueue = m_graphicsQueue;
        m_hasDedicatedCompute = false;
        std::printf("No dedicated compute queue - using graphics queue for both\n");
    } else {
        m_computeQueue = m_device->getQueue(rhi::QueueType::Compute, 0);
        m_hasDedicatedCompute = true;
        std::printf("Using dedicated compute queue\n");
    }

    if (shaders::createCompiler(shaders::CompilerDesc{}, m_compiler) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (sf::compileToModule(m_compiler, m_device, kComputeSrc, shaders::ShaderStage::Compute,  u8"CSMain", u8"CS", m_cs) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (sf::compileToModule(m_compiler, m_device, kRenderSrc, shaders::ShaderStage::Vertex,   u8"VSMain", u8"VS", m_vs) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (sf::compileToModule(m_compiler, m_device, kRenderSrc, shaders::ShaderStage::Fragment, u8"PSMain", u8"PS", m_ps) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Shared vertex/storage buffer.
    rhi::BufferDesc vbd{}; vbd.size = kBufSz; vbd.usage = rhi::BufferUsage::Storage | rhi::BufferUsage::Vertex; vbd.memory = rhi::MemoryLocation::GpuOnly;
    if (m_device->createBuffer(vbd, m_vtxBuf) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Compute params UBO.
    rhi::BufferDesc pbd{}; pbd.size = 16; pbd.usage = rhi::BufferUsage::Uniform; pbd.memory = rhi::MemoryLocation::CpuToGpu;
    if (m_device->createBuffer(pbd, m_paramsBuf) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    m_paramsMapped = m_paramsBuf->map();

    // View-projection UBO.
    rhi::BufferDesc vpd{}; vpd.size = 64; vpd.usage = rhi::BufferUsage::Uniform; vpd.memory = rhi::MemoryLocation::CpuToGpu;
    if (m_device->createBuffer(vpd, m_vpBuf) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    m_vpMapped = m_vpBuf->map();

    // Compute pipeline.
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

    // Render pipeline.
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
    rpd.depthStencil->depthWriteEnabled = true; rpd.depthStencil->depthCompare = rhi::CompareFunction::Less;
    if (m_device->createRenderPipeline(rpd, m_renPipe) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Command pools - one per queue type.
    if (m_device->createCommandPool(rhi::QueueType::Graphics, m_gfxPool) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    auto compPoolType = m_hasDedicatedCompute ? rhi::QueueType::Compute : rhi::QueueType::Graphics;
    if (m_device->createCommandPool(compPoolType, m_computePool) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    if (m_device->createFence(0, m_compFence) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (m_device->createFence(0, m_gfxFence) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    return draco::ErrorCode::Ok;
}

void MultiQueueSample::onRender() {
    using draco::u32, draco::f32, std::span;
    if (m_gfxFenceVal > 0) m_gfxFence->wait(m_gfxFenceVal, ~0ull);
    if (m_swapChain->acquireNextImage() != draco::ErrorCode::Ok) return;

    // Update compute params.
    u32 numPts = kNumPts;
    f32 params[4] = { m_totalTime, 0, 1.0f, 0 };
    std::memcpy(&params[1], &numPts, 4);
    std::memcpy(m_paramsMapped, params, 16);

    // Update VP.
    f32 aspect = static_cast<f32>(m_width) / static_cast<f32>(m_height);
    f32 camAngle = m_totalTime * 0.4f, camDist = 2.5f;
    Matrix4 view = Matrix4::lookAtRH(draco::math::Vector3{std::sin(camAngle)*camDist, 1.2f, std::cos(camAngle)*camDist}, draco::math::Vector3{ 0,0,0}, draco::math::Vector3{0,1,0});
    Matrix4 proj = Matrix4::perspectiveFovRH(draco::math::degToRad(45.0f), aspect, 0.1f, 100.0f);
    Matrix4 vp = view * proj;
    std::memcpy(m_vpMapped, vp.data(), 64);

    // === Compute pass on compute queue ===
    m_computePool->reset();
    rhi::CommandEncoder* cEnc = nullptr;
    if (m_computePool->createEncoder(cEnc) != draco::ErrorCode::Ok || !cEnc) return;

    rhi::BufferBarrier bb{}; bb.buffer = m_vtxBuf; bb.oldState = rhi::ResourceState::VertexBuffer; bb.newState = rhi::ResourceState::ShaderWrite;
    rhi::BarrierGroup bg1{}; bg1.bufferBarriers = std::span<const rhi::BufferBarrier>(&bb, 1);
    cEnc->barrier(bg1);
    auto* cp = cEnc->beginComputePass(u8"AsyncCompute");
    cp->setPipeline(m_compPipe); cp->setBindGroup(0, m_compBg);
    cp->dispatch((kNumPts + 63) / 64); cp->end();
    bb.oldState = rhi::ResourceState::ShaderWrite; bb.newState = rhi::ResourceState::VertexBuffer;
    cEnc->barrier(bg1);

    rhi::CommandBuffer* cCb = cEnc->finish(); m_compFenceVal++;
    rhi::CommandBuffer* cCbs[1] = { cCb };
    m_computeQueue->submit(std::span<rhi::CommandBuffer* const>(cCbs, 1), m_compFence, m_compFenceVal);
    m_computePool->destroyEncoder(cEnc);

    // === Graphics pass - waits on compute fence before executing ===
    m_gfxPool->reset();
    rhi::CommandEncoder* gEnc = nullptr;
    if (m_gfxPool->createEncoder(gEnc) != draco::ErrorCode::Ok || !gEnc) return;

    gEnc->transitionTexture(m_swapChain->currentTexture(), rhi::ResourceState::Undefined, rhi::ResourceState::RenderTarget);
    gEnc->transitionTexture(m_depthBuf.texture, rhi::ResourceState::Undefined, rhi::ResourceState::DepthStencilWrite);

    rhi::ColorAttachment ca{}; ca.view = m_swapChain->currentTextureView();
    ca.loadOp = rhi::LoadOp::Clear; ca.storeOp = rhi::StoreOp::Store;
    ca.clearValue = rhi::ClearColor(0.03f, 0.03f, 0.06f, 1.0f);
    rhi::DepthStencilAttachment dsa{}; dsa.view = m_depthBuf.view;
    dsa.depthLoadOp = rhi::LoadOp::Clear; dsa.depthStoreOp = rhi::StoreOp::Store; dsa.depthClearValue = 1.0f;
    rhi::RenderPassDesc rpd{}; rpd.colorAttachments.push_back(ca); rpd.depthStencilAttachment = dsa;
    auto* rp = gEnc->beginRenderPass(rpd);
    rp->setPipeline(m_renPipe); rp->setBindGroup(0, m_renBg);
    rp->setViewport(0, 0, static_cast<f32>(m_width), static_cast<f32>(m_height), 0, 1);
    rp->setScissor(0, 0, m_width, m_height);
    rp->setVertexBuffer(0, m_vtxBuf, 0);
    rp->draw(kNumPts); rp->end();

    gEnc->transitionTexture(m_swapChain->currentTexture(), rhi::ResourceState::RenderTarget, rhi::ResourceState::Present);
    rhi::CommandBuffer* gCb = gEnc->finish(); m_gfxFenceVal++;
    rhi::CommandBuffer* gCbs[1] = { gCb };

    // Submit graphics - wait on compute fence, signal graphics fence.
    rhi::Fence* waitFences[1] = { m_compFence };
    draco::u64 waitValues[1] = { m_compFenceVal };
    m_graphicsQueue->submit(std::span<rhi::CommandBuffer* const>(gCbs, 1),
                           std::span<rhi::Fence* const>(waitFences, 1),
                           std::span<const draco::u64>(waitValues, 1),
                           m_gfxFence, m_gfxFenceVal);
    m_swapChain->present(m_graphicsQueue);
    m_gfxPool->destroyEncoder(gEnc);

    if (m_totalTime - m_lastReportTime >= 3.0f) {
        std::printf("MultiQueue: compute fence=%llu, graphics fence=%llu, dt=%.2fms\n",
            static_cast<unsigned long long>(m_compFenceVal),
            static_cast<unsigned long long>(m_gfxFenceVal),
            m_deltaTime * 1000.0f);
        m_lastReportTime = m_totalTime;
    }
}

void MultiQueueSample::onShutdown() {
    if (m_paramsBuf && m_paramsMapped) m_paramsBuf->unmap();
    if (m_vpBuf && m_vpMapped) m_vpBuf->unmap();
    m_depthBuf.destroy(m_device);
    if (m_gfxFence) m_device->destroyFence(m_gfxFence); if (m_compFence) m_device->destroyFence(m_compFence);
    if (m_gfxPool) m_device->destroyCommandPool(m_gfxPool); if (m_computePool) m_device->destroyCommandPool(m_computePool);
    if (m_renPipe) m_device->destroyRenderPipeline(m_renPipe); if (m_renPl) m_device->destroyPipelineLayout(m_renPl);
    if (m_renBg) m_device->destroyBindGroup(m_renBg); if (m_renBgl) m_device->destroyBindGroupLayout(m_renBgl);
    if (m_compPipe) m_device->destroyComputePipeline(m_compPipe); if (m_compPl) m_device->destroyPipelineLayout(m_compPl);
    if (m_compBg) m_device->destroyBindGroup(m_compBg); if (m_compBgl) m_device->destroyBindGroupLayout(m_compBgl);
    if (m_vpBuf) m_device->destroyBuffer(m_vpBuf); if (m_paramsBuf) m_device->destroyBuffer(m_paramsBuf);
    if (m_vtxBuf) m_device->destroyBuffer(m_vtxBuf);
    if (m_ps) m_device->destroyShaderModule(m_ps); if (m_vs) m_device->destroyShaderModule(m_vs);
    if (m_cs) m_device->destroyShaderModule(m_cs);
    if (m_compiler) { m_compiler->destroy(); delete m_compiler; }
}

int main(int argc, char** argv) { MultiQueueSample app; return app.run(argc, argv); }
