module;

#include <cmath>

#include <bx/math.h>

export module core.math.transform;

import core.stdtypes;

export namespace draco::math {
struct Transform {
	f32 position[3] = {0.0F, 0.0F, 0.0F};
	f32 rotation[3] = {0.0F, 0.0F, 0.0F}; // Euler (radians)
	f32 scale[3]    = {1.0F, 1.0F, 1.0F};
};

// TODO: Besides compute_matrix, we should make rest of the funcs constexpr funcs

// Creates a default identity transform
Transform make_transform();

// Recompute matrix from transform (column-major)
void compute_matrix(Transform const &t, f32 out[16]);

// Helpers
void set_position(Transform &t, f32 x, f32 y, f32 z);
void set_rotation(Transform &t, f32 x, f32 y, f32 z);
void set_scale(Transform &t, f32 x, f32 y, f32 z);
} // namespace draco::math
