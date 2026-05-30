#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

import core.memory;
import core.stdtypes;

TEST_CASE("Fixed allocator is correctly aligned") {
	using namespace draco::memory;
	alignas(8) draco::u8 buffer[1024];
	fixed::FixedAllocator fixedAlloc;
	Allocator alloc;
	Slice block;
	fixed::init(&fixedAlloc, {.data = buffer, .size = 1024});
	fixed::asAllocator(&alloc, &fixedAlloc);
	alloc.alloc(&block, 512, 16);
	REQUIRE((((uintptr_t)block.data) & 15) == 0);
}
