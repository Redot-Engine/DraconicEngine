# Memory and Allocators

All memory in native code is to be allocated using an allocator, with the
exception of the primitive allocators (namely, the page allocator). This
provides visibility into which functions and data structures do and do not
allocate and makes error handling for these functions more obvious. This also
provides additional thread safety in that there isn't a single, global heap for
the entire application.

The design is based on that of Zig's allocators, of which are also very similar
to Odin's allocators, as essentially just a wide pointer, one to a vtable and
another to the state that vtable operates on.

## Usage

### Initialization and Basic Usage

The current `Allocator` design separates their own resource acquisition and
initialization. The only current exception is the `PageAllocator`, which, due to
its global nature as it integrates with the kernel. The `PageAllocator` can be
acquired from the appropriate global variable.

> [!Warning]
> There is zero guarantee that the allocation you get is zeroed. If that is what
> caller wants, the caller ***must*** zero the allocation itself.

> [!Warning]
> The API only guarantees that you get ***at least*** as much data as requested. The
> true size of the allocation is returned with the `Slice`

```cpp
import core.memory;
import core.stdtypes;

int main()
{
	using namespace draco::memory;
	bump::BumpAllocator bumpAlloc;
	Allocator alloc;
	Error err;
	Slice mem;
	char *cstr;
	// after the fact initialization of the bump allocator
	bump::init(&bumpAlloc, page::pageAllocator); // bumpAlloc's lifetime begins
	bump::asAllocator(&alloc, &bumpAlloc); // get the allocator interface
	// ... code which allocates
	err = alloc.alloc(&mem, 14, 1); // 14 bytes aligned on 1 byte
	// error handling of err
	cstr = (char *)mem.data;
	// ...
	// explicit deinitialization
	bump::deinit(&bumpAlloc); // bumpAlloc's lifetime ends
	// Anything allocated on bumpAlloc is now invalid!
}
```

### (Free) Functions

Functions which allocate directly ***must*** take an `Allocator` as a parameter with the
exception of the program's entry point. This makes it clear that the function
allocates. If the function can't handle `Allocator` failures, those failures ***must***
be returned to the caller. This implies that a function which calls a function
which allocates, it too must also take an `Allocator` and propagate errors it can
not, itself, handle. Because C++ doesn't have multiple return values, functions
that return allocated data ***should*** follow the following convention:

- Passing the destination of the allocation by pointer in the first parameter
- Passing the `Allocator` as the last parameter
- Returning any errors out the return value

Example:
```cpp
draco::memory::Error allocates(
	Foo **dst,
	/* params for allocates */,
	// note, this is the wide pointer allocator and that it is passed by value
	// optional temporary allocator if needed. Allocations to tempAlloc
	// ***must*** not be returned
	draco::memory::Allocator tempAlloc,
	// this is the allocator dst ***must*** be allocated to
	draco::memory::Allocator alloc
);
```

This mimics the assembly convention of having the destination on the left,
mirroring the syntax applied in mathematics where the assigned variable is on
the left and the expression is on the right.

### Data Structures

Data structures, iff they allocate, ***must*** hold a wide pointer handle to an
`Allocator`. This avoids the problem of passing one `Allocator` to one call of a
function which may allocate a portion of the data structure, and then passing a
different `Allocator` to the next call. In the case of dynamic arrays, it is then
unknown which `Allocator` owns the data, and in the case of node-based data
structures, by deinitializing one of the `Allocator`s, the data structure can then
no longer be considered valid as it is then poorly formed.

Keep in mind, this is for data structures that own the allocations. Intrusive
data structures shouldn't be holding allocators unless they themselves own
allocations for other reasons.

```cpp
// good
template <typename T>
struct DynamicArray
{
	T *data;
	usize capacity;
	usize length;
	Allocator alloc;

	Error append(T elem);
};

// bad, see the dynamic array failure case
template <typename T>
struct DynamicArray
{
	T *data;
	usize capacity;
	usize length;

	Error append(T elem, Allocator alloc);
};
```

#### Dynamic Array Failure Case

Consider the following:

```cpp
template <typename T>
struct DynamicArray
{
	T *data;
	usize capacity;
	usize length;

	Error append(T elem, Allocator alloc); // assume this is implemented
	// ...
};

int main()
{
	using namespace draco::memory;
	bump::BumpAllocator bumpAllocA;
	bump::BumpAllocator bumpAllocB;
	Allocator allocA;
	Allocator allocB;
	DynamicArray<i32> arr = {};
	Error err;
	bump::init(&bumpAllocA, page::pageAllocator);
	bump::init(&bumpAllocB, page::pageAllocator);
	bump::asAllocator(&allocA, &bumpAllocA);
	bump::asAllocator(&allocB, &bumpAllocB);
	err = arr.append(69, allocA); // maybe allocate on A
	// error checking
	err = arr.append(420, allocB); // maybe allocate on B
	// error checking
	// ...
}
```

