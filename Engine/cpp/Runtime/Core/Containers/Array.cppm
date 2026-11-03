module;

#include <cassert>
#include <cstring>
#include <new>
#include <type_traits>
#include <utility>

export module core.containers.array;
export import core.stdtypes;
export import core.memory;

import core.defs;
import core.math.functions;

using namespace draco::memory;

export namespace draco::containers
{

    /// @brief Type-erased structure handling the internal allocation & layout logic of arrays.
    /// This keeps heavy looping & re-allocation code out of template instantiations.
    struct UntypedArray
    {
        Allocator allocator;
        void *buffer;
        isize size;
        isize capacity;

        enum class Error
        {
            Okay = (int)memory::Error::Okay,
            OutOfMemory,
            InvalidArgument
        };

        using RelocateFn = void (*)(void *dst, void *src, isize count);
        using DestroyFn = void (*)(void *data, isize count);

        UntypedArray(Allocator alloc) : allocator{alloc}, buffer{nullptr}, size{0}, capacity{0} {}

        /// @brief Reserves capacity for at least the specified number of items.
        /// @param newCapacity Requested element capacity.
        /// @param itemSize Size of each element in bytes.
        /// @param itemAlign Alignment requirement of the elements.
        /// @param relocate Callback to move elements to new memory storage.
        /// @param destroy Callback to destruct active elements.
        /// @return Error code indicating success or out of memory conditions.
        Error reserve(isize newCapacity, isize itemSize, isize itemAlign, RelocateFn relocate, DestroyFn destroy)
        {
            if (newCapacity < 0 || itemSize <= 0)
                return Error::InvalidArgument;

            if (newCapacity <= capacity)
                return Error::Okay;

            isize maxRepresentable = ~((isize)0) & ~(isize)((unsigned long long)1 << (sizeof(isize) * 8 - 1));
            if (newCapacity > maxRepresentable / itemSize)
                return Error::OutOfMemory;

            Slice newDst;
            auto memoryErr = allocator.alloc(&newDst, newCapacity * itemSize, itemAlign);
            if (memoryErr != memory::Error::Okay)
            {
                return Error::OutOfMemory;
            }

            if (buffer)
            {
                if (relocate)
                {
                    try
                    {
                        relocate(newDst.data, buffer, size);
                    }
                    catch (...)
                    {
                        allocator.free(newDst);
                        throw;
                    }
                }
                else
                {
                    std::memcpy(newDst.data, buffer, size * itemSize);
                }

                if (destroy)
                {
                    destroy(buffer, size);
                }

                Slice oldDst = {.data = buffer, .size = (usize)capacity * itemSize};
                allocator.free(oldDst);
            }

            capacity = newCapacity;
            buffer = newDst.data;
            return Error::Okay;
        }

        /// @brief Grows the capacity geometric policy tracking to guarantee efficient push operations.
        /// @param minCapacity Minimum total element count capacity required.
        /// @param itemSize Size of each element in bytes.
        /// @param itemAlign Alignment requirement of the elements.
        /// @param relocate Callback to move elements to new memory storage.
        /// @param destroy Callback to destruct active elements.
        /// @return Error code indicating success or out of memory conditions.
        Error growCapacity(isize minCapacity, isize itemSize, isize itemAlign, RelocateFn relocate, DestroyFn destroy)
        {
            if (capacity >= minCapacity)
                return Error::Okay;

            isize maxRepresentable = ~((isize)0) & ~(isize)((unsigned long long)1 << (sizeof(isize) * 8 - 1));
            isize delta = draco::math::max(capacity / 2, minCapacity - capacity);
            if (maxRepresentable - capacity < delta)
                return Error::OutOfMemory;

            isize newCapacity = capacity + delta;
            return reserve(newCapacity, itemSize, itemAlign, relocate, destroy);
        }
    };

    /// @brief Type-safe template wrapper over an untyped dynamic continuous storage array.
    /// @tparam T Type of elements stored inside the container.
    template <typename T>
    struct Array
    {
    public:
        using Error = UntypedArray::Error;

    private:
        UntypedArray internal;

        static void relocateElements(void *dst, void *src, isize count)
        {
            T *dstTyped = static_cast<T *>(dst);
            T *srcTyped = static_cast<T *>(src);
            if constexpr (std::is_trivially_copyable_v<T>)
            {
                std::memcpy(dstTyped, srcTyped, count * sizeof(T));
            }
            else
            {
                isize constructed = 0;
                try
                {
                    for (isize i = 0; i < count; ++i)
                    {
                        ::new (static_cast<void *>(dstTyped + i)) T(std::move(srcTyped[i]));
                        constructed++;
                    }
                }
                catch (...)
                {
                    for (isize i = 0; i < constructed; ++i)
                    {
                        dstTyped[i].~T();
                    }
                    throw;
                }
            }
        }

        static void destroyElements(void *data, isize count)
        {
            if constexpr (!std::is_trivially_destructible_v<T>)
            {
                T *typedData = static_cast<T *>(data);
                for (isize i = 0; i < count; ++i)
                {
                    typedData[i].~T();
                }
            }
        }

