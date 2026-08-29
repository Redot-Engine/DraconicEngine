// Draconic::RenderGraph - the `rendergraph` module.
//
// A render graph over the RHI: passes declare resource accesses, the graph
// resolves dependencies, allocates/aliases transient resources, and inserts the
// the same one Draconic's RHI is a faithful port of). One named module composed of
// partitions, re-exported here.

export module rendergraph;

export import :types;
export import :callbacks;
export import :descriptors;
export import :persistent_resource;
export import :resource;
export import :pass;
export import :state_tracker;
export import :barrier_solver;
export import :pass_builder;
export import :transient_pool;
export import :graph;
export import :validator;
export import :debug;
export import :profiler;
