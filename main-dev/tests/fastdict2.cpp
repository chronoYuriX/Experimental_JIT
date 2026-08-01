#include <string.h>
#include <stdlib.h>
#include <stdio.h>

template <typename TYPE_VAL>
struct DICT {
	wchar_t** strptrs;
	wchar_t*  strs;
	TYPE_VAL* vals;
	size_t    ptr_counter, storage_counter;
	bool      disordered;
	bool (*sortby)(const wchar_t*, const wchar_t*);
	const wchar_t TAG_DEFAULT = L'#', TAG_DELETE = L'!';
	DICT(size_t max_strs, size_t max_storage): ptr_counter(0), storage_counter(0), disordered(0),
			strptrs((wchar_t**)malloc(max_strs * (sizeof(wchar_t*) + sizeof(TYPE_VAL)))),
			strs((wchar_t*)malloc(max_storage * sizeof(wchar_t))) {
		vals = (TYPE_VAL*)(strptrs + max_strs);
	}
	~DICT() {
		if (strptrs) free(strptrs);
		if (strs) free(strs);
		strptrs = nullptr; strs = nullptr; vals = nullptr;
	}
	DICT(const DICT&) = delete;
	DICT& operator=(const DICT&) = delete;
	void add(const wchar_t* key, TYPE_VAL val) {
		vals[ptr_counter] = val;
		strs[storage_counter++] = TAG_DEFAULT;
		strptrs[ptr_counter++] = strs + storage_counter;
		for (size_t i = 0; key[i] != L'\0'; i++) strs[storage_counter++] = key[i];
		strs[storage_counter++] = L'\0';
		disordered = 1;
	}
	static bool __sort_optimize(const wchar_t* str_1, const wchar_t* str_2) { return wcscmp(str_1, str_2) < 0; }
	static bool __sort_roolback(const wchar_t* str_1, const wchar_t* str_2) { return str_1 < str_2; }
	inline void __sort_exchange(ptrdiff_t ptr_1, ptrdiff_t ptr_2) {
		wchar_t* temp_strptr = strptrs[ptr_1]; strptrs[ptr_1] = strptrs[ptr_2]; strptrs[ptr_2] = temp_strptr;
		TYPE_VAL temp_int = vals[ptr_1]; vals[ptr_1] = vals[ptr_2]; vals[ptr_2] = temp_int;
	}
	inline ptrdiff_t __sort_partiton(ptrdiff_t left, ptrdiff_t right) {
    	wchar_t* mark = strptrs[right];
    	ptrdiff_t i = left - 1;
		for (ptrdiff_t j = left; j < right; j++) if (sortby(strptrs[j], mark)) if (++i != j) __sort_exchange(i, j);
    	__sort_exchange(++i, right);
    	return i;
	}
	void __sort(ptrdiff_t left, ptrdiff_t right) {
		if (left >= right) return;
		ptrdiff_t mid = __sort_partiton(left, right);
		__sort(left, mid - 1);
		__sort(mid + 1, right);
	}
	size_t __find_index(const wchar_t* key) {
		if (disordered) {
			sortby = __sort_optimize;
			__sort(0, ptr_counter - 1);
			disordered = 0;
		}
		size_t left = 0, right = ptr_counter - 1, mid = ptr_counter >> 1;
		while (left <= right) {
			if (int cmp = wcscmp(strptrs[mid], key); cmp > 0) right = mid - 1;
			else if (cmp < 0) left = mid + 1;
			else return mid;
			mid = (left + right) >> 1;
		}
		return ~0;
	}
	size_t find_index_ro(const wchar_t* key) {
		if (disordered) {
			for (size_t i = 0; i < ptr_counter; i++) if (wcscmp(strptrs[i], key) == 0)
				return (*(strptrs[i] - 1) == TAG_DELETE) ? ~0 : i;
			return ~0;
		} else return __find_index(key);
	}
	void del(const wchar_t* key) {
		if (size_t index = __find_index(key); index != ~0) *(strptrs[index] - 1) = TAG_DELETE;
	}
	TYPE_VAL get(const wchar_t* key, TYPE_VAL not_found) {
		size_t index = __find_index(key);
		return (index == ~0 || *(strptrs[index] - 1) == TAG_DELETE) ? not_found : vals[index];
	}
	void GC() {
		sortby = __sort_roolback; __sort(0, ptr_counter - 1);
		size_t ptr_counter_copy = ptr_counter, move_distance = 0;
		ptr_counter = storage_counter = 0;
		for (size_t i = 0; i < ptr_counter_copy; i++) {
			if (*(strptrs[i] - 1) == TAG_DELETE) move_distance += wcslen(strptrs[i]) + 2;
			else {
				size_t current_len = wcslen(strptrs[i]) + 2;
				wchar_t* dest = strs + storage_counter, *source = strptrs[i] - 1;
				for (size_t j = 0; j < current_len; j++) dest[j] = source[j];
				storage_counter += current_len;
				strptrs[ptr_counter] = strptrs[i] - move_distance;
				vals[ptr_counter++] = vals[i];
			}
		}
		disordered = 1;
	}
	void cleanup() {
		ptr_counter = storage_counter = 0;
		disordered = 0;
	}
};

