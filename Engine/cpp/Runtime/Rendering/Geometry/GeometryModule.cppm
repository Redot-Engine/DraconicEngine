/// Primary module for geometry - the engine's runtime mesh format. Re-exports all partitions.
///
/// Distinct from draco.model (the importer's representation of a loaded file): this
/// is the canonical, GPU-upload-ready mesh the renderer and scene components consume.
/// A converter (tooling, not ported) turns an imported ModelMesh into a StaticMesh /
/// SkinnedMesh. Aggregates the partitions: value types (:types), the index buffer
/// (:index_buffer), the StaticMesh/SkinnedMesh formats (:mesh), and procedural
/// primitives (:primitives).

export module geometry;

export import :types;
export import :index_buffer;
export import :mesh;
export import :primitives;
