#ifndef _WINDOWS_
	#define WIN32_LEAN_AND_MEAN
	#define NOMINMAX
	#define _GNU_SOURCE
	#include <windows.h>
#endif
#include <stdint.h>
#include <stdlib.h>

#define exchange(A, B) auto __swap_temp = A; A = B; B = __swap_temp;

template <typename TYPE_VAL>
struct HASH_ENTRY {
	wchar_t* key;
	TYPE_VAL val;
	HASH_ENTRY<TYPE_VAL>* prev;
};
template <typename TYPE_VAL>
struct HASH_DICT {
	wchar_t              *strs;
	size_t               storage_counter, entry_counter, max_entries;
	HASH_ENTRY<TYPE_VAL> *entries, *last_entry, **enum_targets;
	static constexpr float redundant = 1.5f;
	HASH_DICT(size_t __max_entries, size_t __max_storage): storage_counter(0), entry_counter(0),
			max_entries(size_t(__max_entries * redundant)) {
		strs = (wchar_t*)malloc(__max_storage * sizeof(wchar_t)
			+ max_entries * (sizeof(HASH_ENTRY<TYPE_VAL>) + sizeof(HASH_ENTRY<TYPE_VAL>*)));
		entries = (HASH_ENTRY<TYPE_VAL>*)(strs + __max_storage); last_entry = entries;
		enum_targets = (HASH_ENTRY<TYPE_VAL>**)(entries + __max_entries);
		for (size_t i = 0; i < max_entries; i++) entries[i].key = nullptr;
	}
	~HASH_DICT() {
		if (strs) free(strs);
		strs = nullptr;
	}
	size_t str_hash(const wchar_t* str) {
		size_t h = 0, len = wcslen(str);
		for (size_t i = 0; i < len; i++) h = h * 31 + (uint16_t(str[i]) & 0xFF);
		h %= max_entries;
		return h;
	}
	void add(const wchar_t* key, TYPE_VAL val) {
		size_t target = str_hash(key);
		while (entries[target].key != nullptr) target = (target + 1) % max_entries;
		HASH_ENTRY<TYPE_VAL>* current_entry = entries + target;
		last_entry->prev = current_entry;
		last_entry = current_entry;
		last_entry->prev = nullptr;
		last_entry->key = strs + storage_counter;
		last_entry->val = val;
		enum_targets[entry_counter++] = current_entry;
		for (size_t i = 0; key[i] != L'\0'; i++) strs[storage_counter++] = key[i];
		strs[storage_counter++] = L'\0';
	}
	bool get(const wchar_t* key, TYPE_VAL** result) {
		size_t target = str_hash(key); size_t start = target;
		while (entries[target].key != nullptr) {
			if (wcscmp(entries[target].key, key) == 0) {
				*result = &(entries[target].val);
				return 1;
			}
			target = (target + 1) % max_entries;
			if (target == start) break;
		}
		return 0;
	}
	void cleanup() {
		storage_counter = entry_counter = 0;
		strs[0] = L'\0';
		for (size_t i = 0; i < max_entries; i++) entries[i].key = nullptr;
	}
};
#define enum_dict(DICT, TYPE, KEY, VAL) \
	for (size_t __enum_v = 0; __enum_v < DICT.entry_counter; __enum_v++) { \
		TYPE VAL = DICT.enum_targets[__enum_v]->val; \
		wchar_t* KEY = DICT.enum_targets[__enum_v]->key;
#define end_enum }

constexpr BYTE _0b(const char* bin_str) {
	BYTE n = 0;
	for (BYTE i = 0; bin_str[i] != '\0'; i++) n = (n << 1) + (bin_str[i] - '0');
	return n;
}

struct BIT_FIELD {
	BYTE field = 0;
	inline void set(BYTE bit) { field |= 1 << bit; }
	inline void set4(bool b_0, bool b_1, bool b_2, bool b_3) { // low -> high
		field = (b_3 << 3) | (b_2 << 2) | (b_1 << 1) | b_0;
	}
	inline BYTE get(BYTE bit) { return field & (1 << bit); }
	inline bool get_strict(BYTE bit) { return get(bit) >> bit; }
};
