export module scene.renderable;

import rendering.mesh;
import rendering.material;
import core.math.transform;

export namespace draco::scene::renderable {
struct Renderable {
	draco::rendering::mesh::MeshHandle mesh;

	draco::math::Transform transform;

	draco::rendering::material::Material material;
};
} // namespace draco::scene::renderable
