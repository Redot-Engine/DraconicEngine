module scene.transform_component;

namespace draco::scene {
void mark_dirty(TransformComponent &t) {
	t.dirty = true;
}
} // namespace draco::scene
