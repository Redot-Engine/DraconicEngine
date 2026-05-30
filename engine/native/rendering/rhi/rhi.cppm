module;

#include <vector>
#include <algorithm>
#include <functional>
#include <print>
#include <cstdlib>

#include <bgfx/bgfx.h>

#include "macros.h"

export module rendering.rhi;

import core.stdtypes;
import core.math.constants;
import core.memory;
import platform;
import rendering.rhi.vertex;

export namespace draco::rendering::rhi {
struct BufferTag { };
struct PipelineTag { };
struct ShaderTag { };
struct UniformTag { };
struct TextureTag { };
struct FramebufferTag { };
struct LayoutTag { };

using BufferHandle      = core::memory::Handle<BufferTag>;
using PipelineHandle    = core::memory::Handle<PipelineTag>;
using ShaderHandle      = core::memory::Handle<ShaderTag>;
using UniformHandle     = core::memory::Handle<UniformTag>;
using TextureHandle     = core::memory::Handle<TextureTag>;
using FramebufferHandle = core::memory::Handle<FramebufferTag>;
using LayoutHandle      = core::memory::Handle<LayoutTag>;

using ViewID        = u16; // bgfx native
using SamplerHandle = u64; // bgfx sampler flags

inline constexpr BufferHandle InvalidBuffer{};
inline constexpr PipelineHandle InvalidPipeline{};
inline constexpr ShaderHandle InvalidShader{};
inline constexpr UniformHandle InvalidUniform{};
inline constexpr TextureHandle InvalidTexture{};
inline constexpr FramebufferHandle InvalidFramebuffer{};
inline constexpr LayoutHandle InvalidLayout{};
inline constexpr SamplerHandle InvalidSampler = 0;
inline constexpr ViewID InvalidView           = math::UINT16_MAX_VAL;

enum class PipelineState : u64 {
	Default           = 0,
	WriteRGB          = 1ULL << 0,
	WriteAlpha        = 1ULL << 1,
	MSAA              = 1ULL << 2,
	PrimitiveTriStrip = 1ULL << 3,
};

enum class ClearFlags : u32 {
	Color   = BGFX_CLEAR_COLOR,
	Depth   = BGFX_CLEAR_DEPTH,
	Stencil = BGFX_CLEAR_STENCIL
};

struct ViewDesc {
	FramebufferHandle fb = InvalidFramebuffer;
	u16 x = 0, y = 0, w = 0, h = 0;
	u32 clear_flags = 0;
	u32 clear_color = 0;
};

enum class UniformType {
	Sampler,
	Vec4,
	Mat3,
	Mat4,
};

struct UniformBind {
	UniformHandle handle;
	void const *data;
	u16 num;
};

enum class TextureFormat {
	RGBA8,
	BGRA8,
	D16,
	D24,
	D24S8,
	D32
};

enum class BlendMode {
	None,
	Alpha,
	Additive,
	Multiply
};

enum class DepthTest {
	None,
	Less,
	Equal,
	Always
};

enum class CullMode {
	None,
	CW,
	CCW
};

struct Buffer {
	bgfx::VertexBufferHandle vbh         = BGFX_INVALID_HANDLE;
	bgfx::DynamicVertexBufferHandle dvbh = BGFX_INVALID_HANDLE;
	bgfx::IndexBufferHandle ibh          = BGFX_INVALID_HANDLE;
	bgfx::DynamicIndexBufferHandle dibh;
	bool is_dynamic = false;
	bool is_index   = false;
};

struct FramebufferResource {
	bgfx::FrameBufferHandle fbh;
	TextureHandle texture;
};

struct VertexLayoutResource {
	bgfx::VertexLayout layout;
};

struct ScissorRect {
	u16 x, y, w, h;
	bool enabled = true;
};

struct DeletionReq {
	u64 frame;
	std::function<void()> cleanup;
};

struct PipelineDesc {
	ShaderHandle vs;
	ShaderHandle fs;
	PipelineState state = PipelineState::Default;

	BlendMode blend = BlendMode::None;
	DepthTest depth = DepthTest::Less;
	CullMode cull   = CullMode::CCW;

	bool depth_write = true;
};

struct RenderPacket {
	u64 sort_key = 0;

	BufferHandle vertex_buffer = InvalidBuffer;
	BufferHandle index_buffer  = InvalidBuffer;
	PipelineHandle pipeline    = InvalidPipeline;

	u32 vertex_count = math::UINT32_MAX_VAL;
	u32 index_count  = math::UINT32_MAX_VAL;

	UniformHandle sampler_uniform = InvalidUniform;
	SamplerHandle sampler_flags   = InvalidSampler;
	TextureHandle texture_handle  = InvalidTexture;

	f32 color[4] = {1, 1, 1, 1};

	std::vector<UniformBind> uniforms;
	u8 texture_unit = 0;

