module;

#include <vector>
#include <cstdint>
#include <cstring>
#include <print>
#include <algorithm>

#include <bgfx/bgfx.h>

module rendering.renderer;

import core.stdtypes;
import core.math.transform;
import rendering.rhi;
import rendering.rhi.uniform_registry;
import rendering.rendergraph;
import rendering.mesh;
import rendering.material;
import rendering.quad;

namespace draco::rendering::renderer
{
    static constexpr const char* MAIN_PASS = "MainPass";

    void init(u16 width, u16 height)
    {
        g_ctx.screenWidth = width;
        g_ctx.screenHeight = height;
    }

    void resize(u16 width, u16 height)
    {
        g_ctx.screenWidth = width;
        g_ctx.screenHeight = height;
    }

    void beginFrame(const Camera& cam)
    {
        rhi::beginFrame();

        g_ctx.mainCamera = cam;
        g_ctx.graph.reset();

        // Create main pass once per frame
        auto& pass = g_ctx.graph.addPass(MAIN_PASS);

        pass.view = 0;
        pass.framebuffer = rhi::InvalidFramebuffer;

        pass.width  = g_ctx.screenWidth;
        pass.height = g_ctx.screenHeight;

        pass.clearFlags = BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH;
        pass.clearColor = 0x303030ff;

        rhi::lookAt(pass.viewMatrix, cam.position.data(), cam.target.data(), cam.up.data());
        const f32 aspect = static_cast<f32>(g_ctx.screenWidth) / static_cast<f32>(std::max<u16>(g_ctx.screenHeight, 1));
        rhi::perspective(pass.projMatrix, cam.fov, aspect, cam.nearPlane, cam.farPlane);
    }

    static void buildUniforms(const material::Material& mat, std::vector<rhi::UniformBind>& out)
    {
        out.clear();
        out.reserve(mat.uniforms.size());

        for (const auto& u : mat.uniforms)
        {
            rhi::UniformBind bind{};

            bind.handle = rhi::getUniform(u.nameHash);

            bind.data = u.data;
            bind.num  = u.count;

            if (bind.handle == rhi::InvalidUniform)
            {
                std::println("[Renderer] Missing uniform hash: {}", u.nameHash);
                continue;
            }

            out.push_back(bind);
        }
    }

    void submitEntity(const rhi::RenderPacket& packet)
    {
        auto* pass = g_ctx.graph.getPass(MAIN_PASS);
        if (!pass) return;

        pass->packets.push_back(packet);
    }

    void submitRenderable(const draco::math::Transform& transform, const material::Material& material, mesh::MeshHandle mesh_id)
    {
        const auto* m = mesh::get(mesh_id);
        if (!m) return;

        rhi::RenderPacket p{};

        p.vertexBuffer = m->vbh;
        p.indexBuffer  = m->ibh;

        p.pipeline        = material.pipeline;
        p.textureHandle  = material.texture;
        p.textureUnit    = material.texture_unit;
        p.samplerUniform = material.sampler;

        buildUniforms(material, p.uniforms);

        transform.toMatrix(p.model);

        submitEntity(p);
    }

    void submitUI(quad::QuadRenderer& quad_renderer)
    {
        auto& ui_pass = g_ctx.graph.addPass("UIPass");

        ui_pass.view = 1;
        ui_pass.sortMode = rendergraph::SortMode::None;

        ui_pass.framebuffer = rhi::InvalidFramebuffer;

        ui_pass.width  = g_ctx.screenWidth;
        ui_pass.height = g_ctx.screenHeight;

        ui_pass.clearFlags = 0;

        quad::OrthoCamera ortho;

        quad::QuadRenderer::buildOrtho(ortho, g_ctx.screenWidth, g_ctx.screenHeight);

        std::memcpy(ui_pass.viewMatrix, ortho.view, sizeof(f32) * 16);
        std::memcpy(ui_pass.projMatrix, ortho.proj, sizeof(f32) * 16);

        quad_renderer.flushToPass(ui_pass);
    }

    void endFrame()
    {
        g_ctx.graph.execute();
        rhi::endFrame();
    }

    rendergraph::RenderGraph& getGraph()
    {
        return g_ctx.graph;
    }
}
