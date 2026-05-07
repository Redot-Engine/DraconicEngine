module;

// TODO: STL?

export module core.defs;
export import core.version;

static_assert(__cplusplus >=  202302L, "Minimum of C++23 required.");

export namespace draco {

    // implementation details for traits

    template<typename T, T V>
    struct integral_constant
    {
        static constexpr T value = V;
        using value_type = T;
        using type = integral_constant;
        constexpr operator value_type() const noexcept { return value; }
        constexpr value_type operator()() const noexcept { return value; }
    };

    template<bool Value>
    using bool_constant = integral_constant<bool, Value>;

    using true_type = integral_constant<bool, true>;
    using false_type = integral_constant<bool, false>;

    // Traits and Concepts
    template <typename T>
    inline constexpr bool is_trivial_v = __is_trivial(T); // GCC/Clang primitive

    template<typename T, typename U>
    struct same_type : false_type {};

    template<typename T>
    struct same_type<T, T> : true_type {};

    template<typename T, typename U>
    concept same = same_type<T, U>::value;

    template<typename T>
    concept without_sign =
        same<T, unsigned char> || same<T, unsigned short> ||
        same<T, unsigned int> || same<T, unsigned long> || same<T, unsigned long long>;

    template<typename T>
    concept with_sign =
        same<T, char> || same<T, short> || same<T, int> || same<T, long> || same<T, long long>;

    template<typename T>
    concept integral = with_sign<T> || without_sign<T>;

    template<typename T>
    concept floating_point = same<T, float> || same<T, double> || same<T, long double>;

    template<typename T>
    concept arithmetic = integral<T> || floating_point<T>;

    template<typename T>
    concept trivial = is_trivial_v<T>;

    // Whether the default value of a type is just all-0 bytes.
    // This can most commonly be exploited by using memset for these types instead of loop-construct.
    // Must be explicitly specialized to mark a type as such.
    template <typename T>
    struct is_zero_constructible : false_type {};

    template <typename T>
    constexpr bool is_zero_constructible_v = is_zero_constructible<T>::value;

    template <typename T>
    concept zero_constructible = is_zero_constructible_v<T>;
}