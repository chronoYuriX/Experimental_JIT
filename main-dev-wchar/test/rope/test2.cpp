#include <stdlib.h>
#include <string.h>
#include <stdio.h>


struct ROPE_NODE {
	wchar_t* str;
	size_t max_size;
	union {
		size_t size;
		size_t mid_index;
	};
 ROPE_NODE *left, *right;
	static constexpr float ROPE_STR_EXPANSION = 1.5f;
	inline size_t __get_expanded_size(size_t minimum_size, bool* expanded) {
		size_t new_size = max_size;
		if (new_size < minimum_size) {
			if (expanded != NULL) *expanded = 1;
			do {
				size_t expanded_new_size = size_t(new_size * ROPE_STR_EXPANSION);
				new_size = (expanded_new_size == new_size) ? (expanded_new_size + 1) : expanded_new_size;
			} while (new_size < minimum_size);
		} else if (expanded != NULL) *expanded = 0;
		return new_size;
	}
	ROPE_NODE(const wchar_t* __str) {
		if (__str == NULL) {
			size = 0;
			max_size = 0;
			str = NULL;
		} else {
			size = wcslen(__str);
			max_size = __get_expanded_size(size, NULL);
			str = (wchar_t*)malloc(max_size * sizeof(wchar_t));
			wcscpy(str, __str);
		}
	}
	void set_branch(size_t __mid_index) {
		if (str != NULL) free(str);
		str = NULL;
		mid_index = __mid_index;
	}
	~ROPE_NODE() {
		if (str != NULL) free(str);
		str = NULL;
	}
};

struct ROPE {
	ROPE_NODE* root;
	ROPE_NODE** nodes;
	size_t node_counter, max_nodes;
	static constexpr float ROPE_EXPANSION = 1.5f;
	static constexpr size_t ROPE_DEFAULT_NODES = 16;
	ROPE(const wchar_t* __text): node_counter(0), max_nodes(ROPE_DEFAULT_NODES) {
		nodes = (ROPE_NODE**)malloc(ROPE_DEFAULT_NODES * sizeof(ROPE_NODE*));
		if (__text == NULL) root = NULL;
		else {
			root = new ROPE_NODE(__text);
			__add_node(root);
		}
	}
	~ROPE() {
		root = NULL; // Root will be deleted in the loop
		if (nodes != NULL) {
			for (size_t i = 0; i < node_counter; i++) if (nodes[i] != NULL) delete nodes[i];
			free(nodes);
			wprintf(L"Done!\n");
			nodes = NULL;
		}
	}
	void __add_node(ROPE_NODE* node) {
		if (node_counter == max_nodes) {
			size_t new_max_nodes = size_t(max_nodes * ROPE_EXPANSION);
			if (new_max_nodes == max_nodes) max_nodes++;
			else max_nodes = new_max_nodes;
			nodes = (ROPE_NODE**)realloc(nodes, max_nodes * sizeof(node));
		}
		nodes[node_counter++] = node;
	}
	void __split(ROPE_NODE* node, size_t index, ROPE_NODE** left, ROPE_NODE** right) {
		if (node == NULL) { *left = NULL; *right = NULL; }
		else if (node->str != NULL) { // Split leaf
			if (index >= node->size) { *left = node; *right = NULL; }
			else if (index == 0) { *left = NULL; *right = node; }
			else {
				*right = new ROPE_NODE(node->str + index);
	            __add_node(*right);
	            node->str[index] = L'\0';
        	    node->size = index;
	            *left = node;
			}
		} else { // Split branch
			if (index < node->mid_index) {
	            ROPE_NODE *left_left, *left_right;
	            __split(node->left, index, &left_left, &left_right);
	            *left = left_left;
	            *right = __concat(left_right, node->right);
	        } else {
	            ROPE_NODE *right_left, *right_right;
	            __split(node->right, index - node->mid_index, &right_left, &right_right);
	            *left = __concat(node->left, right_left);
	            *right = right_right;
	        }
    	    node->mid_index = node->left.;
		}
	}
};
