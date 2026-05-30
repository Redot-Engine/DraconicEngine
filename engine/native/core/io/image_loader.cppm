module;

#include <vector>
#include <cstdint>
#include <filesystem>

export module core.io.image_loader;

import core.stdtypes;

export namespace draco::core::io::image_loader {
struct ImageData {
	std::vector<u8> pixels;
	u32 width     = 0;
	u32 height    = 0;
	u8 channels   = 0;
	bool is_valid = false;
};

// Load an image file (PNG, JPG, etc) & decode it to raw RGBA8
ImageData load_image(std::filesystem::path const &path);
} // namespace draco::core::io::image_loader
