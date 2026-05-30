export module scene.camera.controller;

import core.stdtypes;
import rendering;

export namespace draco::scene {
struct CameraController {
	void init(f32 x = 0.0F, f32 y = 0.0F, f32 z = -2.0F);

	void update(f32 dt);

	draco::rendering::renderer::Camera get_camera() const;

	private:
	// Init with default values
	f32 m_x = 0.0F, m_y = 0.0F, m_z = 0.0F;
	f32 m_yaw         = 0.0F;
	f32 m_pitch       = 0.0F;
	f32 m_speed       = 5.0F;
	f32 m_sensitivity = 0.1F;
};
} // namespace draco::scene
