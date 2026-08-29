/// Primary module for rhi.dx12. Re-exports all partitions.
/// DX12 backend - Windows only.

export module rhi.dx12;

export import :conversions;
export import :surface;
export import :descriptor_heap;
export import :adapter;
export import :backend;
export import :buffer;
export import :texture;
export import :texture_view;
export import :sampler;
export import :shader_module;
export import :fence;
export import :query_set;
export import :gpu_descriptor_heap;
export import :descriptor_staging;
export import :bind_group_layout;
export import :pipeline_cache;
export import :pipeline_layout;
export import :bind_group;
export import :render_pipeline;
export import :compute_pipeline;
export import :command_buffer;
export import :command_pool;
export import :render_pass_encoder;
export import :render_bundle_encoder;
export import :compute_pass_encoder;
export import :command_encoder;
export import :transfer_batch;
export import :swap_chain;
export import :queue;
export import :accel_struct;
export import :mesh_pipeline;
export import :ray_tracing_pipeline;
export import :device;
