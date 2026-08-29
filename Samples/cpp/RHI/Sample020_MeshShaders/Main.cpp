#include <new>
/// Demonstrates mesh shader pipeline: a rotating triangle generated entirely in the mesh shader.

#include <cstdio>
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

struct PushData {
    float time;
    float aspectRatio;
    float pad0;
    float pad1;
};

class MeshShaderSample : public sf::SampleApp {
public:
    using sf::SampleApp::SampleApp;
    std::u8string_view title() const override { return u8"Sample020 - Mesh Shaders (Rotating Triangle)"; }
protected:
    rhi::DeviceFeatures requiredFeatures() const override {
        rhi::DeviceFeatures f{};
        f.meshShaders = true;
        return f;
    }
    draco::Status onInit() override;
    void onRender() override;
    void onShutdown() override;
private:
    static constexpr const char8_t kMeshShaderSource[] = u8R"(
        struct PushConstants
        {
            float Time;
            float AspectRatio;
            float Pad0, Pad1;
        };

        [[vk::push_constant]] ConstantBuffer<PushConstants> pc : register(b0, space0);

        struct MeshOutput
        {
            float4 Position : SV_POSITION;
            float3 Color    : TEXCOORD0;
        };

        [outputtopology("triangle")]
        [numthreads(1, 1, 1)]
        void MSMain(out vertices MeshOutput verts[3], out indices uint3 tris[1])
        {
            SetMeshOutputCounts(3, 1);

            float angle = pc.Time * 0.5;
            float c = cos(angle);
            float s = sin(angle);

            float2 positions[3] = {
                float2( 0.0,  0.5),
                float2(-0.5, -0.5),
                float2( 0.5, -0.5)
            };

            float3 colors[3] = {
                float3(1.0, 0.0, 0.0),
                float3(0.0, 1.0, 0.0),
                float3(0.0, 0.0, 1.0)
            };

            for (uint i = 0; i < 3; i++)
            {
                float2 p = positions[i];
                float2 rotated = float2(p.x * c - p.y * s, p.x * s + p.y * c);
                rotated.x /= pc.AspectRatio;

                verts[i].Position = float4(rotated, 0.0, 1.0);
                verts[i].Color = colors[i];
            }

            tris[0] = uint3(0, 1, 2);
        }
    )";

    static constexpr const char8_t kFragmentShaderSource[] = u8R"(
        struct PSInput
        {
            float4 Position : SV_POSITION;
            float3 Color    : TEXCOORD0;
        };

        float4 PSMain(PSInput input) : SV_TARGET
        {
            return float4(input.Color, 1.0);
        }
    )";

    shaders::Compiler* m_compiler = nullptr;
    rhi::ShaderModule* m_meshModule = nullptr;
    rhi::ShaderModule* m_fragModule = nullptr;
    rhi::PipelineLayout* m_pipelineLayout = nullptr;
    rhi::MeshPipeline* m_meshPipeline = nullptr;
    rhi::CommandPool* m_pool = nullptr;
    rhi::Fence* m_fence = nullptr;
    draco::u64 m_fenceVal = 0;
};

