#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <string.h>
#include "cyxlib/non-crt-math.h"

struct DEBUG_OUTPUT {
	HANDLE   hout;
	size_t   buffer_size;
	wchar_t* buffer;
	DEBUG_OUTPUT(size_t __buffer_size): buffer_size(__buffer_size), hout(GetStdHandle(STD_OUTPUT_HANDLE)),
		buffer((wchar_t*)HeapAlloc(GetProcessHeap(), 0, (__buffer_size + 1) * sizeof(wchar_t))) { }
	~DEBUG_OUTPUT() {
		if (buffer != NULL) HeapFree(GetProcessHeap(), 0, buffer);
		buffer = NULL;
	}
	void __printint(uint64_t n, size_t* counter) {
		if (n == 0) buffer[(*counter)++] = L'0';
		else {
			wchar_t reversed[20];
			BYTE i = 0;
			while (n > 0) { reversed[i++] = L'0' + n % 10; n /= 10; }
			while (i > 0) buffer[(*counter)++] = reversed[--i];
		}
	}
	void print(const wchar_t* format, ...) {
		va_list args;
		va_start(args, format);
		size_t len = wcslen(format), counter = 0;
		for (size_t i = 0; i < len; i++) {
			switch (format[i]) {
				case L'~': {
					wchar_t* source = (wchar_t*)va_arg(args, wchar_t*);
					wcscpy(buffer + counter, source);
					counter += wcslen(source);
					break;
				} case L'%': {
					double n = va_arg(args, double);
					if (NCM::isnan(n)) { wcscpy(buffer + counter, L"NaN"); counter += 3; break; }
					if (n < 0.) { buffer[counter++] = L'-'; n = -n; }
					if (NCM::isinf(n)) { wcscpy(buffer + counter, L"Inf"); counter += 3; break; }
					double int_part;
					n = NCM::fsplit(n, &int_part); // n = modf(n, &int_part);
					__printint((uint64_t)int_part, &counter);
					buffer[counter++] = L'.';
					__printint(uint64_t(n  * 100.), &counter);
					break;
				} case L'$': {
					int32_t n = (int32_t)va_arg(args, int);
					if (n < 0) { buffer[counter++] = L'-'; n = -n; }
					__printint(n, &counter);
					break;
				} case L'`': { buffer[counter++] = (wchar_t)va_arg(args, int); break; }
				default: buffer[counter++] = wchar_t(format[i]);
			}
		}
		va_end(args);
		buffer[counter] = L'\0';
		WriteConsoleW(hout, buffer, counter, NULL, NULL);
	}
	void divide(size_t len = 20) {
		size_t i = 0;
		while (i < len) buffer[i++] = L'#';
		buffer[i] = L'\0';
		WriteConsoleW(hout, buffer, i, NULL, NULL);
	}
};

int main() {
	DEBUG_OUTPUT d(1024);
	d.print(L"int: $, float: %, string: ~, wchar: `\n", 123, NCM::NAN, L"LOL", L'W');
	d.divide();
	return 0;
}
