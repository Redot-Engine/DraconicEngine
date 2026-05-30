module;

#include <vector>
#include <string>
#include <fstream>
#include <print>

module core.io.filesystem;

import core.stdtypes;

namespace draco::core::io::filesystem {
std::vector<u8> load_binary(std::string const &path) {
	// Open at the end (ate) to get size and in binary mode
	std::ifstream file(path, std::ios::binary | std::ios::ate);

	if (!file.is_open()) {
		std::println("Error: Could not open file at: {}", path);
		// Return an empty vector
		return {};
	}

	std::streamsize size = file.tellg();
	if (size < 0) {
		std::println("Error: File is empty or unreadable: {}", path);
		return {};
	}

	if (size == 0) { return {}; }

	file.seekg(0, std::ios::beg);

	std::vector<u8> buffer(static_cast<std::size_t>(size));
	if (file.read(reinterpret_cast<char *>(buffer.data()), size)) { return buffer; }

	std::println("Error: Failed to read file contents: {}", path);
	return {};
}
} // namespace draco::core::io::filesystem
