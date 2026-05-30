module;

#include <vector>
#include <bgfx/bgfx.h>
#include <bx/math.h>
#include "macros.h"

module rendering.rhi;

import core.stdtypes;
import core.math.constants;

namespace draco::rendering::rhi {
void perspective(f32 *out, f32 fov, f32 aspect, f32 nearp, f32 farp) {
	bx::mtxProj(out, fov, aspect, nearp, farp, bgfx::getCaps()->homogeneousDepth);
}

void look_at(f32 *out, f32 const *eye, f32 const *at, f32 const *up) {
	bx::Vec3 eye_v{eye[0], eye[1], eye[2]};
	bx::Vec3 at_v{at[0], at[1], at[2]};
	bx::Vec3 up_v{up[0], up[1], up[2]};

	bx::mtxLookAt(out, eye_v, at_v, up_v);
}

// Note: Internal use only, use apply_view() instead
void set_view_rect(ViewID view, u16 x, u16 y, u16 w, u16 h) {
	bgfx::setViewRect(view, x, y, w, h);
}

// Note: Internal use only, use apply_view() instead
void set_view_framebuffer(ViewID view, FramebufferHandle h) {
	auto *fb = get_checked(g_framebuffers, h, "Framebuffer");

	if (!fb) { return; }

	bgfx::setViewFrameBuffer(view, fb->fbh);
}

void set_view_projection(ViewID view, f32 const *view_mtx, f32 const *proj_mtx) {
	bgfx::setViewTransform(view, view_mtx, proj_mtx);
}

void set_scissor(ScissorRect const &r) {
	if (!r.enabled) { bgfx::setScissor(math::UINT16_MAX_VAL); }
	else { bgfx::setScissor(r.x, r.y, r.w, r.h); }
}

void set_stencil(u32 fstencil, u32 bstencil) {
	bgfx::setStencil(fstencil, bstencil);
}

void apply_view(ViewID view, ViewDesc const &desc) {
	if (desc.fb != InvalidFramebuffer) {
		auto *fb = get_checked(g_framebuffers, desc.fb, "Framebuffer");

		if (fb && bgfx::isValid(fb->fbh)) { bgfx::setViewFrameBuffer(view, fb->fbh); }
		else { RHI_WARN(false, "Framebuffer invalid at apply_view"); }
	}

	bgfx::setViewRect(view, desc.x, desc.y, desc.w, desc.h);

	if (desc.clear_flags != 0) { bgfx::setViewClear(view, desc.clear_flags, desc.clear_color); }
}

void identity_matrix(f32 *mtx) {
	bx::mtxIdentity(mtx);
}

void submit(RenderPacket const &p, ViewID view) {
	auto *pipeline = get_checked(g_pipelines, p.pipeline, "Pipeline");
	auto *vb       = get_checked(g_buffers, p.vertex_buffer, "VertexBuffer");
	Buffer *ib     = nullptr;

	if (!pipeline || !vb) { return; }

	if (p.index_buffer != InvalidBuffer) { ib = get_checked(g_buffers, p.index_buffer, "IndexBuffer"); }

	// Transform matrix (model)
	bgfx::setTransform(p.model);

	// Vertex buffer binding with explicit range control
	if (vb->is_dynamic) {
		// If count is UINT32_MAX, bgfx will fallback to drawing the full buffer automatically
		bgfx::setVertexBuffer(0, vb->dvbh, 0, p.vertex_count);
	}
	else { bgfx::setVertexBuffer(0, vb->vbh, 0, p.vertex_count); }

	// Index buffer binding with explicit range control
	if (ib && ib->is_index) {
		if (ib->is_dynamic) { bgfx::setIndexBuffer(ib->dibh, 0, p.index_count); }
		else { bgfx::setIndexBuffer(ib->ibh, 0, p.index_count); }
	}

	// Uniforms
	for (auto const &u : p.uniforms) {
		if (auto *handle = get_checked(g_uniforms, u.handle, "UniformBind")) { bgfx::setUniform(*handle, u.data, u.num); }
	}

	// Texture binding
	if (auto *tex = get_checked(g_textures, p.texture_handle, "Texture")) {
		if (auto *sampler = get_checked(g_uniforms, p.sampler_uniform, "Sampler")) {
			bgfx::setTexture(p.texture_unit, *sampler, *tex, p.sampler_flags);
		}
	}

	// Apply pipeline state & submit draw call
	bgfx::setState(pipeline->state);
	bgfx::submit(view, pipeline->program);
}
} // namespace draco::rendering::rhi
