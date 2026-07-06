#include <print>
#include <cmath>
#include <chrono>

#include <bx/math.h>

import draconic;

using namespace draco::shell;

namespace
{
    // Map the shell's window system onto the RHI's native window type so bgfx
    // interprets the handles correctly (only Wayland needs explicit flagging).
    draco::rendering::rhi::NativeWindowType toRhiWindowType(WindowSystem sys)
    {
        using RhiType = draco::rendering::rhi::NativeWindowType;
        switch (sys)
        {
            case WindowSystem::Win32:   return RhiType::Win32;
            case WindowSystem::X11:     return RhiType::X11;
            case WindowSystem::Wayland: return RhiType::Wayland;
            case WindowSystem::Cocoa:   return RhiType::Cocoa;
            default:                    return RhiType::Default;
        }
    }
}

int main(int, char*[])
{
    auto shell = createShell(WindowSettings{
        .title = u8"Draconic Engine Rendering Sample",
        .width = 1280,
        .height = 720,
    });

    IWindow* mainWindow = shell->mainWindow();
    if (mainWindow == nullptr)
    {
        std::println("Failed to create shell window");
        destroyShell(shell);
        return -1;
    }

    // Capture the mouse for camera control.
    shell->input()->mouse()->setRelativeMode(true);

    const NativeWindow native = mainWindow->native();
    const draco::u32 startW = mainWindow->width();
    const draco::u32 startH = mainWindow->height();

    if (!draco::rendering::rhi::init(native.display, native.window, toRhiWindowType(native.system),
                                     static_cast<draco::u16>(startW), static_cast<draco::u16>(startH)))
    {
        std::println("RHI init failed");
        destroyShell(shell);
        return -1;
    }

    draco::rendering::renderer::init(startW, startH);

    auto cube_mesh = draco::rendering::mesh::createCube();
    auto plane_mesh = draco::rendering::mesh::createPlane(5.0f);
    auto sphere_mesh = draco::rendering::mesh::createSphere(24, 16);
    auto cylinder_mesh = draco::rendering::mesh::createCylinder(24, 2.0f);
    auto capsule_mesh = draco::rendering::mesh::createCapsule(24, 12, 2.0f);

    auto img = draco::core::io::loader::image::loadImage("test.png");

    draco::rendering::rhi::TextureHandle tex = draco::rendering::rhi::InvalidTexture;

    if (img.isValid) {
        tex = draco::rendering::rhi::createTexture(img.pixels.data(), img.width, img.height);
    }

    auto s_texColor = draco::rendering::rhi::createUniform("s_texColor", draco::rendering::rhi::UniformType::Sampler);

    auto vs = draco::core::io::filesystem::loadBinary("vs.bin");
    auto fs = draco::core::io::filesystem::loadBinary("fs.bin");

    auto vs_quad = draco::core::io::filesystem::loadBinary("vs_quad.bin");
    auto fs_quad = draco::core::io::filesystem::loadBinary("fs_quad.bin");

    if (vs.empty() || fs.empty() || vs_quad.empty() || fs_quad.empty()) {
        std::println("Shader load failed");
        draco::rendering::rhi::shutdown();
        destroyShell(shell);
        return -1;
    }

    auto vsh = draco::rendering::rhi::createShader(vs.data(), (draco::u32)vs.size());
    auto fsh = draco::rendering::rhi::createShader(fs.data(), (draco::u32)fs.size());

    auto vsh_quad = draco::rendering::rhi::createShader(vs_quad.data(), (draco::u32)vs_quad.size());
    auto fsh_quad = draco::rendering::rhi::createShader(fs_quad.data(), (draco::u32)fs_quad.size());

    auto pipeline = draco::rendering::rhi::createPipeline({vsh, fsh, draco::rendering::rhi::PipelineState::WriteRGB | draco::rendering::rhi::PipelineState::WriteAlpha | draco::rendering::rhi::PipelineState::MSAA, draco::rendering::rhi::BlendMode::None, draco::rendering::rhi::DepthTest::Less, draco::rendering::rhi::CullMode::CCW, true});

    auto pipeline_quad = draco::rendering::rhi::createPipeline({vsh_quad, fsh_quad, draco::rendering::rhi::PipelineState::WriteRGB | draco::rendering::rhi::PipelineState::WriteAlpha | draco::rendering::rhi::PipelineState::MSAA, draco::rendering::rhi::BlendMode::Alpha, draco::rendering::rhi::DepthTest::None, draco::rendering::rhi::CullMode::None, true});

    draco::rendering::quad::QuadRenderer quad_renderer;
    quad_renderer.init(pipeline_quad);

    draco::scene::CameraController camera;
    camera.init();

    auto u_tint   = draco::rendering::rhi::createUniform("u_tint",   draco::rendering::rhi::UniformType::Vec4);
    auto u_offset = draco::rendering::rhi::createUniform("u_offset", draco::rendering::rhi::UniformType::Vec4);

    draco::rendering::rhi::registerUniform(draco::rendering::rhi::hashUniform("u_tint"), u_tint);

    draco::rendering::rhi::registerUniform(draco::rendering::rhi::hashUniform("u_offset"), u_offset);

    draco::f32 tint[4]   = {1,1,1,1};
    draco::f32 offset[4] = {0,0,0,0};

    bool mouse_captured = true;

    draco::rendering::material::Material mat{};
    mat.pipeline = pipeline;
    mat.texture = tex;
    mat.sampler = s_texColor;

    mat.uniforms.push_back({.nameHash = draco::rendering::rhi::hashUniform("u_tint"), .data = tint, .count = 1});
    mat.uniforms.push_back({.nameHash = draco::rendering::rhi::hashUniform("u_offset"), .data = offset, .count = 1});

    draco::scene::Scene scene;

    static constexpr draco::math::Transform tr;

    scene.renderables.push_back({cube_mesh, tr, mat});
    scene.renderables.push_back({plane_mesh, tr, mat});
    scene.renderables.push_back({sphere_mesh, tr, mat});
    scene.renderables.push_back({cylinder_mesh, tr, mat});
    scene.renderables.push_back({capsule_mesh, tr, mat});

    scene.renderables[0].transform.setPosition(-12.0f, 0.0f, 0.0f);
    scene.renderables[1].transform.setPosition(-6.0f, 0.0f, 0.0f);
    scene.renderables[2].transform.setPosition(0.0f, 0.0f, 0.0f);
    scene.renderables[3].transform.setPosition(6.0f, 0.0f, 0.0f);
    scene.renderables[4].transform.setPosition(12.0f, 0.0f, 0.0f);

    scene.renderables[1].transform.setRotation(-bx::kPiHalf, 0.0f, 0.0f);

    using clock = std::chrono::steady_clock;
    const auto startTime = clock::now();
    auto lastTime = startTime;

    while (shell->isRunning())
    {
        shell->processEvents();

        const auto nowTime = clock::now();
        const draco::f32 dt = std::chrono::duration<draco::f32>(nowTime - lastTime).count();
        lastTime = nowTime;
        const draco::f32 elapsed = std::chrono::duration<draco::f32>(nowTime - startTime).count();

        IInputManager* input = shell->input();
        IKeyboard* kb = input->keyboard();
        IMouse* ms = input->mouse();

        // Toggle mouse capture on Escape (edge-triggered).
        if (kb->isKeyPressed(KeyCode::Escape))
        {
            mouse_captured = !mouse_captured;
            ms->setRelativeMode(mouse_captured);
        }

        const draco::u32 w = mainWindow->width();
        const draco::u32 h = mainWindow->height();

        if (w == 0 || h == 0 || mainWindow->isMinimized())
        {
            continue;
        }

        draco::rendering::rhi::resize((draco::u16)w, (draco::u16)h);
        draco::rendering::renderer::resize((draco::u16)w, (draco::u16)h);

        draco::scene::CameraInput camInput{};
        camInput.mouseDx      = ms->deltaX();
        camInput.mouseDy      = ms->deltaY();
        camInput.moveForward  = kb->isKeyDown(KeyCode::W);
        camInput.moveBackward = kb->isKeyDown(KeyCode::S);
        camInput.moveLeft     = kb->isKeyDown(KeyCode::A);
        camInput.moveRight    = kb->isKeyDown(KeyCode::D);
        camera.update(dt, camInput);

        auto cam = camera.getCamera();

        draco::rendering::renderer::beginFrame(cam);

        for (const auto& renderable : scene.renderables)
        {
            draco::rendering::renderer::submitRenderable(renderable.transform,  renderable.material,  renderable.mesh);
        }

        quad_renderer.begin();

        static draco::f32 quad_base_x = 400.0f;
        static draco::f32 quad_base_y = 300.0f;

        for (int i = 0; i < 50; i++)
        {
            draco::rendering::quad::QuadCommand q{};

            q.texture = tex;
            q.color = 0xffffffff;
            q.x = quad_base_x + std::sin(elapsed + i) * 50.0f;
            q.y = quad_base_y + i * 6.0f;
            q.width = 50.0f;
            q.height = 50.0f;
            q.rotation = elapsed;

            quad_renderer.submit(q);
        }

        draco::rendering::renderer::submitUI(quad_renderer);

        draco::rendering::renderer::endFrame();
    }

    draco::rendering::rhi::shutdown();
    destroyShell(shell);   // destroys the window and shuts SDL down

    return 0;
}
