
struct GL_STRING_ENTRY {
	wchar_t* key;
	GLuint textureID;
	GL_STRING_ENTRY* prev;
	GL_STRING_ENTRY(const wchar_t* text) {
		GLuint textureID;
    	glGenTextures(1, &textureID);
    	glBindTexture(GL_TEXTURE_2D, textureID);
    	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, bmp_size_x, bmp_size_y, 0, GL_RGB, GL_UNSIGNED_BYTE, BMPdata_RGB);
	}
};

struct GL_STRINGS {
	wchar_t         *strs;
	size_t           storage_counter, entry_counter, max_entries;
	GL_STRING_ENTRY *entries, *last_entry, **enum_targets;
	static constexpr size_t DEFAULT_ENTRIES = 64, DEFAULT_STORAGE
	static constexpr float REDUNDANT = 1.5f;
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

