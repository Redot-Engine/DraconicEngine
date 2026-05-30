module;

#include <vector>

export module scene;

export import scene.renderable;
export import scene.transform_component;
export import scene.camera.controller;

export namespace draco::scene {
struct Scene {
	std::vector<renderable::Renderable> renderables;
};
} // namespace draco::scene
