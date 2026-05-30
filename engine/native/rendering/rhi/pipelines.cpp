module;

#include <vector>
#include <bgfx/bgfx.h>
#include "macros.h"

module rendering.rhi;

import core.stdtypes;
import core.math.constants;

namespace draco::rendering::rhi {
PipelineHandle create_pipeline(PipelineDesc const &desc) {
	RHI_ASSERT(desc.vs != InvalidShader, "Pipeline missing vertex shader");
	RHI_ASSERT(desc.fs != InvalidShader, "Pipeline missing fragment shader");

	bgfx::ProgramHandle prog = bgfx::createProgram(resolve(desc.vs), resolve(desc.fs), true);

	u64 state = map_state(desc.state, desc.blend, desc.depth, desc.cull, desc.depth_write);

	return g_pipelines.create({prog, state});
}

LayoutHandle create_vertex_layout(VertexLayoutDesc const &desc) {
	bgfx::VertexLayout layout;
	layout.begin();

	for (auto const &e : desc.elements) { layout.add(map_attrib(e.attrib), e.count, map_attrib_type(e.type), e.normalized); }

	layout.end();

	return g_layouts.create({layout});
}

ShaderHandle create_shader(void const *data, u32 size) {
	RHI_ASSERT(data && size > 0, "Invalid shader data");

	bgfx::ShaderHandle sh = bgfx::createShader(bgfx::copy(data, size));

	return g_shaders.create(sh);
}

bgfx::ShaderHandle resolve(ShaderHandle h) {
	auto *sh = g_shaders.get(h);
	return sh ? *sh : bgfx::ShaderHandle{bgfx::kInvalidHandle};
}

// For debugging/tooling
bgfx::ShaderHandle *get_shader_native(ShaderHandle h) {
	return get_checked(g_shaders, h, "Shader");
}

void destroy_shader(ShaderHandle h) {
	if (auto *sh = get_checked(g_shaders, h, "Shader")) {
		destroy_later(*sh);
		g_shaders.destroy(h);
	}
}
} // namespace draco::rendering::rhi
