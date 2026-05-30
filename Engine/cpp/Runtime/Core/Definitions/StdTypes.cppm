module;

#include <cstddef>
#include <cstdint>

export module core.stdtypes;

export namespace draco {
using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

using uint = unsigned int;
using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using f32 = float;
using f64 = double;

using isize = int64_t;
using usize = std::size_t;

using rawptr = void *;
using uintptr = uintptr_t;
using ptrdiff = ptrdiff_t;

// Unicode types
using utf8 = char8_t;
using utf16 = char16_t;
using utf32 = char32_t;
using rune = char32_t;

} // namespace draco
