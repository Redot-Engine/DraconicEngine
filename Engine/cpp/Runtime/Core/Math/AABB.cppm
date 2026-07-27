module;

#include <limits>

export module core.math.aabb;

import core.defs;
import core.stdtypes;
import core.math.types;
import core.math.functions;

export namespace draco::math {

    /**
     * @brief An axis-aligned bounding box defined by its minimum and maximum corners.
     */
    struct [[nodiscard]] AABB
    {
        Vector3 min{0.0f, 0.0f, 0.0f};
        Vector3 max{0.0f, 0.0f, 0.0f};

        /**
         * @brief Returns an inverted box (min = +inf, max = -inf) so that the first expand()
         * call correctly snaps both corners to the target point.
         */
        [[nodiscard]] static constexpr AABB empty() noexcept
        {
            constexpr f32 inf = std::numeric_limits<f32>::infinity();
            return AABB{Vector3{inf}, Vector3{-inf}};
        }

        /**
         * @brief Grows the bounding box to encapsulate the given point `p`.
         */
        constexpr void expand(const Vector3 &p) noexcept
        {
            min = draco::math::min(min, p);
            max = draco::math::max(max, p);
        }

        /**
         * @brief Computes and returns the center point of the bounding box.
         */
        [[nodiscard]] constexpr Vector3 center() const noexcept
        {
            return (min + max) * 0.5f;
        }

        /**
         * @brief Computes and returns the total dimensions/size of the bounding box.
         */
        [[nodiscard]] constexpr Vector3 size() const noexcept
        {
            return max - min;
        }
    };
}
