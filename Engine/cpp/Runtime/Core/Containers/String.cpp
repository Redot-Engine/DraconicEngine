module;

#include <cstring>

module core.containers.string;

namespace draco::containers {

StringBuilder::Error StringBuilder::Reserve(isize newCapacity) {
	if (newCapacity <= capacity) {
		return Error::Okay;
	}

	Slice newDst;
	StringBuilder::Error err = (StringBuilder::Error)allocator.alloc(&newDst, newCapacity * sizeof(utf16), alignof(utf16));
	if (err != Error::Okay) {
		return err;
	}

	if (buffer) {
		// TODO(Jon) Replace with Slice version when created
		memcpy(newDst.data, buffer, size * sizeof(utf16));

		Slice oldDst = {
			.data = buffer,
			.size = (usize)capacity * sizeof(utf16)
		};
		allocator.free(oldDst);
	}

	capacity = newDst.size / sizeof(utf16);
	buffer = (utf16 *)newDst.data;

	return Error::Okay;
}

StringBuilder::Error StringBuilder::GrowCapacity(isize minCapacity) {
	if (capacity >= minCapacity) {
		return Error::Okay;
	}

	isize newCapacity = capacity + draco::math::max(capacity / 2, minCapacity);
	return Reserve(newCapacity);
}

StringBuilder::Error StringBuilder::Write(utf16 c) {
	Error err = GrowCapacity(size + 1);
	if (err != Error::Okay) {
		return err;
	}

	buffer[size++] = c;
	return Error::Okay;
}

StringBuilder::Error StringBuilder::Write(rune r) {
	if (r > 0x10FFFF || (r >= 0xD800 && r <= 0xDFFF)) {
		return Error::InvalidRune;
	}

	Error err = GrowCapacity(size + 2);
	if (err != Error::Okay) {
		return err;
	}

	if (r <= 0xFFFF) {
		buffer[size++] = (utf16)r;
		return Error::Okay;
	}

	r -= 0x10000;

	utf16 high = 0xD800 + (r >> 10);
	utf16 low = 0xDC00 + (r & 0x3FF);

	buffer[size++] = high;
	buffer[size++] = low;

	return Error::Okay;
}

StringBuilder::Error StringBuilder::Write(utf16 const *str) {
	// TODO(Jon) assert not null
	isize length = StringLength(str);
	Error err = GrowCapacity(size + length);
	if (err != Error::Okay) {
		return err;
	}
	memcpy(buffer + size, str, length * sizeof(utf16));
	size += length;
	return Error::Okay;
}

StringBuilder::Error StringBuilder::Write(String str) {
	Error err = GrowCapacity(size + str.size);
	if (err != Error::Okay) {
		return err;
	}
	memcpy(buffer + size, str.text, str.size * sizeof(utf16));
	size += str.size;
	return Error::Okay;
}

} //namespace draco::containers
