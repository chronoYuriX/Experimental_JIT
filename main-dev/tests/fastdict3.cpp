#include <string.h>
#include <stdlib.h>
#include <stdio.h>

template <typename TYPE_VAL>
struct DICT {
	wchar_t  **strptrs, **strptrs_original, *strs;
	TYPE_VAL **valptrs, *vals;
	size_t   ptr_counter, storage_counter;
	bool     disordered;
	const wchar_t TAG_DEFAULT = L'#', TAG_DELETE = L'!';
	DICT(size_t max_strs, size_t max_storage): ptr_counter(0), storage_counter(0), disordered(0),
			strptrs((wchar_t**)malloc(max_strs * (sizeof(wchar_t*) * 2 + sizeof(TYPE_VAL*) + sizeof(TYPE_VAL)))),
			strs((wchar_t*)malloc(max_storage * sizeof(wchar_t))) {
		strptrs_original = strptrs + max_strs;
		valptrs = (TYPE_VAL**)(strptrs_original + max_strs);
		vals = (TYPE_VAL*)(valptrs + max_strs);
	}
	~DICT() {
		if (strptrs) free(strptrs);
		if (strs) free(strs);
		strptrs = nullptr; strptrs_original = nullptr; strs = nullptr; vals = nullptr;
	}
	DICT(const DICT&) = delete;
	DICT& operator=(const DICT&) = delete;
	void add(const wchar_t* key, TYPE_VAL val) {
		vals[ptr_counter] = val; valptrs[ptr_counter] = vals + ptr_counter;
		strs[storage_counter++] = TAG_DEFAULT;
		strptrs_original[ptr_counter] = strs + storage_counter; strptrs[ptr_counter++] = strs + storage_counter;
		for (size_t i = 0; key[i] != L'\0'; i++) strs[storage_counter++] = key[i];
		strs[storage_counter++] = L'\0';
		disordered = 1;
	}
	inline void __sort_exchange(ptrdiff_t ptr_1, ptrdiff_t ptr_2) {
		wchar_t*  temp_strptr = strptrs[ptr_1]; strptrs[ptr_1] = strptrs[ptr_2]; strptrs[ptr_2] = temp_strptr;
		TYPE_VAL* temp_valptr = valptrs[ptr_1]; valptrs[ptr_1] = valptrs[ptr_2]; valptrs[ptr_2] = temp_valptr;
	}
	inline ptrdiff_t __sort_partiton(ptrdiff_t left, ptrdiff_t right) {
    	wchar_t* mark = strptrs[right];
    	ptrdiff_t i = left - 1;
		for (ptrdiff_t j = left; j < right; j++) if (wcscmp(strptrs[j], mark) < 0) if (++i != j) __sort_exchange(i, j);
    	__sort_exchange(++i, right);
    	return i;
	}
	void __sort(ptrdiff_t left, ptrdiff_t right) {
		if (left >= right) return;
		ptrdiff_t mid = __sort_partiton(left, right);
		__sort(left, mid - 1);
		__sort(mid + 1, right);
	}
	void optimize() { __sort(0, ptr_counter - 1); disordered = 0; }
	size_t __find_index(const wchar_t* key) {
		size_t left = 0, right = ptr_counter - 1, mid = ptr_counter >> 1;
		while (left <= right) {
			if (int cmp = wcscmp(strptrs[mid], key); cmp > 0) right = mid - 1;
			else if (cmp < 0) left = mid + 1;
			else return mid;
			mid = (left + right) >> 1;
		}
		return ~0;
	}
	size_t __find_index_linear(const wchar_t* key) {
		for (size_t i = 0; i < ptr_counter; i++) if (wcscmp(strptrs_original[i], key) == 0) return i;
		return ~0;
	}
	void del(const wchar_t* key) {
		if (size_t index = __find_index(key); index != ~0) *(strptrs[index] - 1) = TAG_DELETE;
	}
	TYPE_VAL get(const wchar_t* key, TYPE_VAL not_found) {
		if (disordered) {
			size_t index = __find_index_linear(key);
			return (index == ~0 || *(strptrs_original[index] - 1) == TAG_DELETE) ? not_found : vals[index];
		}
		size_t index = __find_index(key);
		return (index == ~0 || *(strptrs[index] - 1) == TAG_DELETE) ? not_found : *(valptrs[index]);
	}
	TYPE_VAL* get_ptr(const wchar_t* key) {
		if (disordered) {
			size_t index = __find_index_linear(key);
			return (index == ~0 || *(strptrs_original[index] - 1) == TAG_DELETE) ? nullptr : (vals + index);
		}
		size_t index = __find_index(key);
		return (index == ~0 || *(strptrs[index] - 1) == TAG_DELETE) ? nullptr : valptrs[index];
	}
	void GC() {
		size_t ptr_counter_copy = ptr_counter, move_distance = 0;
		ptr_counter = storage_counter = 0;
		for (size_t i = 0; i < ptr_counter_copy; i++) {
			if (*(strptrs_original[i] - 1) == TAG_DELETE) move_distance += wcslen(strptrs_original[i]) + 2;
			else {
				size_t current_len = wcslen(strptrs_original[i]) + 2;
				wchar_t* dest = strs + storage_counter, *source = strptrs_original[i] - 1;
				for (size_t j = 0; j < current_len; j++) dest[j] = source[j];
				storage_counter += current_len;
				vals[ptr_counter] = vals[i]; valptrs[ptr_counter] = vals + ptr_counter;
				strptrs_original[ptr_counter++] = strptrs_original[i] - move_distance;
			}
		}
		memcpy(strptrs, strptrs_original, ptr_counter * sizeof(wchar_t*));
		disordered = 1;
	}
	void cleanup() {
		ptr_counter = storage_counter = 0;
		disordered = 0;
	}
};

