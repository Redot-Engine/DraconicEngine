module;

#include <vector>
#include <cstdint>

export module rendering.quad_renderer;

import core.stdtypes;
import rendering.rhi;
import rendering.rhi.vertex;
import rendering.rendergraph;

export namespace draco::rendering::quad_renderer {

struct BatchKey {
	draco::rendering::rhi::TextureHandle texture = draco::rendering::rhi::InvalidTexture;

	draco::rendering::rhi::PipelineHandle pipeline = draco::rendering::rhi::InvalidPipeline;

	draco::rendering::rhi::SamplerHandle sampler = draco::rendering::rhi::InvalidSampler;

	bool operator==(BatchKey const &) const = default;
};

struct QuadCommand {
	draco::rendering::rhi::TextureHandle texture = draco::rendering::rhi::InvalidTexture;

	f32 x = 0.0F;
	f32 y = 0.0F;
	f32 z = 0.0F;

	f32 width  = 1.0F;
	f32 height = 1.0F;

	f32 rotation = 0.0F;

	u32 color = 0xffff'ffff;
};

struct OrthoCamera {
	f32 view[16];
	f32 proj[16];

	f32 x    = 0.0F;
	f32 y    = 0.0F;
	f32 zoom = 1.0F;
};

class QuadRenderer {
	public:
	static constexpr u32 MaxQuads    = 10000;
	static constexpr u32 MaxVertices = MaxQuads * 4;
	static constexpr u32 MaxIndices  = MaxQuads * 6;

	void init(draco::rendering::rhi::PipelineHandle pipeline);

	void begin();

	void submit(QuadCommand const &cmd);

	void flush_to_pass(draco::rendering::rendergraph::Pass &pass);

	void shutdown();

	static void build_ortho(OrthoCamera &cam, f32 width, f32 height);

	private:
	void push_quad(QuadCommand const &cmd);

	private:
	BatchKey m_batch_key{};

	std::vector<draco::rendering::rhi::TexturedVertex> m_vertices;

	std::vector<u16> m_indices;

	draco::rendering::rhi::BufferHandle m_vb = draco::rendering::rhi::InvalidBuffer;

	draco::rendering::rhi::BufferHandle m_ib = draco::rendering::rhi::InvalidBuffer;

	draco::rendering::rhi::LayoutHandle m_layout = draco::rendering::rhi::InvalidLayout;

	draco::rendering::rhi::PipelineHandle m_pipeline = draco::rendering::rhi::InvalidPipeline;

	draco::rendering::rhi::UniformHandle m_sampler = draco::rendering::rhi::InvalidUniform;

	u32 m_quad_count = 0;
};
} // namespace draco::rendering::quad_renderer
