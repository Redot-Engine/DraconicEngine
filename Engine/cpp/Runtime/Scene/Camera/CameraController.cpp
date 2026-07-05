module;

#include <cmath>
#include <bx/math.h>

module scene.camera.controller;

namespace draco::scene
{
    void CameraController::init(f32 px, f32 py, f32 pz)
    {
        x = px;
        y = py;
        z = pz;

        yaw = 0.0f;
        pitch = 0.0f;

        speed = 5.0f;          // units per second
        sensitivity = 0.002f;  // mouse sensitivity
    }

    void CameraController::update(f32 dt, const CameraInput& input)
    {
        yaw   += input.mouseDx * sensitivity;
        pitch -= input.mouseDy * sensitivity; // Temp fix to flip mouse input

        // Clamp pitch
        if (pitch > 1.5f)  pitch = 1.5f;
        if (pitch < -1.5f) pitch = -1.5f;

        bx::Vec3 forward = {
            cosf(pitch) * sinf(yaw),
            sinf(pitch),
            cosf(pitch) * cosf(yaw)
        };

        bx::Vec3 right = {
            sinf(yaw - bx::kPiHalf),
            0.0f,
            cosf(yaw - bx::kPiHalf)
        };

        f32 velocity = speed * dt;

        if (input.moveForward)
        {
            x += forward.x * velocity;
            y += forward.y * velocity;
            z += forward.z * velocity;
        }

        if (input.moveBackward)
        {
            x -= forward.x * velocity;
            y -= forward.y * velocity;
            z -= forward.z * velocity;
        }

        if (input.moveLeft)
        {
            x += right.x * velocity;
            z += right.z * velocity;
        }

        if (input.moveRight)
        {
            x -= right.x * velocity;
            z -= right.z * velocity;
        }
    }

    rendering::renderer::Camera CameraController::getCamera() const
    {
        const bx::Vec3 forward = {
            cosf(pitch) * sinf(yaw),
            sinf(pitch),
            cosf(pitch) * cosf(yaw)
        };

        rendering::renderer::Camera cam{};

        cam.position = { x, y, z };
        cam.target   = {
            x + forward.x,
            y + forward.y,
            z + forward.z
        };

        cam.up = { 0.0f, 1.0f, 0.0f };

        cam.fov = 60.0f;
        cam.nearPlane = 0.1f;
        cam.farPlane  = 100.0f;

        return cam;
    }
}
