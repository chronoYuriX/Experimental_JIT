#include <stdlib.h>
#include <string.h>

struct ROPE_NODE {
	wchar_t* str;
	union { size_t length; size_t mid_index; };
	ROPE_NODE *left, *right;
	ROPE_NODE(wchar_t* __str): left(NULL), right(NULL) {
		if (__str == NULL) {
			str = NULL;
			length = 0;
		} else {
			str = (wchar_t*)malloc((length + 1) * sizeof(wchar_t));
			wcscpy(str, __str);
		}
	}
	~ROPE_NODE() {
		if (str != NULL) free(str);
		str = NULL;
	}
	void resize(size_t __length) {
		length = __length;
		str = (wchar_t*)realloc(str, (length + 1) * sizeof(wchar_t));
	}
};

struct ROPE {
	ROPE_NODE* root;
	ROPE(wchar_t* __text) {
		if (__text == NULL) root = NULL;
		else root = new ROPE_NODE(__text);
	}
	ROPE_NODE* __tree_grow(ROPE_NODE* left, ROPE_NODE* right) {
		if (left == NULL) return right;
		if (right == NULL) return left;
		ROPE_NODE* new_node = new ROPE_NODE(NULL);
		new_node->left = left; new_node->right = right;
		new_node->mid_index = left->length;
		return new_node;
	}
	void __split_node(size_t index, ROPE_NODE* node, ROPE_NODE** left, ROPE_NODE** right) {
		if (node == NULL) { *left = NULL; *right = NULL; }
		else if (node->str != NULL) { // Split leaf
			if (index >= node->length) { *left = node; *right = NULL; }
			else if (index == 0) { *left = NULL; *right = node; }
			else {
				*right = new ROPE_NODE(node->str + index);
				node->str[index] = L'\0';
				node->resize(index);
				*left = node;
			}
		} else { // Split branch
			if (index < node->mid_index) {
				ROPE_NODE *left_left, *left_right;
	            __split_node(node->left, index, &left_left, &left_right);
	            *left = left_left;
	            *right = __tree_grow(left_right, node->right);
			} else {
				ROPE_NODE *right_left, *right_right;
	            __split_node(node->right, index - node->mid_index, &right_left, &right_right);
	            *left = __tree_grow(node->left, right_left);
	            *right = right_right;
			}
		}
	}
};
