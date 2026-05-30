module;

#include <cassert>
#include <source_location>

export module core.memory.bumpAllocator;
export import core.memory.allocator;
export import core.memory.slice;
export import core.stdtypes;

export namespace draco::memory {
namespace bump {
struct Node {
	Node *next;
	usize size;
	u8 data[];
};

struct BumpAllocator {
	Allocator base;
	Node *first;
	usize minAllocRequest;
	usize allocated;
};

void init(BumpAllocator *alloc,
          Allocator baseAlloc,
          // one page by default on unix-like systems
          usize minAllocRequest = (1 << 12));

void deinit(BumpAllocator *alloc);

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

AllocatorVTbl bumpAllocatorVtbl = {
	.alloc   = alloc,
	.free    = nilFree,
	.freeAll = freeAll,
};

usize saveMark(BumpAllocator *self);

void resumeMark(BumpAllocator *self, usize mark);

inline void asAllocator(Allocator *dst, BumpAllocator *alloc) {
	asAllocatorVoid(dst, (void *)alloc, &bumpAllocatorVtbl);
}
} // namespace bump
} // namespace draco::memory
