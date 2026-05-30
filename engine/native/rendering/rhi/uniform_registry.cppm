module;

#include <unordered_map>
#include <string>
#include <functional>
#include <cstdint>

export module rendering.rhi.uniform_registry;

import rendering.rhi;

export namespace draco::rendering::rhi {
inline std::unordered_map<uint32_t, UniformHandle> g_uniform_map;

inline uint32_t hash_uniform(std::string const &name) {
	return static_cast<uint32_t>(std::hash<std::string>{}(name));
}

inline void register_uniform(uint32_t hash, UniformHandle h) {
	g_uniform_map[hash] = h;
}

inline void unregister_uniform(uint32_t hash, UniformHandle h) {
	auto it = g_uniform_map.find(hash);

	if (it != g_uniform_map.end() && it->second == h) {
		g_uniform_map.erase(it);
	}
}

inline void clear_uniform_registry() {
	g_uniform_map.clear();
}

inline UniformHandle get_uniform(uint32_t hash) {
	auto it = g_uniform_map.find(hash);

	if (it == g_uniform_map.end()) { return InvalidUniform; }

	return it->second;
}
} // namespace draco::rendering::rhi
