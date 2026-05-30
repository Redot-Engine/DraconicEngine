module;

#include <cstring>
#include <type_traits>

export module core.containers.string;
export import core.stdtypes;
export import core.memory;

import core.defs;
import core.math.functions;
import core.memory.fixedAllocator;

using namespace draco::memory;

export namespace draco::containers {

// Immutable String
// using utf-16 encoding for compatability with C#
// Signed size as that allows for us to reason about math operations easier
// with the only downside being we can only have strings as large as 2^63 (oh nooo)
struct String {
	utf16 const *text;
	isize size;

	utf16 const &operator[](isize index) const {
		// TODO(Jon) assert(index < size);
		return text[index];
	}

	int constexpr StringCompare(String s) const;
	bool constexpr operator==(String s) const {
		return StringCompare(s) == 0;
	}
	bool constexpr operator!=(String s) const {
		return StringCompare(s) != 0;
	}
	bool constexpr operator>(String s) const {
		return StringCompare(s) > 0;
	}
	bool constexpr operator>=(String s) const {
		return StringCompare(s) >= 0;
	}
	bool constexpr operator<(String s) const {
		return StringCompare(s) < 0;
	}
	bool constexpr operator<=(String s) const {
		return StringCompare(s) <= 0;
	}

	///  Creates a substring of the String
	///
	///  @param pos The start of the substring
	///  @param len The length of the substring, if exceeds size of string, will truncate to end of string
	///
	///  @return Requested substring
	String constexpr Substr(isize pos, isize len) const;

	bool constexpr StartsWith(String prefix) const;
	bool constexpr EndsWith(String suffix) const;
	isize constexpr FindFirst(utf16 c) const;
	isize constexpr FindLast(utf16 c) const;
};
String constexpr const EMPTY_STRING = { nullptr, 0 };

struct StringIterator {
	utf16 const *start;
	utf16 const *end;

	constexpr StringIterator(String s) : start{ s.text }, end{ s.text ? s.text + s.size : nullptr } {}

	/// Advances to next canonical character
	///
	/// @param[out] codepoint The utf-32 codepoint that was advanced over
	///
	/// @return true if codepoint is a valid character that can be used.
	///         false if codepoint is invalid. Still advances past the character on failure.
	///
	bool constexpr Advance(rune *codepoint);
};

struct StringBuilder {
	Allocator allocator;
	utf16 *buffer;
	isize size;
	isize capacity;

	// TODO(Jon) figure out better solution for adding InvalidRune error to Memory Errors
	enum class Error {
		Okay = (int)memory::Error::Okay,
		OutOfMemory,
		NotImplemented,
		IllegalAddressRange,
		InvalidRune
	};

	StringBuilder(Allocator allocator) : allocator{ allocator }, buffer{ nullptr }, size{ 0 }, capacity{ 0 } {}

	String constexpr GetString() const {
		return String{ buffer, size };
	}
	void constexpr Reset() {
		size = 0;
	}

	/// Set exact capacity, will not shrink capacity
	Error Reserve(isize newCapacity);

	/// Changes capacity to allow for at minimum the passed in capacity
	/// Will grow capacity according to a growth policy
	Error GrowCapacity(isize minCapacity);

	/// Write UTF-16 character literal.
	///  example: u'a'
	Error Write(utf16 c);

	/// Write rune to string, converting to utf16
	/// Returns Error::Other if rune is outside valid range
	Error Write(rune r);

	/// Write utf-16 encoded null-terminated string
	///  example: u"Hello World"
	Error Write(utf16 const *str);

	/// Copies contents of str into StringBuilder
	Error Write(String str);

	/// Writes Integer with specified base
	/// Supports base 2, 10 and 16
	template <integral T>
	Error WriteInt(T val, int base = 10);

