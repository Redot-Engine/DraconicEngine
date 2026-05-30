module;

#include <format>

export module core.version;
import core.stdtypes;

export namespace draco {

struct Version {
	u16 major;
	u16 minor;
	u16 patch;
};

constexpr Version VERSION{.major = 2026, .minor = 0, .patch = 0};
} // namespace draco

export namespace std {
template<> struct formatter<draco::Version> {
	constexpr auto parse(std::format_parse_context &ctx) {
		return ctx.begin(); // Accept any format spec (or parse custom ones)
	}

	auto format(draco::Version const &v, std::format_context &ctx) const {
		return std::format_to(ctx.out(), "{}.{}.{}", v.major, v.minor, v.patch);
	}
};

} // namespace std
