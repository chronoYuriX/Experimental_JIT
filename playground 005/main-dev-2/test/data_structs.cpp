#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <windows.h>


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
		wprintf(L"Max entries: %d -> %d\n", __max_entries, max_entries);
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

void printk(HASH_DICT<int>* d, const wchar_t* key) {
	int* result;
	if (d->get(key, &result)) wprintf(L"%ls: %d\n", key, *result);
	else wprintf(L"%ls: Not found\n", key);
}
int main() {
	HASH_DICT<int> d(10, 100);
	d.str_hash(L"12345678");
	d.add(L"123", 123);
	d.add(L"456", 456);
	d.add(L"789", 789);
	d.add(L"very very long name", 114514);
	for (int i = 0; i < d.storage_counter; i++) wprintf(L"%lc", (d.strs[i] == L'\0') ? L'~' : d.strs[i]);
	wprintf(L"\n");
	printk(&d, L"123");
	printk(&d, L"456");
	printk(&d, L"789");
	printk(&d, L"very very long name");
	printk(&d, L"undefined");
	enum_dict(d, int, name, arg)
		printf("%ls: %d\n", name, arg);
	end_enum
	return 0;
}