int main() {
	DICT<int> dict(10, 100);
	dict.add(L"n", 1);
	dict.add(L"abc", 2);
	dict.add(L"123", 123);
	dict.add(L"xxxx", 6666);
	dict.add(L"very_long_name_0123456789ABCDEF", 114514);
	dict.add(L"LOL", 8888);

	for (int i = 0; i < dict.ptr_counter; i++) wprintf(L"%ls: %d\n", dict.strptrs[i], dict.vals[i]);
	for (int i = 0; i < dict.storage_counter; i++) {
		if (dict.strs[i] == L'\0') wprintf(L"`");
		else wprintf(L"%lc", dict.strs[i]);
	}
	dict.get(L"Nothing", 0);
	wprintf(L"\n\nAfter sorting:\n");
	for (int i = 0; i < dict.ptr_counter; i++) wprintf(L"%ls: %d\n", dict.strptrs[i], dict.vals[i]);
	for (int i = 0; i < dict.storage_counter; i++) {
		if (dict.strs[i] == L'\0') wprintf(L"`");
		else wprintf(L"%lc", dict.strs[i]);
	}
	wprintf(L"\n\n");

	wprintf(L"n: %d\n", dict.get(L"n", -1));
	wprintf(L"abc: %d\n", dict.get(L"abc", -1));
	wprintf(L"123: %d\n", dict.get(L"123", -1));
	wprintf(L"xxxx: %d\n", dict.get(L"xxxx", -1));
	wprintf(L"very_long_name_0123456789ABCDEF: %d\n", dict.get(L"very_long_name_0123456789ABCDEF", -1));
	wprintf(L"(undefined): %d\n", dict.get(L"LOL", -1));

	dict.del(L"xxxx");
	for (int i = 0; i < dict.storage_counter; i++) {
		if (dict.strs[i] == L'\0') wprintf(L"`");
		else wprintf(L"%lc", dict.strs[i]);
	}
	wprintf(L"\nxxxx(deleted): %d\n\n", dict.get(L"xxxx", -1));

	dict.sortby = dict.__sort_roolback;
	dict.__sort(0, dict.ptr_counter - 1);
	for (int i = 0; i < dict.ptr_counter; i++) wprintf(L"%ls: %d\n", dict.strptrs[i], dict.vals[i]);
	for (int i = 0; i < dict.storage_counter; i++) {
		if (dict.strs[i] == L'\0') wprintf(L"`");
		else wprintf(L"%lc", dict.strs[i]);
	} wprintf(L"\n\n");
	dict.GC();
	for (int i = 0; i < dict.ptr_counter; i++) wprintf(L"%ls: %d\n", dict.strptrs[i], dict.vals[i]);
	for (int i = 0; i < dict.storage_counter; i++) {
		if (dict.strs[i] == L'\0') wprintf(L"`");
		else wprintf(L"%lc", dict.strs[i]);
	} wprintf(L"\n\n");
	wprintf(L"n: %d\n", dict.get(L"n", -1));
	wprintf(L"abc: %d\n", dict.get(L"abc", -1));
	wprintf(L"123: %d\n", dict.get(L"123", -1));
	wprintf(L"xxxx: %d\n", dict.get(L"xxxx", -1));
	wprintf(L"very_long_name_0123456789ABCDEF: %d\n", dict.get(L"very_long_name_0123456789ABCDEF", -1));

	return 0;
}
