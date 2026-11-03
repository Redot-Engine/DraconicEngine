module;

#include <cassert>

export module core.containers.span;
export import core.stdtypes;

export namespace draco::containers {

    /// @brief Non-owning representation context view covering contiguous element series.
    /// @tparam T Tracked structural raw data sequence reference variant.
    template <typename T>
    struct Span
    {
    private:
        T *buffer;
        isize count;

    public:
        constexpr Span() : buffer(nullptr), count(0) {}
        constexpr Span(T *ptr, isize size) : buffer(size == 0 ? nullptr : ptr), count(size) 
        {
            assert(size >= 0);
        }
        constexpr Span(T *first, T *last) : buffer(first == last ? nullptr : first), count(static_cast<isize>(last - first)) 
        {
            assert(count >= 0);
        }

        template <usize N>
        constexpr Span(T (&arr)[N]) : buffer(arr), count(static_cast<isize>(N)) {}

        /// @brief Access data item at sequence index location. Bound validated.
        constexpr T &operator[](isize index) const
        {
            assert(index >= 0 && index < count);
            return buffer[index];
        }

        constexpr T *data() const { return buffer; }
        constexpr isize size() const { return count; }
        constexpr bool empty() const { return count == 0; }

        constexpr T *begin() const { return buffer; }
        constexpr T *end() const { return count == 0 ? buffer : buffer + count; }

        /// @brief Returns reference to the first item with size validation.
        constexpr T &front() const
        {
            assert(count > 0);
            return buffer[0];
        }

        /// @brief Returns reference to the trailing element block item with size validation.
        constexpr T &back() const
        {
            assert(count > 0);
            return buffer[count - 1];
        }

        /// @brief Pulls a subview span section range from tracking window space.
        /// @param offset Origin boundary skip value index position pointer.
        /// @param length Subspan capacity tracker scale indicator. -1 computes automatic tracking sizes.
        constexpr Span<T> subspan(isize offset, isize length = -1) const
        {
            assert(offset >= 0 && offset <= count);
            assert(length >= -1);
            isize realLength = (length == -1) ? (count - offset) : length;
            assert(realLength <= count - offset);
            return realLength == 0 ? Span<T>() : Span<T>(buffer ? buffer + offset : nullptr, realLength);
        }
    };
}
