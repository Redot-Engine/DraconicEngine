module;

#include <vector>

export module core.memory.slot_array;

import core.stdtypes;
import core.memory.handle;

export namespace draco::core::memory {
template<typename T>
struct Slot {
	T value{};
	u32 generation = 0;
	bool alive     = false;
};

template<typename T, typename Tag>
class SlotArray {
	public:
	using Handle = Handle<Tag>;

	Handle create(T const &value) {
		u32 idx;

		if (!free_list.empty()) {
			idx = free_list.back();
			free_list.pop_back();
		}
		else {
			idx = static_cast<u32>(slots.size());
			slots.push_back({});
		}

		Slot<T> &slot = slots[idx];

		slot.value = value;
		slot.alive = true;

		return Handle::make(idx, slot.generation);
	}

	bool valid(Handle h) const {
		u32 i = h.index();

		return i < slots.size() && slots[i].alive && slots[i].generation == h.generation();
	}

	T *get(Handle h) {
		if (!valid(h)) { return nullptr; }

		return &slots[h.index()].value;
	}

	T const *get(Handle h) const {
		if (!valid(h)) { return nullptr; }

		return &slots[h.index()].value;
	}

	void destroy(Handle h) {
		if (!valid(h)) { return; }

		auto &s = slots[h.index()];

		s.alive = false;
		s.generation++; // Invalidate all old handles
		free_list.push_back(h.index());
	}

	std::vector<Slot<T>> const &raw() const {
		return slots;
	}

	private:
	std::vector<Slot<T>> slots;
	std::vector<u32> free_list;
};
} // namespace draco::core::memory
