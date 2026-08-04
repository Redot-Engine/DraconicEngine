module;

#include <cassert>
#include <new>
#include <type_traits>
#include <utility>

export module core.containers.expected;
export import core.stdtypes;

export namespace draco::containers {

    /// @brief Wrapper context denoting an unexpected error status state.
    /// @tparam E Type of the error details payload.
    template <typename E>
    struct Unexpected
    {
        E errorValue;
        constexpr explicit Unexpected(E const &val) : errorValue(val) {}
        constexpr explicit Unexpected(E &&val) : errorValue(std::move(val)) {}
    };

    /// @brief Discriminated control union container representing either a valid output object or an error code.
    /// @tparam T Intended type of return value object.
    /// @tparam E Error details payload type on failures.
    template <typename T, typename E>
    struct Expected
    {
    private:
        union
        {
            T val;
            E err;
        };
        bool isValue;

    public:
        constexpr Expected(T const &value) : val(value), isValue(true) {}
        constexpr Expected(T &&value) : val(std::move(value)), isValue(true) {}
        constexpr Expected(Unexpected<E> const &u) : err(u.errorValue), isValue(false) {}
        constexpr Expected(Unexpected<E> &&u) : err(std::move(u.errorValue)), isValue(false) {}

        constexpr Expected(Expected const &other) : isValue(other.isValue)
        {
            if (isValue)
                ::new (static_cast<void *>(&val)) T(other.val);
            else
                ::new (static_cast<void *>(&err)) E(other.err);
        }

        constexpr Expected(Expected &&other) noexcept(std::is_nothrow_move_constructible_v<T> && std::is_nothrow_move_constructible_v<E>) : isValue(other.isValue)
        {
            if (isValue)
                ::new (static_cast<void *>(&val)) T(std::move(other.val));
            else
                ::new (static_cast<void *>(&err)) E(std::move(other.err));
        }

        ~Expected()
        {
            if (isValue)
                val.~T();
            else
                err.~E();
        }

        constexpr Expected &operator=(Expected const &other)
        {
            if (this != &other)
            {
                if (isValue && other.isValue)
                {
                    val = other.val;
                }
                else if (!isValue && !other.isValue)
                {
                    err = other.err;
                }
                else if (other.isValue)
                {
                    err.~E();
                    ::new (static_cast<void *>(&val)) T(other.val);
                    isValue = true;
                }
                else
                {
                    val.~T();
                    ::new (static_cast<void *>(&err)) E(other.err);
                    isValue = false;
                }
            }
            return *this;
        }

        constexpr Expected &operator=(Expected &&other) noexcept(std::is_nothrow_move_assignable_v<T> && std::is_nothrow_move_assignable_v<E> && std::is_nothrow_move_constructible_v<T> && std::is_nothrow_move_constructible_v<E>)
        {
            if (this != &other)
            {
                if (isValue && other.isValue)
                {
                    val = std::move(other.val);
                }
                else if (!isValue && !other.isValue)
                {
                    err = std::move(other.err);
                }
                else if (other.isValue)
                {
                    err.~E();
                    ::new (static_cast<void *>(&val)) T(std::move(other.val));
                    isValue = true;
                }
                else
                {
                    val.~T();
                    ::new (static_cast<void *>(&err)) E(std::move(other.err));
                    isValue = false;
                }
            }
            return *this;
        }

        /// @brief Checks if the container currently holds a valid functional output value state.
        constexpr bool hasValue() const { return isValue; }
        constexpr explicit operator bool() const { return isValue; }

        /// @brief Fetches mutable reference to value with assertion triggers on missing state entries.
        constexpr T &value()
        {
            assert(isValue);
            return val;
        }

        /// @brief Fetches constant reference to value with assertion triggers on missing state entries.
        constexpr T const &value() const
        {
            assert(isValue);
            return val;
        }

        /// @brief Fetches mutable reference to internal error details with failure check assertions.
        constexpr E &error()
        {
            assert(!isValue);
            return err;
        }

        /// @brief Fetches constant reference to internal error details with failure check assertions.
        constexpr E const &error() const
        {
            assert(!isValue);
            return err;
        }

        constexpr T &operator*() { return value(); }
        constexpr T const &operator*() const { return value(); }

        constexpr T *operator->() { return &value(); }
        constexpr T const *operator->() const { return &value(); }
    };
}
