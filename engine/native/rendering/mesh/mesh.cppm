module;

#include <vector>

export module rendering.mesh;

import core.stdtypes;
import core.memory;
import rendering.rhi;

export namespace draco::rendering::mesh {
struct MeshTag { };

using MeshHandle = draco::core::memory::Handle<MeshTag>;

struct Vertex {
	f32 px, py, pz;
	f32 nx, ny, nz;
	f32 u, v;
};

struct Mesh {
	draco::rendering::rhi::BufferHandle vbh;
	draco::rendering::rhi::BufferHandle ibh;

	draco::rendering::rhi::LayoutHandle layout;

	u32 vertex_count = 0;
	u32 index_count  = 0;

	bool valid = false;
};

MeshHandle create(void const *vertex_data, u32 vertex_size, u32 vertex_count, std::vector<u32> const &indices,
                  draco::rendering::rhi::LayoutHandle layout);

MeshHandle create_cube();
MeshHandle create_plane(float size);
MeshHandle create_sphere(int segments, int rings);
MeshHandle create_cylinder(int segments, float height);
MeshHandle create_capsule(int segments, int rings, float height);

void destroy(MeshHandle mesh);
Mesh const *get(MeshHandle mesh);
} // namespace draco::rendering::mesh

export namespace draco::rendering::mesh::gen {
std::vector<Vertex> cube_vertices();
std::vector<u32> cube_indices();

std::vector<Vertex> plane_vertices(float size);
std::vector<u32> plane_indices();

std::vector<Vertex> sphere_vertices(int segments, int rings);
std::vector<u32> sphere_indices(int segments, int rings);

std::vector<Vertex> cylinder_vertices(int segments, float height);
std::vector<u32> cylinder_indices(int segments);

std::vector<Vertex> capsule_vertices(int segments, int rings, float height);
std::vector<u32> capsule_indices(int segments, int rings);
} // namespace draco::rendering::mesh::gen
