#ifndef _CYX_IO_CPP_INCLUDED
	#define _CYX_IO_CPP_INCLUDED
#ifndef _WINDOWS_
	#define WIN32_LEAN_AND_MEAN
	#define NOMINMAX
	#include <windows.h>
#endif
#include <string.h>
#include "non-crt-math.cpp"

struct DEBUG_OUTPUT {
	HANDLE   hout;
	size_t   buffer_size, counter;
	wchar_t* buffer;
	struct ENDL { }; static constexpr ENDL endl = { };
	DEBUG_OUTPUT(size_t __buffer_size): buffer_size(__buffer_size), counter(0), hout(GetStdHandle(STD_OUTPUT_HANDLE)),
		buffer((wchar_t*)HeapAlloc(GetProcessHeap(), 0, (__buffer_size + 1) * sizeof(wchar_t))) { }
	~DEBUG_OUTPUT() {
		if (buffer != nullptr) HeapFree(GetProcessHeap(), 0, buffer);
		buffer = nullptr;
	}
	void flush() {
		if (counter > 0) {
			buffer[counter] = L'\0';
			WriteConsoleW(hout, buffer, counter, nullptr, nullptr);
			buffer[0] = L'\0';
			counter = 0;
		}
	}
	void __printint(uint64_t n) {
		if (n == 0) buffer[counter++] = L'0';
		else {
			wchar_t reversed[20];
			BYTE i = 0;
			while (n > 0) { reversed[i++] = L'0' + n % 10; n /= 10; }
			while (i > 0) buffer[counter++] = reversed[--i];
		}
	}
	void print(const wchar_t* format, ...) {
		va_list args;
		va_start(args, format);
		for (size_t i = 0; format[i] != L'\0'; i++) {
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
					__printint((uint64_t)int_part);
					buffer[counter++] = L'.';
					__printint(uint64_t(n  * 100.));
					break;
				} case L'$': {
					int32_t n = (int32_t)va_arg(args, int);
					if (n < 0) { buffer[counter++] = L'-'; n = -n; }
					__printint(n);
					break;
				} case L'`': buffer[counter++] = (wchar_t)va_arg(args, int); break;
				case L'@': buffer[counter++] = format[++i]; break;
				default: buffer[counter++] = wchar_t(format[i]);
			}
		}
		va_end(args);
		flush();
	}
	void divide(size_t len = 20) {
		size_t i = 0;
		while (i < len) buffer[i++] = L'#';
		buffer[i] = L'\0';
		flush();
	}
	DEBUG_OUTPUT& operator<<(const wchar_t* string) {
		size_t len = wcslen(string);
		if (counter + len > buffer_size) flush();
		for (size_t i = 0; i < len; i++) buffer[counter++] = string[i];
		return *this;
	}
	DEBUG_OUTPUT& operator<<(wchar_t single_char) {
		if (counter >= buffer_size) flush();
		buffer[counter++] = single_char;
		return *this;
	}
	DEBUG_OUTPUT& operator<<(uint64_t n) {
		if (counter + 20 > buffer_size) flush();
		__printint(n);
		return *this;
	}
	DEBUG_OUTPUT& operator<<(int n) {
		if (n < 0) { *this << L'-'; n = -n; }
		*this << (uint64_t)n;
		return *this;
	}
	DEBUG_OUTPUT& operator<<(double n) {
		if (NCM::isnan(n)) { *this << L"NaN"; return *this; }
		if (n < 0.) { *this << L'-'; n = -n; }
		if (NCM::isinf(n)) { *this << L"Inf"; return *this; }
		double int_part;
		n = NCM::fsplit(n, &int_part);
		*this << (uint64_t)int_part;
		*this << L'.';
		*this << (uint64_t)(n * 100.);
		return *this;
	}
	DEBUG_OUTPUT& operator<<(ENDL placeholder) {
		*this << L'\n';
		flush();
		return *this;
	}
};


#endif // #ifndef _CYX_IO_CPP_INCLUDED
