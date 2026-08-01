/*
def quick_sort(arr, left, right):
	if left < right:
	pivot = partition(arr, left, right)
	quick_sort(arr, left, pivot - 1)
	quick_sort(arr, pivot + 1, right)

def partition(arr, left, right):
	pivot = left
	index = pivot + 1
	for i in range(index, right + 1):
		if arr[i] < arr[pivot]:
			arr[i], arr[index] = arr[index], arr[i]
			index += 1
	arr[pivot], arr[index - 1] = arr[index - 1], arr[pivot]
	return index - 1

# 示例数组
array = [3,4,6,1,2,4,7]
# 执行快速排序
quick_sort(array, 0, len(array) - 1)
*/

#include <stdio.h>

inline void quick_sort_exchange(int* a, int* b) { int temp = *a; *a = *b; *b = temp; }
inline int quick_sort_partiton(int* arr, int* mirror, int left, int right) {
	int mid = left, index = left + 1; right++;
	for (int i = index; i < right; i++) if (arr[i] > arr[mid]) {
		quick_sort_exchange(mirror + i, mirror + index);
		quick_sort_exchange(arr + i, arr + (index++));
	} index--;
	quick_sort_exchange(mirror + mid, mirror + index);
	quick_sort_exchange(arr + mid, arr + index);
	return index;
}
__fastcall void quick_sort(int* arr, int* mirror, int left, int right) {
	if (left >= right) return;
	int mid = quick_sort_partiton(arr, mirror, left, right);
	quick_sort(arr, mirror, left, mid - 1);
	quick_sort(arr, mirror, mid + 1, right);
}

int main() {
	int arr[10] = { 3, 2, 7, 9, 1, 5, 4, 6, 0, 8 }, mirror[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
	quick_sort(arr, mirror, 0, 10 - 1);
	for (int i = 0; i < 10; i++) printf("%d ", arr[i]);
	return 0;
}
