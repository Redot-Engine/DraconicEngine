module;

export module core.memory.slice;
export import core.stdtypes;

export namespace draco::memory {
struct Slice {
	rawptr data;
	usize size;
};
} // namespace draco::memory
