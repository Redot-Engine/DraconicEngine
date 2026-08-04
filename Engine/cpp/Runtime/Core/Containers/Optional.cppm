module;

#include <cassert>
#include <new>
#include <type_traits>
#include <utility>

export module core.containers.optional;
export import core.stdtypes;

export namespace draco::containers {

    /// @brief Tag structure defining empty nullable states.
    struct NullOpt {} inline constexpr nullopt{};

    /// @brief Value wrapper structure using local union configurations to represent an optional entity.
    /// @tparam T Underlying data object type.
    template <typename T>
    struct Optional
    {
    private:
        union
        {
            T valueStorage;
            char dummy;
        };
        bool hasVal;

    public:
        constexpr Optional() : dummy{}, hasVal(false) {}
        constexpr Optional(NullOpt) : dummy{}, hasVal(false) {}

        constexpr Optional(T const &val) : hasVal(true)
        {
            ::new (static_cast<void *>(&valueStorage)) T(val);
        }

        constexpr Optional(T &&val) : hasVal(true)
        {
            ::new (static_cast<void *>(&valueStorage)) T(std::move(val));
        }

        constexpr Optional(Optional const &other) : hasVal(other.hasVal)
        {
            if (hasVal)
            {
                ::new (static_cast<void *>(&valueStorage)) T(other.valueStorage);
            }
        }

        constexpr Optional(Optional &&other) noexcept(std::is_nothrow_move_constructible_v<T>)
            : hasVal(other.hasVal)
        {
            if (hasVal)
            {
                ::new (static_cast<void *>(&valueStorage)) T(std::move(other.valueStorage));
            }
        }

        ~Optional()
        {
            reset();
        }

        constexpr Optional &operator=(NullOpt)
        {
            reset();
            return *this;
        }

        constexpr Optional &operator=(Optional const &combat)
        {
            if (this != &combat)
            {
                reset();
                if (combat.hasVal)
                {
                    ::new (static_cast<void *>(&valueStorage)) T(combat.valueStorage);
                    hasVal = true;
                }
            }
            return *this;
        }

        constexpr Optional &operator=(Optional &&combat) noexcept(std::is_nothrow_move_constructible_v<T>)
        {
            if (this != &combat)
            {
                reset();
                if (combat.hasVal)
                {
                    ::new (static_cast<void *>(&valueStorage)) T(std::move(combat.valueStorage));
                    hasVal = true;
                }
            }
            return *this;
        }

        /// @brief Validates if an initialized object entry value is valid & configured.
        constexpr bool hasValue() const { return hasVal; }
        constexpr explicit operator bool() const { return hasVal; }

        /// @brief Returns reference to contained type with checking asset triggers.
        constexpr T &value()
        {
            assert(hasVal);
            return valueStorage;
        }

        /// @brief Returns constant reference to contained type with checking asset triggers.
        constexpr T const &value() const
        {
            assert(hasVal);
            return valueStorage;
        }

        constexpr T &operator*() { return value(); }
        constexpr T const &operator*() const { return value(); }

        constexpr T *operator->() { return &value(); }
        constexpr T const *operator->() const { return &value(); }

        /// @brief Unwraps the object, falling back to a custom default reference configuration if empty.
        template <typename U>
        constexpr T valueOr(U &&defaultValue) const &
        {
            return hasVal ? valueStorage : static_cast<T>(std::forward<U>(defaultValue));
        }

        template <typename U>
        constexpr T valueOr(U &&defaultValue) &&
        {
            return hasVal ? std::move(valueStorage) : static_cast<T>(std::forward<U>(defaultValue));
        }

        /// @brief Destructs active values & returns optional wrapper interface context back to null state.
        constexpr void reset()
        {
            if (hasVal)
            {
                valueStorage.~T();
                hasVal = false;
            }
        }
    };
}
