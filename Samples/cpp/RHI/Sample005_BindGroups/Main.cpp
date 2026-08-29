#include <new>
/// Sample005 - Multiple Bind Groups with Dynamic Offsets.
/// 4x4 grid of lit cubes, each with unique color via dynamic offset.

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

class BindGroupSample : public sf::SampleApp {
public:
    using sf::SampleApp::SampleApp;
    std::u8string_view title() const override { return u8"Sample005 - Multiple Bind Groups"; }
protected:
    draco::Status onInit() override;
    void onRender() override;
    void onResize(draco::u32 w, draco::u32 h) override { m_depthBuf.recreate(m_device, w, h); }
    void onShutdown() override;
private:
    static constexpr const char8_t kShader[] = u8R"(
        cbuffer GlobalUBO : register(b0, space0) { row_major float4x4 VP; };
        cbuffer ObjectUBO : register(b0, space1) { row_major float4x4 Model; float4 ObjColor; };
        struct VSInput { float3 Position : TEXCOORD0; float3 Normal : TEXCOORD1; };
        struct PSInput { float4 Position : SV_POSITION; float3 Normal : NORMAL; float4 Color : COLOR; };
        PSInput VSMain(VSInput i) {
            PSInput o;
            float4 wp = mul(float4(i.Position, 1.0), Model);
            o.Position = mul(wp, VP);
            o.Normal = mul(i.Normal, (float3x3)Model);
            o.Color = ObjColor;
            return o;
        }
        float4 PSMain(PSInput i) : SV_TARGET {
            float3 ld = normalize(float3(0.5, 1.0, -0.7));
            float ndotl = max(dot(normalize(i.Normal), ld), 0.0);
            return float4(i.Color.rgb * (0.2 + 0.8 * ndotl), 1.0);
        }
    )";

    static constexpr int kGrid = 4, kObjCount = kGrid * kGrid;
    static constexpr draco::u32 kObjStride = 256; // DX12 CBV alignment

    // Cube with face normals (24 verts, 36 indices).
    struct Vert { float px,py,pz, nx,ny,nz; };
    static constexpr Vert kCubeV[24] = {
        {-.5f,-.5f,-.5f, 0,0,-1},{.5f,-.5f,-.5f, 0,0,-1},{.5f,.5f,-.5f, 0,0,-1},{-.5f,.5f,-.5f, 0,0,-1},
        {.5f,-.5f,.5f, 0,0,1},{-.5f,-.5f,.5f, 0,0,1},{-.5f,.5f,.5f, 0,0,1},{.5f,.5f,.5f, 0,0,1},
        {-.5f,-.5f,.5f,-1,0,0},{-.5f,-.5f,-.5f,-1,0,0},{-.5f,.5f,-.5f,-1,0,0},{-.5f,.5f,.5f,-1,0,0},
        {.5f,-.5f,-.5f,1,0,0},{.5f,-.5f,.5f,1,0,0},{.5f,.5f,.5f,1,0,0},{.5f,.5f,-.5f,1,0,0},
        {-.5f,.5f,-.5f,0,1,0},{.5f,.5f,-.5f,0,1,0},{.5f,.5f,.5f,0,1,0},{-.5f,.5f,.5f,0,1,0},
        {-.5f,-.5f,.5f,0,-1,0},{.5f,-.5f,.5f,0,-1,0},{.5f,-.5f,-.5f,0,-1,0},{-.5f,-.5f,-.5f,0,-1,0},
    };
    static constexpr draco::u16 kCubeI[36] = {
        0,1,2,0,2,3, 4,5,6,4,6,7, 8,9,10,8,10,11, 12,13,14,12,14,15, 16,17,18,16,18,19, 20,21,22,20,22,23
    };

    shaders::Compiler* m_compiler = nullptr;
    rhi::ShaderModule *m_vs = nullptr, *m_ps = nullptr;
    rhi::Buffer *m_vb = nullptr, *m_ib = nullptr, *m_globalUbo = nullptr, *m_objUbo = nullptr;
    void *m_globalMapped = nullptr, *m_objMapped = nullptr;
    rhi::BindGroupLayout *m_globalBgl = nullptr, *m_objBgl = nullptr;
    rhi::BindGroup *m_globalBg = nullptr, *m_objBg = nullptr;
    rhi::PipelineLayout *m_pl = nullptr;
    rhi::RenderPipeline *m_pipeline = nullptr;
    rhi::CommandPool *m_pool = nullptr; rhi::Fence *m_fence = nullptr;
    draco::u64 m_fenceVal = 0;
    sf::DepthBuffer m_depthBuf;
};

