module;

#include <source_location>

export module core.memory.trackingAllocator;
export import core.memory.allocator;
export import core.memory.slice;

export namespace draco::memory {
namespace tracking {
struct Analytics {
	usize totalAllocatedBytes;
	usize activeAllocationsCount;
};

// TODO(Victor Sohier): Set this up to handle stack traces when clang finally
// supports them
struct AllocationDetails {
	Slice data;
#ifdef DEBUG
	std::source_location loc;
#endif
};

struct Node {
	Node *prev;
	Node *next;
	AllocationDetails details;
};

struct TrackingAllocator {
	Allocator base;
	Node *nodes;
};

void init(TrackingAllocator *alloc, Allocator baseAlloc);

Error alloc(
	Allocator alloc, Slice *dst, usize size, usize align
#ifdef DEBUG
	,
	std::source_location loc
#endif
);
Error free(Allocator alloc, Slice block);
Error freeAll(Allocator alloc);
void getAnalytics(TrackingAllocator alloc, Analytics *analytics);
usize getActiveAllocations(TrackingAllocator alloc, usize detailsCount, AllocationDetails *details);

AllocatorVTbl trackingAllocatorVtbl = {
	.alloc   = alloc,
	.free    = free,
	.freeAll = freeAll,
};

inline void asAllocator(Allocator *dst, TrackingAllocator *alloc) {
	asAllocatorVoid(dst, (void *)alloc, &trackingAllocatorVtbl);
}
} // namespace tracking
} // namespace draco::memory
