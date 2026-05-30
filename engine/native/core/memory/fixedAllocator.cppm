module;

#include <cassert>
#include <source_location>

export module core.memory.fixedAllocator;
export import core.memory.allocator;
export import core.memory.slice;
export import core.stdtypes;

export namespace draco::memory {
namespace fixed {
struct FixedAllocator {
	u8 *buffer;
	usize size;
	bool allocated;
};

void init(FixedAllocator *alloc, Slice block);

Error alloc(
	Allocator alloc,
	Slice *dst,
	usize size,
	usize align
#ifdef DEBUG
	,
	std::source_location loc
#endif
);

Error freeAll(Allocator alloc);

AllocatorVTbl fixedAllocatorVtbl = {
	.alloc   = alloc,
	.free    = nilFree,
	.freeAll = freeAll,
};

inline void asAllocator(Allocator *dst, FixedAllocator *alloc) {
	asAllocatorVoid(dst, (void *)alloc, &fixedAllocatorVtbl);
}
} // namespace fixed
} // namespace draco::memory
