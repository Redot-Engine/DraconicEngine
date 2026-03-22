export module core.defs;
export import core.version;
import std;

static_assert(__cplusplus >=  202302L, "Minimum of C++23 required.");

export namespace draco {

    // Traits and Concepts

    template <typename T>
    concept arithmetic = std::is_arithmetic_v<T>;

    template <typename T>
    concept trivial = std::is_trivial_v<T>;

    // Whether the default value of a type is just all-0 bytes.
    // This can most commonly be exploited by using memset for these types instead of loop-construct.
    // Must be explicitly specialized to mark a type as such.
    template <typename T>
    struct is_zero_constructible : std::false_type {};

    template <typename T>
    constexpr bool is_zero_constructible_v = is_zero_constructible<T>::value;

    template <typename T>
    concept zero_constructible = is_zero_constructible_v<T>;

    // Limit the depth of recursive algorithms when dealing with Array/Dictionary
    constexpr int MAX_RECURSION = 100;

}