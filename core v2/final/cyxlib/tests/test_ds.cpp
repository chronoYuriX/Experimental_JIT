#ifndef _WINDOWS_
	#define WIN32_LEAN_AND_MEAN
	#define NOMINMAX
	#define UNICODE
	#include <windows.h>
#endif
#include <stdint.h>


template <typename TYPE_KEY, typename TYPE_VAL>
struct DICT_ENTRY {
	TYPE_KEY key;
	TYPE_VAL val;
	uint8_t  state;
	DICT_ENTRY<TYPE_KEY, TYPE_VAL>* previous;
	static constexpr uint8_t FREE = 1, OCCUPIED = 2, DELETED = 3;
};

template <typename TYPE_KEY, typename TYPE_VAL>
using DICT_ENUM_FUNC = bool (*)(DICT_ENTRY<TYPE_KEY, TYPE_VAL>*, void*);

template <typename TYPE_KEY, typename TYPE_VAL>
struct DICT {
	#define ENTRY DICT_ENTRY<TYPE_KEY, TYPE_VAL>
	size_t    storage_counter,  max_storage;
	ENTRY    *entries,         *last_entry;
	float     expansion,        load_factor;
	HANDLE    hheap;
	static constexpr float  DEFAULT_EXPANSION = 1.5f, DEFAULT_LOAD_FACTOR = .5f;
	static constexpr size_t DEFAULT_STORAGE = 64, NOT_FOUND = ~0;
	static constexpr bool   ENUM_CONTINUE = 0, ENUM_FINISH = 1;
	DICT(size_t _max_storage = 0, float _expansion = 0.f, float _load_factor = 0.f, HANDLE _hheap = NULL):
			max_storage((_max_storage == 0) ? DEFAULT_STORAGE : _max_storage),
			expansion  ((_expansion <= 1.f) ? DEFAULT_EXPANSION : _expansion),
			load_factor((_load_factor > 1.f || _load_factor <= 0.f) ? DEFAULT_LOAD_FACTOR : _load_factor),
			hheap      ((_hheap == nullptr) ? GetProcessHeap() : _hheap) {
		entries = (ENTRY*)HeapAlloc(hheap, 0, max_storage * sizeof(ENTRY));
		cleanup();
	}
	~DICT() {
		if (entries != nullptr) HeapFree(hheap, 0, entries);
		entries = nullptr;
	}
	uint64_t _hash(const TYPE_KEY key) {
		uint64_t result = 14695981039346656037ULL;
		const uint8_t* key_bytes = (uint8_t*)&key;
		for (size_t i = 0; i < sizeof(key); i++) result = (result ^ key_bytes[i]) * 1099511628211ULL;
		return result % max_storage;
	}
	void set(const TYPE_KEY key, const TYPE_VAL val) {
		TYPE_VAL* target = get(key);
		if (target == nullptr) {
			if (storage_counter >= max_storage * load_factor) rebuild();
			uint64_t dest = _hash(key);
			while (entries[dest].state == ENTRY::OCCUPIED) dest = (dest + 1) % max_storage;
			ENTRY* current_entry = entries + dest;
			current_entry->previous = last_entry;
			current_entry->key = key;
			current_entry->val = val;
			current_entry->state = ENTRY::OCCUPIED;
			last_entry = current_entry;
			storage_counter++;
		} else *target = val;
	}
	ENTRY* get_entry(const TYPE_KEY key) {
		uint64_t dest = _hash(key); uint64_t start = dest;
		while (entries[dest].state != ENTRY::FREE) {
			if (entries[dest].key == key) {
				if (entries[dest].state == DICT_ENTRY<TYPE_KEY, TYPE_VAL>::OCCUPIED) return entries + dest;
				break;
			}
			dest = (dest + 1) % max_storage;
			if (dest == start) break;
		}
		return nullptr;
	}
	inline TYPE_VAL* get(const TYPE_KEY key) {
		ENTRY* target = get_entry(key);
		return (target == nullptr) ? nullptr : &(target->val);
	}
	void remove(const TYPE_KEY key) {
		ENTRY* target = get_entry(key);
		if (target != nullptr) target->state = ENTRY::DELETED;
	}
	void cleanup() {
		last_entry = nullptr;
		storage_counter = 0;
		for (size_t i = 0; i < max_storage; i++) entries[i].state = ENTRY::FREE;
	}
	void rebuild() {
		size_t max_storage_copy = max_storage;
		while (storage_counter >= max_storage * load_factor) {
			size_t max_storage_new = max_storage * expansion;
			if (max_storage_new == max_storage) max_storage_new++;
			max_storage = max_storage_new;
		}
		ENTRY *entries_new = (ENTRY*)HeapAlloc(hheap, 0, max_storage * sizeof(ENTRY)), *entries_copy = entries;
		entries = entries_new;
		cleanup();
		for (size_t i = 0; i < max_storage_copy; i++)
			if (entries_copy[i].state == ENTRY::OCCUPIED) set(entries_copy[i].key, entries_copy[i].val);
		HeapFree(hheap, 0, entries_copy);
	}
	void enum_entries(DICT_ENUM_FUNC<TYPE_KEY, TYPE_VAL> enum_func, void* param) {
		ENTRY* current_entry = last_entry;
		while (current_entry != nullptr) {
			if (current_entry->state == ENTRY::OCCUPIED)
				if (enum_func(current_entry, param) == ENUM_FINISH) return;
			current_entry = current_entry->previous;
		}
	}
	#undef ENTRY
};

// #include <iostream>
#include "io.cpp"
bool enumer(DICT_ENTRY<int, float>* current_entry, void* param) {
	*(DEBUG_OUTPUT*)param << current_entry->key << L": " << current_entry->val << DEBUG_OUTPUT::endl;
	return DICT<int, float>::ENUM_CONTINUE;
}
int main() { // testout
	DEBUG_OUTPUT o(1024);
	DICT<int, float> dict(4, ~0);
	o << L"set:\n"; // std::cout << "set:\n";
	for (int i = 0; i < 20; i++) {
		int k = rand() % 49 + 1;
		float v = float(rand() % 49 + 1);
		dict.set(k, v);
		o << k << L": " << v << o.endl; // std::cout << k << ": " << v << std::endl;
	}
	o << L"get:\n"; // std::cout << "get:\n";
	for (int i = 0; i < 10; i++) {
		float* pv = dict.get(i);
		float v = (pv == nullptr) ? 0.f : *pv;
		o << i << L": " << v << o.endl; // std::cout << i << ": " << v << std::endl;
	}
	o << L"enum:\n"; // std::cout << "enum:\n";
	/*
	DICT_ENTRY<int, float>* current_entry = dict.last_entry;
	while (current_entry != nullptr) {
		o << current_entry->key << L": " << current_entry->val << o.endl; // std::cout << current_entry->key << ": " << current_entry->val << std::endl;
		current_entry = current_entry->previous;
	}*/
	dict.enum_entries(enumer, &o);
	dict.remove(33);
	o << L"remove:\n";
	dict.enum_entries(enumer, &o);
	dict.rebuild();
	o << L"rebuild:\n";
	dict.enum_entries(enumer, &o);
	return 0;
}

