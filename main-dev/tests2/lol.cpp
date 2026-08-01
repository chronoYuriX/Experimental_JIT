#include <stdio.h>

int main() {
	const float f = 1.5f;
	unsigned long long i = 8;
	wprintf(L"8 * 1.5 = %u", (unsigned long long)(i * f));
	return 0;
}
