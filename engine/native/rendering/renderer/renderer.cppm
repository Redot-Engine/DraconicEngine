module;

#include <array>

#include <bx/math.h>

export module rendering.renderer;

import core.stdtypes;
import core.math.transform;
import rendering.rhi;
import rendering.rendergraph;
import rendering.quad_renderer;
import rendering.material;
import rendering.mesh;

export namespace draco::rendering::renderer {

struct Camera {
	std::array<f32, 3> position{0.0F, 0.0F, 0.0F};
	std::array<f32, 3> target{0.0F, 0.0F, 0.0F};
	std::array<f32, 3> up{0.0F, 1.0F, 0.0F};
	f32 fov        = 60.0F;
	f32 near_plane = 0.1F;
	f32 far_plane  = 1000.0F;
};

struct SceneContext {
	u16 screen_width  = 0;
	u16 screen_height = 0;
	Camera main_camera;

	draco::rendering::rendergraph::RenderGraph graph;
};

inline SceneContext g_ctx;

void init(u16 width, u16 height);
void resize(u16 width, u16 height);

void begin_frame(Camera const &cam);

void submit_entity(draco::rendering::rhi::RenderPacket &packet, u16 view);
void submit_renderable(draco::math::Transform const &transform, material::Material const &material, mesh::MeshHandle mesh_id);
void submit_ui(draco::rendering::quad_renderer::QuadRenderer &quad_renderer);

void end_frame();

rendergraph::RenderGraph &get_graph();
} // namespace draco::rendering::renderer
