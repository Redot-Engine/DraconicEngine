module;

#include <bgfx/bgfx.h>
#include "macros.h"

module rendering.rhi;

import core.stdtypes;
import core.math.constants;

namespace draco::rendering::rhi {
BufferHandle create_vertex_buffer(void const *data, u32 size, LayoutHandle layout_h) {
	RHI_ASSERT(data != nullptr, "Vertex buffer data is null");
	RHI_ASSERT(size > 0, "Vertex buffer size is zero");

	auto *layout = get_checked(g_layouts, layout_h, "Layout");

	RHI_ASSERT(layout, "Invalid vertex layout");

	auto vbh = bgfx::createVertexBuffer(bgfx::copy(data, size), layout->layout);

	Buffer buf;
	buf.vbh = vbh;

	return g_buffers.create(buf);
}

BufferHandle create_index_buffer(void const *data, u32 size) {
	RHI_ASSERT(data != nullptr, "Index buffer data is null");
	RHI_ASSERT(size > 0, "Index buffer size is zero");

	bgfx::IndexBufferHandle ibh = bgfx::createIndexBuffer(bgfx::copy(data, size), BGFX_BUFFER_INDEX32);

	Buffer buf; // Idk why I named it this, it just sounds funny ;)
	buf.ibh      = ibh;
	buf.is_index = true;

	return g_buffers.create(buf);
}

BufferHandle create_dynamic_vertex_buffer(u32 size, LayoutHandle layout_h) {
	auto *layout = get_checked(g_layouts, layout_h, "Layout");
	RHI_ASSERT(layout, "Invalid layout");

	bgfx::DynamicVertexBufferHandle dvbh = bgfx::createDynamicVertexBuffer(size, layout->layout);

	RHI_ASSERT(bgfx::isValid(dvbh), "Failed to create dynamic vertex buffer");

	Buffer buf;
	buf.dvbh       = dvbh;
	buf.is_dynamic = true;

	return g_buffers.create(buf);
}

void update_dynamic_vertex_buffer(BufferHandle handle, u32 start_vertex, void const *data, u32 size) {
	auto *buf = get_checked(g_buffers, handle, "Buffer");

	if (!buf) { return; }

	RHI_ASSERT(buf->is_dynamic && !buf->is_index, "Not a dynamic vertex buffer");
	RHI_ASSERT(bgfx::isValid(buf->dvbh), "Invalid dynamic vertex buffer handle");

	bgfx::Memory const *mem = bgfx::copy(data, size);

	bgfx::update(buf->dvbh, start_vertex, mem);
}

BufferHandle create_dynamic_index_buffer(u32 size, u16 flags) {
	bgfx::DynamicIndexBufferHandle ibh = bgfx::createDynamicIndexBuffer(size, flags);

	RHI_ASSERT(bgfx::isValid(ibh), "Invalid dynamic index buffer handle");

	Buffer buf{};
	buf.is_dynamic = true;
	buf.is_index   = true;
	buf.dibh       = ibh;

	return g_buffers.create(buf);
}

void update_dynamic_index_buffer(BufferHandle handle, u32 start_index, void const *data, u32 size) {
	auto *buf = get_checked(g_buffers, handle, "DynamicIndexBuffer");

	if (!buf) { return; }

	RHI_ASSERT(buf->is_dynamic && buf->is_index, "Not a dynamic index buffer");

	bgfx::Memory const *mem = bgfx::copy(data, size);

	bgfx::update(buf->dibh, start_index, mem);
}
} // namespace draco::rendering::rhi