draco::Status BindGroupSample::onInit() {
    using draco::Status, std::span, draco::u8, draco::u32;
    if (shaders::createCompiler(shaders::CompilerDesc{}, m_compiler) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (sf::compileToModule(m_compiler, m_device, kShader, shaders::ShaderStage::Vertex,   u8"VSMain", u8"VS", m_vs) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (sf::compileToModule(m_compiler, m_device, kShader, shaders::ShaderStage::Fragment, u8"PSMain", u8"PS", m_ps) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Buffers.
    rhi::BufferDesc vbd{}; vbd.size = sizeof(kCubeV); vbd.usage = rhi::BufferUsage::Vertex | rhi::BufferUsage::CopyDst; vbd.memory = rhi::MemoryLocation::GpuOnly;
    if (m_device->createBuffer(vbd, m_vb) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    rhi::BufferDesc ibd{}; ibd.size = sizeof(kCubeI); ibd.usage = rhi::BufferUsage::Index | rhi::BufferUsage::CopyDst; ibd.memory = rhi::MemoryLocation::GpuOnly;
    if (m_device->createBuffer(ibd, m_ib) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    rhi::TransferBatch* batch = nullptr; m_graphicsQueue->createTransferBatch(batch);
    batch->writeBuffer(m_vb, 0, std::span<const u8>(reinterpret_cast<const u8*>(kCubeV), sizeof(kCubeV)));
    batch->writeBuffer(m_ib, 0, std::span<const u8>(reinterpret_cast<const u8*>(kCubeI), sizeof(kCubeI)));
    batch->submit(); m_graphicsQueue->destroyTransferBatch(batch);

    rhi::BufferDesc gbd{}; gbd.size = 256; gbd.usage = rhi::BufferUsage::Uniform; gbd.memory = rhi::MemoryLocation::CpuToGpu;
    if (m_device->createBuffer(gbd, m_globalUbo) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    m_globalMapped = m_globalUbo->map();
    rhi::BufferDesc obd{}; obd.size = kObjCount * kObjStride; obd.usage = rhi::BufferUsage::Uniform; obd.memory = rhi::MemoryLocation::CpuToGpu;
    if (m_device->createBuffer(obd, m_objUbo) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    m_objMapped = m_objUbo->map();

    // Set 0: global VP.
    rhi::BindGroupLayoutEntry gE[1] = { rhi::BindGroupLayoutEntry::uniformBuffer(0, rhi::ShaderStage::Vertex) };
    rhi::BindGroupLayoutDesc gBgld{}; gBgld.entries = std::span<const rhi::BindGroupLayoutEntry>(gE, 1);
    if (m_device->createBindGroupLayout(gBgld, m_globalBgl) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    rhi::BindGroupEntry gBgE[1] = { rhi::BindGroupEntry::bufferEntry(m_globalUbo, 0, 64) };
    rhi::BindGroupDesc gBgd{}; gBgd.layout = m_globalBgl; gBgd.entries = std::span<const rhi::BindGroupEntry>(gBgE, 1);
    if (m_device->createBindGroup(gBgd, m_globalBg) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Set 1: per-object with dynamic offset.
    rhi::BindGroupLayoutEntry oE[1] = { rhi::BindGroupLayoutEntry::uniformBuffer(0, rhi::ShaderStage::Vertex | rhi::ShaderStage::Fragment) };
    oE[0].hasDynamicOffset = true;
    rhi::BindGroupLayoutDesc oBgld{}; oBgld.entries = std::span<const rhi::BindGroupLayoutEntry>(oE, 1);
    if (m_device->createBindGroupLayout(oBgld, m_objBgl) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    rhi::BindGroupEntry oBgE[1] = { rhi::BindGroupEntry::bufferEntry(m_objUbo, 0, kObjStride) };
    rhi::BindGroupDesc oBgd{}; oBgd.layout = m_objBgl; oBgd.entries = std::span<const rhi::BindGroupEntry>(oBgE, 1);
    if (m_device->createBindGroup(oBgd, m_objBg) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Pipeline layout with 2 sets.
    rhi::BindGroupLayout* sets[2] = { m_globalBgl, m_objBgl };
    rhi::PipelineLayoutDesc pld{}; pld.bindGroupLayouts = std::span<rhi::BindGroupLayout* const>(sets, 2);
    if (m_device->createPipelineLayout(pld, m_pl) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    m_depthBuf.recreate(m_device, m_width, m_height);

    rhi::VertexAttribute attrs[2] = { {rhi::VertexFormat::Float32x3, 0, 0}, {rhi::VertexFormat::Float32x3, 12, 1} };
    rhi::VertexBufferLayout vbl{}; vbl.stride = 24; vbl.attributes = std::span<const rhi::VertexAttribute>(attrs, 2);
    rhi::ColorTargetState ct{}; ct.format = m_swapChain->format();
    rhi::RenderPipelineDesc rpd{}; rpd.layout = m_pl;
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

void BindGroupSample::onRender() {
    using draco::f32, draco::u32, std::span;
    if (m_fenceVal > 0) m_fence->wait(m_fenceVal, ~0ull);
    if (m_swapChain->acquireNextImage() != draco::ErrorCode::Ok) return;

    // Update VP.
    f32 aspect = static_cast<f32>(m_width) / static_cast<f32>(m_height);
    f32 camAngle = m_totalTime * 0.3f, camDist = 8.0f;
    Matrix4 view = Matrix4::lookAtRH(draco::math::Vector3{std::sin(camAngle)*camDist, 5.0f, -std::cos(camAngle)*camDist}, draco::math::Vector3{ 0,0,0}, draco::math::Vector3{0,1,0});
    Matrix4 proj = Matrix4::perspectiveFovRH(draco::math::degToRad(45.0f), aspect, 0.1f, 100.0f);
    Matrix4 vp = view * proj;
    std::memcpy(m_globalMapped, vp.data(), 64);

    // Update per-object.
    static constexpr f32 kColors[kObjCount * 4] = {
        1,.3f,.3f,1, .3f,1,.3f,1, .3f,.3f,1,1, 1,1,.3f,1, 1,.3f,1,1, .3f,1,1,1, 1,.6f,.2f,1, .6f,.2f,1,1,
        .2f,.8f,.6f,1, .8f,.8f,.8f,1, .5f,.3f,.1f,1, .9f,.5f,.7f,1, .4f,.7f,.2f,1, .2f,.4f,.8f,1, .8f,.4f,.4f,1, .6f,.6f,.3f,1
    };
    f32 spacing = 2.0f, half = (kGrid - 1) * spacing * 0.5f;
    for (int r = 0; r < kGrid; ++r) for (int c = 0; c < kGrid; ++c) {
        int idx = r * kGrid + c;
        f32 angle = m_totalTime * (0.5f + idx * 0.1f);
        Matrix4 model = Matrix4::rotationY(angle);
        model.m[3][0] = c * spacing - half; model.m[3][1] = 0; model.m[3][2] = r * spacing - half;
        auto* dest = static_cast<draco::u8*>(m_objMapped) + idx * kObjStride;
        std::memcpy(dest, model.data(), 64);
        std::memcpy(dest + 64, &kColors[idx * 4], 16);
    }

    m_pool->reset();
    rhi::CommandEncoder* enc = nullptr;
    if (m_pool->createEncoder(enc) != draco::ErrorCode::Ok || !enc) return;
    enc->transitionTexture(m_swapChain->currentTexture(), rhi::ResourceState::Undefined, rhi::ResourceState::RenderTarget);
    enc->transitionTexture(m_depthBuf.texture, rhi::ResourceState::Undefined, rhi::ResourceState::DepthStencilWrite);

    rhi::ColorAttachment ca{}; ca.view = m_swapChain->currentTextureView();
    ca.loadOp = rhi::LoadOp::Clear; ca.storeOp = rhi::StoreOp::Store; ca.clearValue = rhi::ClearColor(0.08f, 0.08f, 0.12f, 1);
    rhi::DepthStencilAttachment dsa{}; dsa.view = m_depthBuf.view;
    dsa.depthLoadOp = rhi::LoadOp::Clear; dsa.depthStoreOp = rhi::StoreOp::Store; dsa.depthClearValue = 1.0f;
    rhi::RenderPassDesc rpd{}; rpd.colorAttachments.push_back(ca); rpd.depthStencilAttachment = dsa;
    auto* rp = enc->beginRenderPass(rpd);
    rp->setPipeline(m_pipeline);
    rp->setViewport(0, 0, static_cast<f32>(m_width), static_cast<f32>(m_height), 0, 1);
    rp->setScissor(0, 0, m_width, m_height);
    rp->setVertexBuffer(0, m_vb, 0);
    rp->setIndexBuffer(m_ib, rhi::IndexFormat::UInt16, 0);
    rp->setBindGroup(0, m_globalBg);
    for (int i = 0; i < kObjCount; ++i) {
        u32 dynOff = static_cast<u32>(i * kObjStride);
        rp->setBindGroup(1, m_objBg, std::span<const u32>(&dynOff, 1));
        rp->drawIndexed(36);
    }
    rp->end();
    enc->transitionTexture(m_swapChain->currentTexture(), rhi::ResourceState::RenderTarget, rhi::ResourceState::Present);

    rhi::CommandBuffer* cb = enc->finish(); m_fenceVal++;
    rhi::CommandBuffer* cbs[1] = { cb };
    m_graphicsQueue->submit(std::span<rhi::CommandBuffer* const>(cbs, 1), m_fence, m_fenceVal);
    m_swapChain->present(m_graphicsQueue); m_pool->destroyEncoder(enc);
}

void BindGroupSample::onShutdown() {
    m_depthBuf.destroy(m_device);
    if (m_fence) m_device->destroyFence(m_fence); if (m_pool) m_device->destroyCommandPool(m_pool);
    if (m_pipeline) m_device->destroyRenderPipeline(m_pipeline); if (m_pl) m_device->destroyPipelineLayout(m_pl);
    if (m_objBg) m_device->destroyBindGroup(m_objBg); if (m_objBgl) m_device->destroyBindGroupLayout(m_objBgl);
    if (m_globalBg) m_device->destroyBindGroup(m_globalBg); if (m_globalBgl) m_device->destroyBindGroupLayout(m_globalBgl);
    if (m_objUbo) m_device->destroyBuffer(m_objUbo); if (m_globalUbo) m_device->destroyBuffer(m_globalUbo);
    if (m_ib) m_device->destroyBuffer(m_ib); if (m_vb) m_device->destroyBuffer(m_vb);
    if (m_ps) m_device->destroyShaderModule(m_ps); if (m_vs) m_device->destroyShaderModule(m_vs);
    if (m_compiler) { m_compiler->destroy(); delete m_compiler; }
}

int main(int argc, char** argv) { BindGroupSample app; return app.run(argc, argv); }