template <typename TYPE_VAL>
struct DICTEX {
	DICT<TYPE_VAL> dict;
	size_t max_strs, max_storage;
	DICTEX(size_t max_strs, size_t max_storage): max_strs(max_strs), max_storage(max_storage),
		dict(max_strs, max_storage) { }
	void add(const wchar_t* key, TYPE_VAL val) {
		if (wcslen(key) + dict.storage_counter + 2 > max_storage || dict.ptr_counter >= max_strs) return;
		dict.add(key, val);
	}
	TYPE_VAL get(const wchar_t* key, TYPE_VAL not_found) {
		if (dict.ptr_counter == 0) return not_found;
		return dict.get(key, not_found);
	}
	inline void del(const wchar_t* key) { dict.del(key); }
	inline void optimize() { dict.optimize(); }
	inline void GC() { dict.GC(); }
	inline void cleanup() { dict.cleanup(); }
};


int main() {
	DICTEX<int> dict(10, 100);
	dict.add(L"n", 1);
	dict.add(L"abc", 2);
	dict.add(L"123", 123);
	dict.add(L"xxxx", 6666);
	dict.add(L"very_long_name_0123456789ABCDEF", 114514);
	dict.add(L"LOL", 8888);

	for (int i = 0; i < dict.dict.ptr_counter; i++) wprintf(L"%ls: %d\n", dict.dict.strptrs[i], dict.dict.vals[i]);
	for (int i = 0; i < dict.dict.storage_counter; i++) {
		if (dict.dict.strs[i] == L'\0') wprintf(L"`");
		else wprintf(L"%lc", dict.dict.strs[i]);
	}
	dict.optimize();
	wprintf(L"\n\nAfter sorting:\n");
	for (int i = 0; i < dict.dict.ptr_counter; i++) wprintf(L"%ls: %d\n", dict.dict.strptrs[i], *(dict.dict.valptrs[i]));
	for (int i = 0; i < dict.dict.storage_counter; i++) {
		if (dict.dict.strs[i] == L'\0') wprintf(L"`");
		else wprintf(L"%lc", dict.dict.strs[i]);
	}
	
	wprintf(L"\n\nget:\n");
	wprintf(L"n: %d\n", dict.get(L"n", -1));
	wprintf(L"abc: %d\n", dict.get(L"abc", -1));
	wprintf(L"123: %d\n", dict.get(L"123", -1));
	wprintf(L"xxxx: %d\n", dict.get(L"xxxx", -1));
	wprintf(L"very_long_name_0123456789ABCDEF: %d\n", dict.get(L"very_long_name_0123456789ABCDEF", -1));
	wprintf(L"(undefined): %d\n", dict.get(L"undefined", -1));

	dict.del(L"xxxx");
	for (int i = 0; i < dict.dict.storage_counter; i++) {
		if (dict.dict.strs[i] == L'\0') wprintf(L"`");
		else wprintf(L"%lc", dict.dict.strs[i]);
	}
	wprintf(L"\nxxxx(deleted): %d\n\n", dict.get(L"xxxx", -1));

	for (int i = 0; i < dict.dict.ptr_counter; i++) wprintf(L"%ls: %d\n", dict.dict.strptrs[i], dict.dict.vals[i]);
	for (int i = 0; i < dict.dict.storage_counter; i++) {
		if (dict.dict.strs[i] == L'\0') wprintf(L"`");
		else wprintf(L"%lc", dict.dict.strs[i]);
	} wprintf(L"\n\n");
	dict.GC();
	for (int i = 0; i < dict.dict.ptr_counter; i++) wprintf(L"%ls: %d\n", dict.dict.strptrs[i], dict.dict.vals[i]);
	for (int i = 0; i < dict.dict.storage_counter; i++) {
		if (dict.dict.strs[i] == L'\0') wprintf(L"`");
		else wprintf(L"%lc", dict.dict.strs[i]);
	} wprintf(L"\n\n");
	wprintf(L"n: %d\n", dict.get(L"n", -1));
	wprintf(L"abc: %d\n", dict.get(L"abc", -1));
	wprintf(L"123: %d\n", dict.get(L"123", -1));
	wprintf(L"xxxx: %d\n", dict.get(L"xxxx", -1));
	wprintf(L"very_long_name_0123456789ABCDEF: %d\n", dict.get(L"very_long_name_0123456789ABCDEF", -1));

	return 0;
}