draco::Status MeshShaderSample::onInit() {
    using draco::Status, std::span;

    // Check mesh shader support.
    if (!m_device->features.meshShaders) {
        std::fprintf(stderr, "ERROR: Mesh shaders are not supported by this device/backend\n");
        return draco::ErrorCode::Unknown;
    }

    // Shader compiler.
    if (shaders::createCompiler(shaders::CompilerDesc{}, m_compiler) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Compile mesh shader (SM 6.5 required for mesh shaders).
    if (sf::compileToModule(m_compiler, m_device, kMeshShaderSource, shaders::ShaderStage::Mesh,
                            u8"MSMain", u8"MeshShader", u8"6_5", m_meshModule) != draco::ErrorCode::Ok)
        return draco::ErrorCode::Unknown;

    // Compile fragment shader.
    if (sf::compileToModule(m_compiler, m_device, kFragmentShaderSource, shaders::ShaderStage::Fragment,
                            u8"PSMain", u8"FragmentShader", m_fragModule) != draco::ErrorCode::Ok)
        return draco::ErrorCode::Unknown;

    // Pipeline layout with push constants.
    rhi::PushConstantRange pushRange{};
    pushRange.stages = rhi::ShaderStage::Mesh;
    pushRange.offset = 0;
    pushRange.size   = sizeof(PushData);

    rhi::PipelineLayoutDesc pld{};
    pld.pushConstantRanges = std::span<const rhi::PushConstantRange>(&pushRange, 1);
    pld.label = u8"MeshPipelineLayout";
    if (m_device->createPipelineLayout(pld, m_pipelineLayout) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Create mesh pipeline.
    rhi::ColorTargetState ct{};
    ct.format   = m_swapChain->format();
    ct.writeMask = rhi::ColorWriteMask::All;

    rhi::MeshPipelineDesc mpd{};
    mpd.layout       = m_pipelineLayout;
    mpd.mesh         = { m_meshModule, u8"MSMain", rhi::ShaderStage::Mesh };
    mpd.fragment     = rhi::FragmentState{};
    mpd.fragment->shader  = { m_fragModule, u8"PSMain", rhi::ShaderStage::Fragment };
    mpd.fragment->targets = std::span<const rhi::ColorTargetState>(&ct, 1);
    mpd.colorTargets = std::span<const rhi::ColorTargetState>(&ct, 1);
    mpd.label        = u8"MeshShaderPipeline";
    if (m_device->createMeshPipeline(mpd, m_meshPipeline) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    // Command pool and fence.
    if (m_device->createCommandPool(rhi::QueueType::Graphics, m_pool) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;
    if (m_device->createFence(0, m_fence) != draco::ErrorCode::Ok) return draco::ErrorCode::Unknown;

    return draco::ErrorCode::Ok;
}

void MeshShaderSample::onRender() {
    using draco::f32, std::span;

    if (m_fenceVal > 0) m_fence->wait(m_fenceVal, ~0ull);
    if (m_swapChain->acquireNextImage() != draco::ErrorCode::Ok) return;

    m_pool->reset();
    rhi::CommandEncoder* enc = nullptr;
    if (m_pool->createEncoder(enc) != draco::ErrorCode::Ok || !enc) return;

    // Barrier: present -> render target.
    enc->transitionTexture(m_swapChain->currentTexture(),
                           rhi::ResourceState::Undefined, rhi::ResourceState::RenderTarget);

    // Begin render pass.
    rhi::ColorAttachment ca{};
    ca.view       = m_swapChain->currentTextureView();
    ca.loadOp     = rhi::LoadOp::Clear;
    ca.storeOp    = rhi::StoreOp::Store;
    ca.clearValue = rhi::ClearColor(0.05f, 0.05f, 0.08f, 1.0f);

    rhi::RenderPassDesc rpd{};
    rpd.colorAttachments.push_back(ca);
    auto* rp = enc->beginRenderPass(rpd);

    rp->setViewport(0, 0, static_cast<f32>(m_width), static_cast<f32>(m_height), 0.0f, 1.0f);
    rp->setScissor(0, 0, m_width, m_height);

    // Draw with mesh shader.
    if (auto* meshPass = rp->asMeshShaderExt()) {
        meshPass->setMeshPipeline(m_meshPipeline);

        // Push constants (must be after pipeline is bound).
        PushData pushData{};
        pushData.time        = m_totalTime;
        pushData.aspectRatio = static_cast<float>(m_width) / static_cast<float>(m_height);
        pushData.pad0        = 0.0f;
        pushData.pad1        = 0.0f;
        rp->setPushConstants(rhi::ShaderStage::Mesh, 0, sizeof(PushData), &pushData);

        meshPass->drawMeshTasks(1);
    }

    rp->end();

    // Barrier: render target -> present.
    enc->transitionTexture(m_swapChain->currentTexture(),
                           rhi::ResourceState::RenderTarget, rhi::ResourceState::Present);

    rhi::CommandBuffer* cb = enc->finish();
    m_fenceVal++;
    rhi::CommandBuffer* cbs[1] = { cb };
    m_graphicsQueue->submit(std::span<rhi::CommandBuffer* const>(cbs, 1), m_fence, m_fenceVal);
    m_swapChain->present(m_graphicsQueue);
    m_pool->destroyEncoder(enc);
}

void MeshShaderSample::onShutdown() {
    if (m_fence) m_device->destroyFence(m_fence);
    if (m_pool) m_device->destroyCommandPool(m_pool);
    if (m_meshPipeline) m_device->destroyMeshPipeline(m_meshPipeline);
    if (m_pipelineLayout) m_device->destroyPipelineLayout(m_pipelineLayout);
    if (m_fragModule) m_device->destroyShaderModule(m_fragModule);
    if (m_meshModule) m_device->destroyShaderModule(m_meshModule);
    if (m_compiler) { m_compiler->destroy(); delete m_compiler; }
}

int main(int argc, char** argv) { MeshShaderSample app; return app.run(argc, argv); }
