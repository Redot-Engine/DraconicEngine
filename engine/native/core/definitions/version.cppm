export module core.version;
export import std;
import std.compat;

export namespace draco {

    struct Version {
        uint16_t major;
        uint16_t minor;
        uint16_t patch;
    };

    constexpr Version VERSION { .major = 2026, .minor = 0, .patch = 0 };
}

export namespace std {
    template<> struct formatter<draco::Version> {
        constexpr auto parse(std::format_parse_context& ctx) {
            return ctx.begin(); // Accept any format spec (or parse custom ones)
        }

        auto format(const draco::Version& v, std::format_context& ctx) const {
            return std::format_to(ctx.out(), "{}.{}.{}", v.major, v.minor, v.patch);
        }
    };
}