export module scene.camera.controller;

import core.stdtypes;
import rendering;

export namespace draco::scene
{
    // Per-frame input for the camera, supplied by the caller. Keeps the
    // controller decoupled from any specific input/shell backend.
    struct CameraInput
    {
        f32 mouseDx = 0.0f;
        f32 mouseDy = 0.0f;
        bool moveForward  = false;
        bool moveBackward = false;
        bool moveLeft     = false;
        bool moveRight    = false;
    };

    struct CameraController
    {
        void init(f32 x = 0.0f, f32 y = 0.0f, f32 z = -2.0f);

        void update(f32 dt, const CameraInput& input);

        [[nodiscard]] rendering::renderer::Camera getCamera() const;

    private:
        f32 x = 0.0f, y = 0.0f, z = 0.0f;
        f32 yaw = 0.0f;
        f32 pitch = 0.0f;
        f32 speed = 5.0f;
        f32 sensitivity = 0.1f;
    };
}