        template <typename... Args>
        Error emplaceInternal(Args &&...args)
        {
            isize maxRepresentable = ~((isize)0) & ~(isize)((unsigned long long)1 << (sizeof(isize) * 8 - 1));
            if (internal.size >= maxRepresentable)
                return Error::OutOfMemory;

            isize reqCapacity = internal.size + 1;
            if (reqCapacity <= internal.capacity)
            {
                ::new (static_cast<void *>(static_cast<T *>(internal.buffer) + internal.size)) T(std::forward<Args>(args)...);
                internal.size++;
                return Error::Okay;
            }

            UntypedArray newStorage(internal.allocator);
            Error err = newStorage.growCapacity(reqCapacity, sizeof(T), alignof(T), nullptr, nullptr);
            if (err != Error::Okay)
                return err;

            T *newBuf = static_cast<T *>(newStorage.buffer);
            T *oldBuf = static_cast<T *>(internal.buffer);

            ::new (static_cast<void *>(newBuf + internal.size)) T(std::forward<Args>(args)...);

            if (internal.buffer)
            {
                try
                {
                    relocateElements(newBuf, oldBuf, internal.size);
                }
                catch (...)
                {
                    newBuf[internal.size].~T();
                    Slice newSlice = {.data = newStorage.buffer, .size = (usize)newStorage.capacity * sizeof(T)};
                    newStorage.allocator.free(newSlice);
                    throw;
                }

                destroyElements(oldBuf, internal.size);
                Slice oldSlice = {.data = internal.buffer, .size = (usize)internal.capacity * sizeof(T)};
                internal.allocator.free(oldSlice);
            }

            internal.buffer = newStorage.buffer;
            internal.capacity = newStorage.capacity;
            internal.size++;
            return Error::Okay;
        }

    public:
        Array(Allocator allocator) : internal(allocator) {}

        ~Array()
        {
            clear();
            if (internal.buffer)
            {
                Slice oldDst = {.data = internal.buffer, .size = (usize)internal.capacity * sizeof(T)};
                internal.allocator.free(oldDst);
            }
        }

        Array(Array const &) = delete;
        Array &operator=(Array const &) = delete;

        Array(Array &&other) noexcept : internal(other.internal.allocator)
        {
            internal.buffer = other.internal.buffer;
            internal.size = other.internal.size;
            internal.capacity = other.internal.capacity;

            other.internal.buffer = nullptr;
            other.internal.size = 0;
            other.internal.capacity = 0;
        }

        Array &operator=(Array &&other) noexcept
        {
            if (this != &other)
            {
                clear();
                if (internal.buffer)
                {
                    Slice oldDst = {.data = internal.buffer, .size = (usize)internal.capacity * sizeof(T)};
                    internal.allocator.free(oldDst);
                }
                internal.allocator = other.internal.allocator;
                internal.buffer = other.internal.buffer;
                internal.size = other.internal.size;
                internal.capacity = other.internal.capacity;

                other.internal.buffer = nullptr;
                other.internal.size = 0;
                other.internal.capacity = 0;
            }
            return *this;
        }

        /// @brief Access element at index with debug bounds verification.
        T &operator[](isize index)
        {
            assert(index >= 0 && index < internal.size);
            return static_cast<T *>(internal.buffer)[index];
        }

        /// @brief Access element at index with debug bounds verification.
        T const &operator[](isize index) const
        {
            assert(index >= 0 && index < internal.size);
            return static_cast<T const *>(internal.buffer)[index];
        }

        /// @brief Reserves space manually for future modifications.
        Error reserve(isize newCapacity)
        {
            return internal.reserve(newCapacity, sizeof(T), alignof(T), relocateElements, destroyElements);
        }

        /// @brief Pushes a copy of an element onto the back of the array.
        Error push(T const &value)
        {
            return emplaceInternal(value);
        }

        /// @brief Pushes a moved element onto the back of the array.
        Error push(T &&value)
        {
            return emplaceInternal(std::move(value));
        }

        /// @brief Constructs an element directly at the back of the array in place.
        template <typename... Args>
        Error emplace(Args &&...args)
        {
            return emplaceInternal(std::forward<Args>(args)...);
        }

        /// @brief Removes the final item of the array & calls destructor logic if applicable.
        void pop()
        {
            if (internal.size > 0)
            {
                internal.size--;
                if constexpr (!std::is_trivially_destructible_v<T>)
                {
                    static_cast<T *>(internal.buffer)[internal.size].~T();
                }
            }
        }

        /// @brief Clear out all elements without altering capacity parameters.
        void clear()
        {
            destroyElements(internal.buffer, internal.size);
            internal.size = 0;
        }

        T *data() { return static_cast<T *>(internal.buffer); }
        T const *data() const { return static_cast<T const *>(internal.buffer); }
        isize size() const { return internal.size; }
        isize capacity() const { return internal.capacity; }
        bool empty() const { return internal.size == 0; }

        T *begin() { return data(); }
        T *end() { return internal.size == 0 ? data() : data() + internal.size; }
        T const *begin() const { return data(); }
        T const *end() const { return internal.size == 0 ? data() : data() + internal.size; }
    };
}