	// TODO(Jon) Functions
	// Format function to write into string with format specifiers
};

/* STRING */

/// Acquire the length of a utf-16 encoded C string
/// Should not be needed largely but given just incase.
isize constexpr StringLength(utf16 const *str) {
	// TODO(Jon) This can be enhanced with SIMD checks
	utf16 const *start = str;

	// Align string to 8 byte boundary
	while ((uintptr)str & 7) {
		if (*str == 0) {
			return str - start;
		}
		str++;
	}

	u64 const *p = (u64 const *)str;

	for (;;) {
		u64 x = *p++;

		/* Test to see if x contains any 0s in the 16 bit chunks
		 *
		 * Extended form of this
		 * (x - 1) & ~x & 0x80;
		 *
		 * a = (0x0000 - 0x0001) = 0xFFFF
		 * b = ~0x0000 = 0xFFFF
		 * c = a & b = 0xFFFF
		 * c & 0x8000 = 0x8000; ZERO Found
		 */
		u64 m = ((x - 0x0001'0001'0001'0001ULL) & ~x & 0x8000'8000'8000'8000ULL);

		if (m) {
			str = (utf16 const *)(p - 1);

			for (int i = 0; i < 4; ++i) {
				if (str[i] == 0) {
					return (str + i) - start;
				}
			}
		}
	}
}

int constexpr String::StringCompare(String s) const {
	if (text == nullptr || s.text == nullptr) {
		if (text == s.text) {
			return 0;
		}
		return text == nullptr ? -1 : 1;
	}

	isize min_size = draco::math::min(size, s.size);

	// TODO(Jon) Can probably increase performance of this comparison in the future
	for (isize i = 0; i < min_size; ++i) {
		if (text[i] != s.text[i]) {
			return text[i] < s.text[i] ? -1 : 1;
		}
	}

	return size < s.size ? -1 : (size > s.size ? 1 : 0);
}

String constexpr String::Substr(isize pos, isize len) const {
	if (pos < 0 || len <= 0 || pos >= size) {
		return EMPTY_STRING;
	}

	isize clampedLen = draco::math::min(len, size - pos);
	return { text + pos, clampedLen };
}

bool constexpr String::StartsWith(String prefix) const {
	if (prefix.size > size) {
		return false;
	}

	return Substr(0, prefix.size) == prefix;
}

bool constexpr String::EndsWith(String suffix) const {
	if (suffix.size > size) {
		return false;
	}

	return Substr(size - suffix.size, suffix.size) == suffix;
}

isize constexpr String::FindFirst(utf16 c) const {
	for (isize i = 0; i < size; i++) {
		if (text[i] == c) {
			return i;
		}
	}
	return size;
}

isize constexpr String::FindLast(utf16 c) const {
	// Advantage of using signed size is this is perfectly valid
	for (isize i = size - 1; i >= 0; i--) {
		if (text[i] == c) {
			return i;
		}
	}
	return size;
}

/* TODO(Jon) Functions
 * - Split by character into Array of Strings
 * - Find First/Last of substring
 * - Trim Whitespace front and back, which involves some Unicode shenanigans. See: https://www.unicode.org/Public/UCD/latest/ucd/PropList.txt
 * - To upper/lower case
 * - Equals fold, check if two strings are equal ignoring case in a Unicode fashion
 */

/* STRING ITERATOR  */

bool constexpr StringIterator::Advance(rune *codepoint) {
	if (start >= end) {
		return false;
	}
	utf16 high = *start++;

	// Values from 0xD800 and 0xDFFF denote a surrogate
	if (high < 0xD800 || high >= 0xE000) {
		*codepoint = high;
		return true;
	}

	// Malformed String Iterator checks, would mean bug in code
	// TODO(Jon) add logging or some other behavior here

	// Misplaced low surrogate value
	if (high >= 0xDC00) {
		return false;
	}
	if (start >= end) {
		return false;
	}

	utf16 low = *start++;

	if (low < 0xDC00 || low > 0xDFFF) {
		return false;
	}

	high -= 0xD800;
	low -= 0xDC00;

	*codepoint = 0x10000 + (((rune)high << 10) | (rune)low);
	return true;
}

/* STRING BUILDER */

template <integral T>
StringBuilder::Error StringBuilder::WriteInt(T val, int base) {
	// Backwards since the algorithm reverses the string after creating it
	static constexpr utf16 binaryNibbles[][4] = {
		{ u'0', u'0', u'0', u'0' },
		{ u'1', u'0', u'0', u'0' },
		{ u'0', u'1', u'0', u'0' },
		{ u'1', u'1', u'0', u'0' },
		{ u'0', u'0', u'1', u'0' },
		{ u'1', u'0', u'1', u'0' },
		{ u'0', u'1', u'1', u'0' },
		{ u'1', u'1', u'1', u'0' },
		{ u'0', u'0', u'0', u'1' },
		{ u'1', u'0', u'0', u'1' },
		{ u'0', u'1', u'0', u'1' },
		{ u'1', u'1', u'0', u'1' },
		{ u'0', u'0', u'1', u'1' },
		{ u'1', u'0', u'1', u'1' },
		{ u'0', u'1', u'1', u'1' },
		{ u'1', u'1', u'1', u'1' },
	};
	static constexpr utf16 hexadecimalNibbles[] = {
		u'0',
		u'1',
		u'2',
		u'3',
		u'4',
		u'5',
		u'6',
		u'7',
		u'8',
		u'9',
		u'A',
		u'B',
		u'C',
		u'D',
		u'E',
		u'F',
	};

	utf16 buff[sizeof(T) * 8];
	isize index = 0;
	bool sign = false;

	using U = std::make_unsigned_t<T>;
	U unsignedVal = val;
	if constexpr (std::is_signed_v<T>) {
		if (val < 0) {
			unsignedVal = U{} - static_cast<U>(val);
			sign = true;
		} else {
			unsignedVal = static_cast<U>(val);
		}
	}

	if (unsignedVal == 0) {
		buff[index++] = u'0';
	} else if (base == 2) {
		while (unsignedVal != 0) {
			std::memcpy(buff + index, binaryNibbles[unsignedVal & 0xF], sizeof(binaryNibbles[0]));
			index += 4;
			unsignedVal >>= 4;
		}
	} else if (base == 16) {
		while (unsignedVal != 0) {
			buff[index++] = hexadecimalNibbles[unsignedVal & 0xF];
			unsignedVal >>= 4;
		}
	} else {
		while (unsignedVal != 0) {
			buff[index++] = u'0' + (unsignedVal % 10);
			unsignedVal /= 10;
		}
	}

	Error err = GrowCapacity(size + index + sign);
	if (err != Error::Okay) {
		return err;
	}

	if (sign) {
		buffer[size++] = u'-';
	}

	isize origIndex = index--;
	for (isize i = 0; i < origIndex; i++, index--) {
		buffer[i + size] = buff[index];
	}
	size += origIndex;

	return Error::Okay;
}

} // namespace draco::containers
