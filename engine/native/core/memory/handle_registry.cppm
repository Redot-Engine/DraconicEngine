export module core.memory.handle_registry;

import core.memory.slot_array;
import core.memory.handle;

export namespace draco::core::memory {
// Manager layer so other subsystems don't touch raw storage logic

template<typename T, typename Tag>
class HandleRegistry {
	public:
	using HandleType = Handle<Tag>;

	HandleType create(T const &value) {
		return storage.create(value);
	}

	bool valid(HandleType h) const {
		return storage.valid(h);
	}

	T *get(HandleType h) {
		return storage.get(h);
	}

	T const *get(HandleType h) const {
		return storage.get(h);
	}

	void destroy(HandleType h) {
		storage.destroy(h);
	}

	SlotArray<T, Tag> &internal() {
		return storage;
	}

	private:
	SlotArray<T, Tag> storage;
};
} // namespace draco::core::memory
