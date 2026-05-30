module;

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstring>
#include <source_location>

module core.memory.bumpAllocator;
import core.stdtypes;

namespace draco::memory::bump {
void init(BumpAllocator *alloc, Allocator baseAlloc,
          // one page by default on unix-like systems
          usize minAllocRequest) {
	memset(alloc, 0, sizeof(BumpAllocator));
	alloc->base            = baseAlloc;
	alloc->minAllocRequest = minAllocRequest;
}

void deinit(BumpAllocator *alloc) {
	Node *lastNode;
	Node *node = alloc->first;
	while (node != nullptr) {
		lastNode = node;
		node     = node->next;
		alloc->base.vtbl->free(alloc->base, {
												.data = (void *)lastNode,
												.size = lastNode->size + sizeof(Node),
											});
	}
}

Error alloc(
	Allocator alloc, Slice *dst, usize size, usize align
#ifdef DEBUG
	,
	std::source_location loc
#endif
) {
	Error err;
	BumpAllocator *allocData = (BumpAllocator *)alloc.allocatorData;
	uintptr alignMask        = align - 1;
	Node **lastNode;
	Node **node     = &(allocData->first);
	usize pos       = allocData->allocated;
	usize oldPos    = pos;
	usize reqSize   = size;
	usize spillover = 0;
	Slice newBlock;
	uintptr currentPtr;
	assert(std::popcount(align) == 1);
	lastNode = node;
	while (((*node) != nullptr) & (pos > 0)) {
		oldPos = pos;
		pos -= std::min((*node)->size, pos);
		lastNode = node;
		node     = &((*node)->next);
	}
	assert(pos == 0); // fraudulent mark provided
	currentPtr = ((uintptr)(*lastNode)) + sizeof(Node) + oldPos;
	reqSize    = size + ((align - (currentPtr & alignMask)) & alignMask);
	if (!(*lastNode) || (reqSize > ((*lastNode)->size - oldPos))) {
		if (*lastNode) { spillover = ((*lastNode)->size - oldPos); }
		reqSize = (sizeof(Node) + size + alignMask) & ~alignMask;
		err     = allocData->base.vtbl->alloc(
			allocData->base, &newBlock, std::max(allocData->minAllocRequest, reqSize), std::max(alignof(Node), align)
#ifdef DEBUG
																						   ,
			loc
#endif
		);
		if (err != Error::Okay) { return Error::OutOfMemory; }
		(*node)       = (Node *)newBlock.data;
		(*node)->next = nullptr;
		(*node)->size = newBlock.size - sizeof(Node);
		pos           = 0;
		lastNode      = node;
		oldPos        = 0;
	}
	currentPtr = ((uintptr) & ((*lastNode)->data[oldPos]));
	reqSize    = size + ((align - (currentPtr & alignMask)) & alignMask);
	currentPtr = (currentPtr + alignMask) & ~alignMask;
	allocData->allocated += reqSize + spillover;
	dst->data = (void *)currentPtr;
	dst->size = size;
	return Error::Okay;
}

Error freeAll(Allocator alloc) {
	BumpAllocator *allocData = (BumpAllocator *)alloc.allocatorData;
	allocData->allocated     = 0;
	return Error::Okay;
}

usize saveMark(BumpAllocator *self) {
	return self->allocated;
}

void resumeMark(BumpAllocator *self, usize mark) {
	self->allocated = mark;
}
} // namespace draco::memory::bump
