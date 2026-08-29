// Single translation unit for stb implementations.
// Module units cannot define these macros (static symbol collisions),
// so the implementations live in a regular .cpp.

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO_WINDOWS_UTF8 0
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