```
line 22:
	allocA: No bytes allocated yet
	allocB: No bytes allocated yet
	arr: Nil struct

line 23:
	allocA: 4 or more bytes allocated
	|---|...|
	|u32|...|
	allocB: No bytes allocated yet
	arr: Definitely points to allocA
line 25:
	allocA:
		case 1: no reallocation
			AllocA provided at least 8 bytes to arr on the first call
			|---|---|...|
			|u32|u32|...|
		case 2: reallocation
			AllocA provided less than 8 bytes to arr on the first call
			|---|...|
			|u32|...|
	allocB:
		case 1: no reallocation
			No bytes allocated yet
		case 2: reallocation
			AllocA provided less than 8 bytes to arr on the first call, allocB
			is now reallocated to
			|---|---|...|
			|u32|u32|...|
	arr:
		case 1: no reallocation, still points to allocA
		case 2: reallocation, now points to allocB
```

On the last line of `main`, which allocator owns the data backing `arr`?

#### Node Failure Case 1

Consider the following:

```cpp
template <typename T>
struct LinkedListNode;

template <typename T>
struct LinkedListNode
{
	T data;
	LinkedListNode<T> *next;
};

template <typename T>
struct LinkedList
{
	LinkedListNode<T> *first;

	Error append(T elem, Allocator alloc); // assume this is implemented
	// ...
};

int main()
{
	using namespace draco::memory;
	bump::BumpAllocator bumpAllocA;
	bump::BumpAllocator bumpAllocB;
	Allocator allocA;
	Allocator allocB;
	LinkedList<u32> list = {};
	Error err;
	bump::init(&bumpAllocA, page::pageAllocator);
	bump::init(&bumpAllocB, page::pageAllocator);
	bump::asAllocator(&allocA, &bumpAllocA);
	bump::asAllocator(&allocB, &bumpAllocB);
	err = list.append(69, allocA); // maybe allocate on A
	// error checking
	err = list.append(420, allocB); // maybe allocate on B
	// error checking
	// ...
	bump::deinit(&bumpAllocB);
	// the first node now points to a node which is unsafe to access
}
```

#### Node Failure Case 2

Consider the following:

```cpp
template <typename T>
struct LinkedListNode;

template <typename T>
struct LinkedListNode
{
	T data;
	LinkedListNode<T> *next;
};

template <typename T>
struct LinkedList
{
	LinkedListNode<T> *first;

	Error append(T elem, Allocator alloc); // assume this is implemented
	// ...
};

int main()
{
	using namespace draco::memory;
	bump::BumpAllocator bumpAllocA;
	bump::BumpAllocator bumpAllocB;
	Allocator allocA;
	Allocator allocB;
	LinkedList<u32> list = {};
	Error err;
	bump::init(&bumpAllocA, page::pageAllocator);
	bump::init(&bumpAllocB, page::pageAllocator);
	bump::asAllocator(&allocA, &bumpAllocA);
	bump::asAllocator(&allocB, &bumpAllocB);
	err = list.append(69, allocA); // maybe allocate on A
	// error checking
	err = list.append(420, allocB); // maybe allocate on B
	// error checking
	// ...
	bump::deinit(&bumpAllocA);
	// the first pointer is now unsafe to access and the second node is now
	// unreachable
}
```

## Implementing `Allocator`s

Allocators need to implement the `AllocatorVTbl`. Feel free to, if needed, to
provide the appropriate `nil` stub function (found in allocator.cpp{,m}). There
***must*** be a global variable of type `AllocatorVTbl` with the implementations,
avoiding vtable duplication.

```cpp
struct AllocatorVTbl
{
	using AllocFn = Error (*)(
		Allocator alloc,
		Slice *dst,
		usize size,
		usize align
#ifdef DEBUG
		, std::source_location loc
#endif
	);
	using FreeFn = Error (*)(Allocator alloc, Slice block);
	using FreeAllFn = Error (*)(Allocator alloc);
	AllocFn alloc;
	FreeFn free;
	FreeAllFn freeAll;
};
```

```cpp
AllocatorVTbl allocatorVtbl = {
	.alloc = alloc,
	.free = free,
	.freeAll = freeAll,
};
```

Afterwards, an `asAllocator` function with the signature
`void (*)(Allocator *dst, TAllocator *alloc)` and an option of the following ***must***
be implemented:

1. Two functions: `init` and `deinit` or
2. A constructor/destructor pair

The constructor/init function can use the type erased `asAllocatorVoid` function
to initialize the type erased allocator if they so choose.

Allocators which don't themselves produce memory (e.g. a `PageAllocator`) ***must***
hold onto another allocator from which they ask for more memory or a buffer from
which they return allocations into (e.g. a `FixedAllocator`).