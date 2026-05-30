module;

#include <print>
#include <algorithm>

#include <bgfx/bgfx.h>

module rendering.rendergraph;

import rendering.rhi;

namespace draco::rendering::rendergraph {

static void sort_material(std::vector<rhi::RenderPacket> &packets) {
	std::sort(packets.begin(), packets.end(),
	          [](rhi::RenderPacket const &a, rhi::RenderPacket const &b) {
				  // Pipeline first
				  if (a.pipeline != b.pipeline) {
					  return a.pipeline.value < b.pipeline.value;
				  }

				  // Texture second
				  if (a.texture_handle != b.texture_handle) {
					  return a.texture_handle.value < b.texture_handle.value;
				  }

				  // Vertex buffer third
				  if (a.vertex_buffer != b.vertex_buffer) {
					  return a.vertex_buffer.value < b.vertex_buffer.value;
				  }

				  // Index buffer fallback
				  return a.index_buffer.value < b.index_buffer.value;
			  });
}

// Placeholder until depth sorting exists
static void sort_front_to_back(std::vector<rhi::RenderPacket> &packets) {
	sort_material(packets);
}

static void sort_back_to_front(std::vector<rhi::RenderPacket> &packets) {
	sort_material(packets);
}

static void sort_packets(std::vector<rhi::RenderPacket> &packets,
                         SortMode mode) {
	switch (mode) {
	case SortMode::None:        break;

	case SortMode::Material:    sort_material(packets); break;

	case SortMode::FrontToBack: sort_front_to_back(packets); break;

	case SortMode::BackToFront: sort_back_to_front(packets); break;
	}
}

void RenderGraph::reset() {
	m_passes.clear(); // Directly clear
}

Pass &RenderGraph::add_pass(std::string const &name) {
	m_passes.emplace_back();

	auto &pass = m_passes.back();
	pass.name  = name;

	return pass;
}

Pass *RenderGraph::get_pass(std::string const &name) {
	for (auto &p : m_passes) {
		if (p.name == name) { return &p; }
	}

	return nullptr;
}

void RenderGraph::execute() {
	for (auto &pass : m_passes) {
		// Future dependency handling hook
		for (auto const &dep : pass.dependencies) { (void)dep; }

		sort_packets(pass.packets, pass.sort_mode);

		rhi::apply_view(pass.view,
		                {pass.framebuffer, 0, 0, pass.width, pass.height,
		                 pass.clear_flags, pass.clear_color});

		rhi::set_view_projection(pass.view, pass.view_mtx, pass.proj_mtx);

		if (pass.clear_flags) {
			bgfx::setViewClear(pass.view, pass.clear_flags, pass.clear_color);
		}

		for (auto &pkt : pass.packets) { rhi::submit(pkt, pass.view); }
	}
}
} // namespace draco::rendering::rendergraph
