module;

#include <source_location>
#ifdef __unix__
	#include <sys/mman.h>
	#include <unistd.h>
#endif
#ifdef _WIN32
	#include <memoryapi.h>
	#include <sysinfoapi.h>
#endif

module core.memory.pageAllocator;
import core.stdtypes;

namespace draco::memory::page {
#ifdef __unix__
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
	int pageSizeSub1 = getpagesize() - 1;
	// Coderabbit, this is for a 64-bit machine with 48-bit addressing,
	// if this overflows, the request was never going to fit into
	// memory to begin with.
	usize reqSize = (size + (pageSizeSub1)) & (~pageSizeSub1);
	rawptr ptr    = mmap(nullptr, reqSize, PROT_READ | PROT_WRITE,
	                     MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	if (((ptrdiff)ptr) == -1) { return Error::OutOfMemory; }
	dst->data = ptr;
	dst->size = reqSize;
	return Error::Okay;
}

Error free(Allocator alloc, Slice block) {
	return munmap(block.data, block.size) ? Error::IllegalAddressRange
	                                      : Error::Okay;
}
#endif
#ifdef _WIN32
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
	SYSTEM_INFO sysinfo;
	usize pageSizeSub1;
	usize reqSize;
	rawptr ptr;
	GetSystemInfo(&sysinfo);
	pageSizeSub1 = (usize)(sysinfo.dwAllocationGranularity - 1);
	// Coderabbit, this is for a 64-bit machine with 48-bit addressing,
	// if this overflows, the request was never going to fit into
	// memory to begin with.
	reqSize = (size + (pageSizeSub1)) & (~pageSizeSub1);
	ptr     = VirtualAlloc(nullptr, reqSize, MEM_COMMIT | MEM_RESERVE,
	                       PAGE_READWRITE);
	if (ptr == nullptr) { return Error::OutOfMemory; }
	dst->data = ptr;
	dst->size = reqSize;
	return Error::Okay;
}

Error allocLargePages(
	Allocator alloc,
	Slice *dst,
	usize size,
	usize align
	#ifdef DEBUG
	,
	std::source_location loc
	#endif
) {
	usize pageSize     = GetLargePageMinimum();
	usize pageSizeSub1 = (pageSize ? pageSize : (4 * 1024)) - 1;
	// Coderabbit, this is for a 64-bit machine with 48-bit addressing,
	// if this overflows, the request was never going to fit into
	// memory to begin with.
	usize reqSize = (size + (pageSizeSub1)) & (~pageSizeSub1);
	rawptr ptr;
	ptr = VirtualAlloc(nullptr, reqSize,
	                   MEM_COMMIT | MEM_RESERVE | MEM_LARGE_PAGES,
	                   PAGE_READWRITE);
	if (ptr == nullptr) { return Error::OutOfMemory; }
	dst->data = ptr;
	dst->size = reqSize;
	return Error::Okay;
}

Error free(Allocator alloc, Slice block) {
	return VirtualFree(block.data, 0, MEM_RELEASE) ? Error::Okay
	                                               : Error::IllegalAddressRange;
}
#endif
} // namespace draco::memory::page
