module;

#include <cmath>
#include <algorithm>

#include <bgfx/bgfx.h>
#include <bx/math.h>

module rendering.quad_renderer;

import core.stdtypes;
import rendering.rhi;
import rendering.rhi.vertex;
import rendering.rendergraph;

namespace draco::rendering::quad_renderer {

static constexpr f32 QuadUV[4][2] = {
	{0.0F, 0.0F},
    {1.0F, 0.0F},
    {1.0F, 1.0F},
    {0.0F, 1.0F}
};

void QuadRenderer::init(draco::rendering::rhi::PipelineHandle pipeline) {
	using namespace draco::rendering::rhi;

	VertexLayoutDesc layout{};
	layout.elements.push_back({Attrib::Position, 3, AttribType::Float});
	layout.elements.push_back({Attrib::TexCoord0, 2, AttribType::Float});
	layout.elements.push_back({Attrib::Color0, 4, AttribType::Uint8, true});

	m_pipeline = pipeline;
	m_layout   = create_vertex_layout(layout);

	// Allocating dynamic streaming buffers
	m_vb = create_dynamic_vertex_buffer(sizeof(TexturedVertex) * MaxVertices, m_layout);

	// Pass BGFX_BUFFER_NONE implicitly to match tracking
	m_ib = create_dynamic_index_buffer(MaxIndices * sizeof(u16), BGFX_BUFFER_NONE);

	m_sampler = create_uniform("s_texColor", UniformType::Sampler);
}

void QuadRenderer::begin() {
	m_vertices.clear();
	m_indices.clear();

	m_quad_count = 0;

	m_batch_key = {};
}

void QuadRenderer::submit(QuadCommand const &cmd) {
	if (m_quad_count >= MaxQuads) { return; }

	BatchKey new_key{cmd.texture, m_pipeline, draco::rendering::rhi::InvalidSampler};

	if (m_batch_key.texture == draco::rendering::rhi::InvalidTexture) { m_batch_key = new_key; }

	bool state_change = !(new_key == m_batch_key);

	if (state_change) {
		// TODO: Flush current batch automatically

		return;
	}

	push_quad(cmd);

	m_quad_count++;
}

void QuadRenderer::push_quad(QuadCommand const &cmd) {
	f32 hw = cmd.width * 0.5F;
	f32 hh = cmd.height * 0.5F;

	f32 c = cosf(cmd.rotation);
	f32 s = sinf(cmd.rotation);

	f32 corners[4][2] = {
		{-hw, -hh},
        { hw, -hh},
        { hw,  hh},
        {-hw,  hh}
	};

	u16 start = static_cast<u16>(m_vertices.size());

	for (int i = 0; i < 4; i++) {
		f32 rx = corners[i][0] * c - corners[i][1] * s;

		f32 ry = corners[i][0] * s + corners[i][1] * c;

		draco::rendering::rhi::TexturedVertex v{};

		v.x = cmd.x + rx;
		v.y = cmd.y + ry;
		v.z = cmd.z;

		v.u = QuadUV[i][0];
		v.v = QuadUV[i][1];

		v.color = cmd.color;

		m_vertices.push_back(v);
	}

	m_indices.push_back(start + 0);
	m_indices.push_back(start + 1);
	m_indices.push_back(start + 2);

	m_indices.push_back(start + 2);
	m_indices.push_back(start + 3);
	m_indices.push_back(start + 0);
}

void QuadRenderer::flush_to_pass(draco::rendering::rendergraph::Pass &pass) {
	using namespace draco::rendering::rhi;

	if (m_vertices.empty()) { return; }

	// Upload only the exact slices we are using this frame
	update_dynamic_vertex_buffer(m_vb, 0, m_vertices.data(), static_cast<u32>(m_vertices.size() * sizeof(TexturedVertex)));
	update_dynamic_index_buffer(m_ib, 0, m_indices.data(), static_cast<u32>(m_indices.size() * sizeof(u16)));

	RenderPacket pkt{};
	pkt.vertex_buffer   = m_vb;
	pkt.index_buffer    = m_ib;
	pkt.pipeline        = m_pipeline;
	pkt.texture_handle  = m_batch_key.texture;
	pkt.sampler_uniform = m_sampler;

	pkt.vertex_count = static_cast<u32>(m_vertices.size());
	pkt.index_count  = static_cast<u32>(m_indices.size());

	pkt.sort_key = make_sort_key(0, 0, static_cast<u16>(m_pipeline.value), static_cast<u16>(m_batch_key.texture.value), 0);

	bx::mtxIdentity(pkt.model);

	pass.packets.push_back(pkt);

	m_vertices.clear();
	m_indices.clear();
}

void QuadRenderer::shutdown() {
	using namespace draco::rendering::rhi;

	destroy_buffer(m_vb);
	destroy_buffer(m_ib);

	destroy_uniform(m_sampler);
}

void QuadRenderer::build_ortho(OrthoCamera &cam, f32 width, f32 height) {
	using namespace draco::rendering::rhi;

	identity_matrix(cam.view);
	identity_matrix(cam.proj);

	f32 rl = std::max(width, 1.0F);
	f32 tb = std::max(height, 1.0F);

	cam.proj[0]  = 2.0F / rl;
	cam.proj[5]  = -2.0F / tb;
	cam.proj[10] = -1.0F;

	cam.proj[12] = -1.0F;
	cam.proj[13] = 1.0F;
}
} // namespace draco::rendering::quad_renderer
