module;

#include <algorithm>
#include <limits>

export module core.math.aabb;

import core.defs;
import core.stdtypes;
import core.math.types;

export namespace draco::math {

// An axis-aligned bounding box defined by its min/max corners.
struct AABB {
    Vector3 min{ 0, 0, 0 };
    Vector3 max{ 0, 0, 0 };

    // An inverted box (min = +inf, max = -inf) so the first expand() sets both corners.
    [[nodiscard]] static AABB empty() noexcept {
        constexpr f32 inf = std::numeric_limits<f32>::infinity();
        return AABB{ Vector3{ inf, inf, inf }, Vector3{ -inf, -inf, -inf } };
    }
    // Grows the box to contain point `p`.
    void expand(Vector3 p) noexcept {
        min = Vector3{ std::min(min.x, p.x), std::min(min.y, p.y), std::min(min.z, p.z) };
        max = Vector3{ std::max(max.x, p.x), std::max(max.y, p.y), std::max(max.z, p.z) };
    }
    [[nodiscard]] Vector3 center() const noexcept { return (min + max) * 0.5f; }
    [[nodiscard]] Vector3 size()   const noexcept { return max - min; }
};

} // namespace draco::math
