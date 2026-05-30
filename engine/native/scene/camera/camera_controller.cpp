module;

#include <math.h>
#include <bx/math.h>

module scene.camera.controller;

import input;

namespace draco::scene {
void CameraController::init(f32 x, f32 y, f32 z) {
	m_x = x;
	m_y = y;
	m_z = z;

	m_yaw   = 0.0F;
	m_pitch = 0.0F;

	m_speed       = 5.0F;   // units per second
	m_sensitivity = 0.002F; // mouse sensitivity
}

void CameraController::update(f32 dt) {
	m_yaw += draco::input::get_mouse_dx() * m_sensitivity;
	m_pitch -= draco::input::get_mouse_dy() *
	           m_sensitivity; // Temp fix to flip mouse input

	// Clamp pitch
	if (m_pitch > 1.5F) { m_pitch = 1.5F; }
	if (m_pitch < -1.5F) { m_pitch = -1.5F; }

	bx::Vec3 forward = {cosf(m_pitch) * sinf(m_yaw), sinf(m_pitch),
	                    cosf(m_pitch) * cosf(m_yaw)};

	bx::Vec3 right = {sinf(m_yaw - bx::kPiHalf), 0.0F,
	                  cosf(m_yaw - bx::kPiHalf)};

	f32 velocity = m_speed * dt;

	if (draco::input::is_down(draco::input::Key::W)) {
		m_x += forward.x * velocity;
		m_y += forward.y * velocity;
		m_z += forward.z * velocity;
	}

	if (draco::input::is_down(draco::input::Key::S)) {
		m_x -= forward.x * velocity;
		m_y -= forward.y * velocity;
		m_z -= forward.z * velocity;
	}

	if (draco::input::is_down(draco::input::Key::A)) {
		m_x += right.x * velocity;
		m_z += right.z * velocity;
	}

	if (draco::input::is_down(draco::input::Key::D)) {
		m_x -= right.x * velocity;
		m_z -= right.z * velocity;
	}
}

draco::rendering::renderer::Camera CameraController::get_camera() const {
	bx::Vec3 forward = {cosf(m_pitch) * sinf(m_yaw), sinf(m_pitch),
	                    cosf(m_pitch) * cosf(m_yaw)};

	draco::rendering::renderer::Camera cam{};

	cam.position = {m_x, m_y, m_z};
	cam.target   = {m_x + forward.x, m_y + forward.y, m_z + forward.z};

	cam.up = {0.0F, 1.0F, 0.0F};

	cam.fov        = 60.0F;
	cam.near_plane = 0.1F;
	cam.far_plane  = 100.0F;

	return cam;
}
} // namespace draco::scene
