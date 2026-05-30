module;

#include <bit>
#include <cassert>
#include <source_location>

module core.memory.fixedAllocator;
import core.stdtypes;

namespace draco::memory::fixed {
void init(FixedAllocator *alloc, Slice block) {
	alloc->buffer    = (u8 *)block.data;
	alloc->size      = block.size;
	alloc->allocated = false;
}

Error alloc(
	Allocator alloc,
	Slice *dst,
	usize size,
	usize align
#ifdef DEBUG
	,
	std::source_location loc
#endif
) {
	FixedAllocator *allocData = (FixedAllocator *)alloc.allocatorData;
	usize alignMask           = align - 1;
	usize alignedSize =
		allocData->size -
		((align - (((uintptr)allocData->buffer) & alignMask)) & alignMask);
	assert(std::popcount(align) == 1);
	if (allocData->allocated | (alignedSize < size)) {
		return Error::OutOfMemory;
	}
	dst->data =
		(rawptr)(((uintptr) & (allocData->buffer[alignMask])) & ~alignMask);
	dst->size            = alignedSize;
	allocData->allocated = true;
	return Error::Okay;
}

Error freeAll(Allocator alloc) {
	FixedAllocator *allocData = (FixedAllocator *)alloc.allocatorData;
	allocData->allocated      = false;
	return Error::Okay;
}
} // namespace draco::memory::fixed
