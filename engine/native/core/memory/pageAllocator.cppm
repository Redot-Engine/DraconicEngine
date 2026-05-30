module;

#include <source_location>

export module core.memory.pageAllocator;
export import core.memory.allocator;
export import core.memory.slice;
export import core.stdtypes;

export namespace draco::memory {
namespace page {
Error alloc(
	Allocator alloc, Slice *dst, usize size, usize align
#ifdef DEBUG
	,
	std::source_location loc
#endif
);

Error free(Allocator alloc, Slice block);

AllocatorVTbl pageAllocatorVtbl = {
	.alloc   = alloc,
	.free    = free,
	.freeAll = nilFreeAll,
};
Allocator pageAllocator = {
	.vtbl          = &pageAllocatorVtbl,
	.allocatorData = nullptr,
};
} // namespace page
} // namespace draco::memory