	f32 model[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

	u32 draw_tags = 0;
};

struct Pipeline {
	bgfx::ProgramHandle program;
	u64 state;
};

bool init(void *display_type,
          void *window_handle,
          draco::platform::NativeWindowType window_type,
          u16 width,
          u16 height);
void resize(u16 width, u16 height);
void shutdown();

PipelineHandle create_pipeline(PipelineDesc const &);

BufferHandle
create_vertex_buffer(void const *data, u32 size, LayoutHandle layout_h);
BufferHandle create_index_buffer(void const *data, u32 size);
void destroy_buffer(BufferHandle handle);

UniformHandle create_uniform(char const *name, UniformType type, u16 num = 1);
void destroy_uniform(UniformHandle handle);
void set_uniform(UniformHandle handle, void const *value, u16 num = 1);

TextureHandle
create_texture(void const *data, u32 width, u32 height, u32 flags = 0);
void destroy_texture(TextureHandle handle);

FramebufferHandle
create_framebuffer(u32 width, u32 height, TextureFormat format);
void destroy_framebuffer(FramebufferHandle handle);
TextureHandle get_framebuffer_texture(FramebufferHandle handle);

BufferHandle create_dynamic_vertex_buffer(u32 size, LayoutHandle layout);
void update_dynamic_vertex_buffer(BufferHandle handle,
                                  u32 start_vertex,
                                  void const *data,
                                  u32 size);

BufferHandle create_dynamic_index_buffer(u32 size,
                                         u16 flags = BGFX_BUFFER_NONE);
void update_dynamic_index_buffer(BufferHandle handle,
                                 u32 start_index,
                                 void const *data,
                                 u32 size);

LayoutHandle create_vertex_layout(VertexLayoutDesc const &desc);

SamplerHandle create_sampler(bool linear, bool clamp);

// Expects bgfx compiled shader binary (shaderc output)
ShaderHandle create_shader(void const *data, u32 size);
bgfx::ShaderHandle resolve(ShaderHandle h);
// For debugging/tooling
bgfx::ShaderHandle *get_shader_native(ShaderHandle h);
void destroy_shader(ShaderHandle h);

void perspective(f32 *out, f32 fov, f32 aspect, f32 nearp, f32 farp);
void look_at(f32 *out, f32 const *eye, f32 const *at, f32 const *up);

// Note: Internal use only, use apply_view() instead
void set_view_rect(ViewID view, u16 x, u16 y, u16 w, u16 h);
void set_view_framebuffer(ViewID view, FramebufferHandle handle);

void set_view_projection(ViewID view, f32 const *view_mtx, f32 const *proj_mtx);
void set_scissor(ScissorRect const &r);
void set_stencil(u32 f_stencil, u32 b_stencil);

void apply_view(ViewID view, ViewDesc const &desc);

void identity_matrix(f32 *_mtx);

u64
map_state(PipelineState s, BlendMode, DepthTest, CullMode, bool depth_write);
bgfx::UniformType::Enum map_uniform_type(UniformType t);
bgfx::Attrib::Enum map_attrib(Attrib a);
bgfx::AttribType::Enum map_attrib_type(AttribType t);

void submit(RenderPacket const &, ViewID);

void begin_frame();
void end_frame();

template<typename T>
void destroy_resource(T handle);

void process_deletions();

inline u64
make_sort_key(u8 layer, u8 pass, u16 pipeline, u16 texture, u16 depth = 0) {
	return (u64(layer) << 56) | (u64(pass) << 48) | (u64(pipeline) << 32) |
	       (u64(texture) << 16) | u64(depth);
}

constexpr PipelineState operator|(PipelineState a, PipelineState b) {
	return static_cast<PipelineState>(static_cast<u64>(a) |
	                                  static_cast<u64>(b));
}

constexpr PipelineState operator&(PipelineState a, PipelineState b) {
	return static_cast<PipelineState>(static_cast<u64>(a) &
	                                  static_cast<u64>(b));
}
} // namespace draco::rendering::rhi

// These are the things that we don't export but are visible to all implementation files
namespace draco::rendering::rhi {
using namespace draco::core::memory;

extern HandleRegistry<Buffer, BufferTag> g_buffers;
extern HandleRegistry<Pipeline, PipelineTag> g_pipelines;
extern HandleRegistry<bgfx::UniformHandle, UniformTag> g_uniforms;
extern HandleRegistry<bgfx::TextureHandle, TextureTag> g_textures;
extern HandleRegistry<FramebufferResource, FramebufferTag> g_framebuffers;
extern HandleRegistry<bgfx::ShaderHandle, ShaderTag> g_shaders;
extern HandleRegistry<VertexLayoutResource, LayoutTag> g_layouts;

// Deferred destruction queue (GPU-safe deletion)
extern std::vector<DeletionReq> g_deletion_queue;

extern u16 g_width;
extern u16 g_height;

// Ensures a handle is valid before use
// TODO: Replace with something better
template<typename Registry, typename HandleT>
auto *get_checked(Registry &reg, HandleT h, char const *name) {
	if (!reg.valid(h)) {
		RHI_WARN(false, "{} handle invalid or stale!", name);
		return (decltype(reg.get(h)))nullptr;
	}

	return reg.get(h);
}
} // namespace draco::rendering::rhi

// Re-export the namespace since the things it uses aren't declared before
// This is a bit hacky but it works
// The problem is that if we move the unexported namespace above the other exported namespace, stuff isn't declared yet & it gives errors
// The same goes for the exported namespace
// In a nutshell, they both rely on each other which is why we use this hack (AR-DEV-1)
export namespace draco::rendering::rhi {
void queue_destruction(std::function<void()> cb);

// Explicit overloads for each bgfx resource type
void destroy_later(bgfx::ShaderHandle handle);
void destroy_later(bgfx::UniformHandle handle);
void destroy_later(bgfx::VertexBufferHandle handle);
void destroy_later(bgfx::IndexBufferHandle handle);
void destroy_later(bgfx::DynamicVertexBufferHandle handle);
void destroy_later(bgfx::DynamicIndexBufferHandle handle);
void destroy_later(bgfx::TextureHandle handle);
void destroy_later(bgfx::FrameBufferHandle handle);
} // namespace draco::rendering::rhi
