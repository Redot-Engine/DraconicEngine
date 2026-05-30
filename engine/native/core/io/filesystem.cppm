module;

#include <vector>
#include <string>
#include <fstream>

export module core.io.filesystem;

import core.stdtypes;

export namespace draco::core::io::filesystem {
// Returns a buffer of the file data
std::vector<u8> load_binary(std::string const &path);
} // namespace draco::core::io::filesystem
