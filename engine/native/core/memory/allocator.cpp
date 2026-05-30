module;

#include <source_location>

module core.memory.allocator;
import core.stdtypes;

namespace draco::memory {
Error nilAlloc(
	Allocator alloc,
	Slice *dst,
	usize size,
	usize align
#ifdef DEBUG
	,
	std::source_location loc
#endif
) {
	return Error::NotImplemented;
}

Error nilFree(Allocator alloc, Slice block) {
	return Error::NotImplemented;
}

Error nilFreeAll(Allocator alloc) {
	return Error::NotImplemented;
}

void asAllocatorVoid(Allocator *dst, rawptr alloc, AllocatorVTbl *vtbl) {
	dst->allocatorData = (void *)alloc;
	dst->vtbl          = vtbl;
}
} // namespace draco::memory
