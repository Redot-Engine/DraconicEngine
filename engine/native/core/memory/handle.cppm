export module core.memory.handle;

import core.stdtypes;
import core.math.constants;

export namespace draco::core::memory {
// Packed generation handle:
// [ 16 bits generation | 16 bits index ]
template<typename Tag>
struct Handle {
	u32 value = math::UINT32_MAX_VAL;

	static constexpr u32 INVALID = math::UINT32_MAX_VAL;

	constexpr Handle() = default;
	constexpr explicit Handle(u32 v) : value(v) { }

	constexpr u16 index() const {
		return static_cast<u16>(value & 0xffff);
	}

	constexpr u16 generation() const {
		return static_cast<u16>(value >> 16);
	}

	constexpr explicit operator bool() const {
		return value != INVALID;
	}

	constexpr bool operator==(Handle const &other) const {
		return value == other.value;
	}

	constexpr bool operator!=(Handle const &other) const {
		return value != other.value;
	}

	static constexpr Handle invalid() {
		return Handle{INVALID};
	}

	static constexpr Handle make(u16 index, u16 generation) {
		return Handle{(static_cast<u32>(generation) << 16) |
		              static_cast<u32>(index)};
	}
};
} // namespace draco::core::memory
