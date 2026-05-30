module;

#include <vector>

export module rendering.material;

import core.stdtypes;
import rendering.rhi;

export namespace draco::rendering::material {
struct Uniform {
	u32 name_hash    = 0;
	void const *data = nullptr;
	u16 count        = 1;
};

struct Material {
	u32 shader_id = 0;

	rhi::PipelineHandle pipeline = rhi::InvalidPipeline;

	rhi::TextureHandle texture = rhi::InvalidTexture;
	rhi::UniformHandle sampler = rhi::InvalidUniform;

	u8 texture_unit = 0;

	std::vector<Uniform> uniforms;
};
} // namespace draco::rendering::material
