module;

#include <algorithm>
#include <cstring>
#include <source_location>

module core.memory.trackingAllocator;
import core.memory.slice;

namespace draco::memory::tracking {
void init(TrackingAllocator *alloc, Allocator baseAlloc) {
	alloc->base  = baseAlloc;
	alloc->nodes = nullptr;
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
	TrackingAllocator *allocData = (TrackingAllocator *)alloc.allocatorData;
	usize reqAlign               = std::max(align, alignof(Node));
	usize alignMask              = reqAlign - 1;
	usize reqSize = (size + sizeof(Node) + alignMask) & ~alignMask;
	Slice tmpDst;
	Node *node;
	Error err = allocData->base.vtbl->alloc(
		allocData->base, &tmpDst, reqSize, reqAlign
#ifdef DEBUG
		,
		loc
#endif
	);
	if (err != Error::Okay) { return err; }
	node = (Node *)&(((u8 *)tmpDst.data)[tmpDst.size - sizeof(Node)]);
	memset(node, 0, sizeof(Node));
	if (allocData->nodes) { allocData->nodes->prev = node; }
	node->next       = allocData->nodes;
	allocData->nodes = node;
	tmpDst.size -= sizeof(Node);
	node->details.data = tmpDst;
#ifdef DEBUG
	node->details.loc = loc;
#endif
	*dst = tmpDst;
	return Error::Okay;
}

Error free(Allocator alloc, Slice block) {
	TrackingAllocator *allocData = (TrackingAllocator *)alloc.allocatorData;
	Node *node                   = allocData->nodes;
	Node *prev;
	Node *next;
	Error err;
	while (node && (node->details.data.data != block.data)) {
		node = node->next;
	}
	block.size += sizeof(Node);
	err = allocData->base.vtbl->free(allocData->base, block);
	switch (err) {
	case Error::Okay:
		if (node) {
			prev       = node->prev;
			next       = node->next;
			prev->next = next;
			if (next) { next->prev = prev; }
			memset(node, 0, sizeof(Node));
		}
		[[fallthrough]];
	default: return err;
	}
}

Error freeAll(Allocator alloc) {
	TrackingAllocator *allocData = (TrackingAllocator *)alloc.allocatorData;
	Error err = allocData->base.vtbl->freeAll(allocData->base);
	switch (err) {
	case Error::Okay: allocData->nodes = nullptr; [[fallthrough]];
	default:          return err;
	}
}

void getAnalytics(TrackingAllocator alloc, Analytics *analytics) {
	Node *node = alloc.nodes;
	memset(analytics, 0, sizeof(Analytics));
	while (node != nullptr) {
		analytics->activeAllocationsCount += 1;
		analytics->totalAllocatedBytes += node->details.data.size;
		node = node->next;
	}
}

size_t getActiveAllocations(
	TrackingAllocator alloc,
	size_t detailsCount,
	AllocationDetails *details // nullable
) {
	size_t ret = 0;
	Node *node = alloc.nodes;
	if (details) {
		while ((node != nullptr) & (ret < detailsCount)) {
			details[ret] = node->details;
			node         = node->next;
			ret += 1;
		}
	}
	while (node != nullptr) {
		node = node->next;
		ret += 1;
	}
	return ret;
}
} // namespace draco::memory::tracking
