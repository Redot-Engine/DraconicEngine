module;

#include <cstdint>
#include <vector>

export module rendering.rhi.vertex;

export namespace draco::rendering::rhi {
enum class Attrib {
	Position,
	Color0,
	TexCoord0,
	Normal,
	Tangent
};

enum class AttribType {
	Float,
	Uint8
};

struct VertexElement {
	Attrib attrib;
	uint16_t count;
	AttribType type;
	bool normalized = false;
};

struct VertexLayoutDesc {
	std::vector<VertexElement> elements;
};

#pragma pack(push, 1)
struct TexturedVertex {
	float x, y, z;
	float u, v;
	uint32_t color;
};
#pragma pack(pop)

static_assert(sizeof(TexturedVertex) == 24);

// Helper to get the standard layout for the current vertex struct
inline VertexLayoutDesc get_textured_vertex_layout() {
	return {
		.elements = {{Attrib::Position, 3, AttribType::Float},
	                 {Attrib::TexCoord0, 2, AttribType::Float},
	                 {Attrib::Color0, 4, AttribType::Uint8, true}}
	};
}
} // namespace draco::rendering::rhi
