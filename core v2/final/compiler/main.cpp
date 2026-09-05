#define _GNU_SOURCE
#ifndef _WINDOWS_
	#define WIN32_LEAN_AND_MEAN
	#define NOMINMAX
	#define UNICODE
	#include <windows.h>
#endif
#include <type_traits>
#include <limits>
#include <stdint.h>
#include <setjmp.h>
#include <stdlib.h>
#include <stdarg.h>
#include "cyxlib/syncode.h"
#include "cyxlib/non-crt-math.h"
#include "cyxlib/io.h"
/*
SYNCODE("cyxlib/non-crt-math.cpp")
SYNCODE("cyxlib/io.cpp")
*/
#include "cyxlib/non-crt-math.cpp"
#include "cyxlib/io.cpp"
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "opengl32.lib")

#ifndef _JIT_NO_PACKING
	namespace JIT {
#endif


enum TAG: uint8_t {
	NOT_INITIALIZED = 1, AVAILABLE = 2, ABANDONED = 3,
	AT_REG = 1, AT_STACK = 2, AT_NOWHERE = 3
};

template <typename INT_TYPE>
constexpr size_t max_base10_chars() {
	static_assert(std::is_integral<INT_TYPE>::value, "global/max_base10_chars: not a int");
	float digits_f = (float)std::numeric_limits<INT_TYPE>::digits * NCM::log10(2.f);
	size_t digits = (digits_f > (int64_t)digits_f) ? (size_t(digits_f) + 1) : size_t(digits_f);
	if constexpr (std::is_signed<INT_TYPE>::value) digits++;
	return digits;
}
template <size_t base, typename INT_TYPE>
inline INT_TYPE convert_last_digit(INT_TYPE* int_val) {
	static_assert(std::is_integral<INT_TYPE>::value, "global/convert_last_digit: not a int");
	INT_TYPE mod = *int_val % base;
	*int_val /= base;
	return mod;
}

template <typename INT_TYPE>
uint8_t print_int(wchar_t* dest, INT_TYPE int_val) {
	static_assert(std::is_integral<INT_TYPE>::value, "global/print_int: not a int");
    uint8_t counter = 0;
    if (int_val == 0) dest[counter++] = L'0';
    else {
    	if constexpr (std::is_signed<INT_TYPE>::value) if (int_val < 0) {
        	dest[counter++] = L'-';
			int_val = -int_val;
    	}
		wchar_t reversed[max_base10_chars<INT_TYPE>()];
		uint8_t reversed_counter = 0;
		while (int_val > 0) reversed[reversed_counter++] = L'0' + convert_last_digit<10, INT_TYPE>(&int_val);
		while (reversed_counter > 0) dest[counter++] = reversed[--reversed_counter];
	}
	return counter;
}

size_t synthesize_str(wchar_t* dest, const wchar_t* format, ...) {
	va_list args;
	va_start(args, format);
	size_t counter = 0;
	for (size_t i = 0; format[i] != L'\0'; i++) {
		switch (format[i]) {
			case L'~': {
				wchar_t* source = (wchar_t*)va_arg(args, wchar_t*);
				wcscpy(dest + counter, source);
				counter += wcslen(source);
				break;
			} case L'%': {
				double n = va_arg(args, double);
				if (NCM::isnan(n)) { wcscpy(dest + counter, L"NaN"); counter += 3; break; }
				if (n < 0.) { dest[counter++] = L'-'; n = -n; }
				if (NCM::isinf(n)) { wcscpy(dest + counter, L"Inf"); counter += 3; break; }
				double int_part;
				n = NCM::fsplit(n, &int_part); // n = modf(n, &int_part);
				counter += print_int<uint64_t>(dest, (uint64_t)int_part);
				dest[counter++] = L'.';
				counter += print_int<uint64_t>(dest, uint64_t(n * 100.));
				break;
			} case L'$': {
				switch (format[++i]) {
					case L'L': counter += print_int<int64_t>(dest, (int64_t)va_arg(args, int64_t)); break;
					case L'U': counter += print_int<uint32_t>(dest, (uint32_t)va_arg(args, uint32_t)); break;
					case L'X': counter += print_int<uint64_t>(dest, (uint64_t)va_arg(args, uint64_t)); break;
					default: {
						counter += print_int<int32_t>(dest, (int32_t)va_arg(args, int32_t));
						i--;
					}
				}
				break;
			} case L'`': dest[counter++] = (wchar_t)va_arg(args, int32_t); break;
			case L'@': dest[counter++] = format[++i]; break;
			default: dest[counter++] = format[i];
		}
	}
	va_end(args);
	dest[counter] = L'\0';
	return counter;
}


struct COMPILATION_ENV {
	TAG      state;
	HANDLE   hheap, houtput;
	wchar_t* error_str;
	size_t   error_str_maxlen;
	COMPILATION_ENV(): state(NOT_INITIALIZED) { }
	~COMPILATION_ENV() {
		if (state == AVAILABLE) {
			HeapFree(hheap, 0, error_str);
			state = ABANDONED;
		}
	}
	void report_error(const wchar_t* error_info, int32_t code) {
		synthesize_str(error_str, L"~\nCode: $", code);
		#ifndef _JIT_SILENT
			MessageBoxW(NULL, error_str, L"ERROR", MB_ICONERROR | MB_OK);
        #endif
	}
	void init() {
		error_str = (wchar_t*)HeapAlloc(hheap, 0, (error_str_maxlen + 1) * sizeof(wchar_t));
		state = AVAILABLE;
	}
};


struct MACHINE_CODE {
	BYTE*  executable_memory;
	size_t counter, max_size;
	MACHINE_CODE(size_t _max_size): max_size(_max_size),
		executable_memory((BYTE*)VirtualAlloc(NULL, _max_size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE)) { }
	~MACHINE_CODE() {
		if (executable_memory != nullptr) VirtualFree(executable_memory, 0, MEM_RELEASE);
		executable_memory = nullptr;
	}
	inline BYTE _hex2byte(char c) {
		if ('0' <= c && c <= '9') return c - '0';
		else if ('A' <= c && c <= 'F') return c - 'A' + 10;
		return 0;
	}
	inline void command(const char* command_str, COMPILATION_ENV* env) {
		constexpr uint8_t HIGH = 1, LOW = 2, JUMP = 3;
		uint8_t state = HIGH, current_byte;
		for (size_t i = 0; command_str[i] != '\0'; i++) {
			switch (state) {
				case HIGH: {
					if (command_str[i] == ' ') break;
					current_byte = _hex2byte(command_str[i]) << 4;
					state = LOW; break;
				} case LOW: {
					current_byte |= _hex2byte(command_str[i]);
					state = JUMP; break;
				} case JUMP: {
					if (command_str[i] == ' ') state = HIGH;
					else env->report_error(L"Hexadecimal string format error", -1);
					break;
				}
			}
		}
		if (state != HIGH) env->report_error(L"Hex string ends incorrectly", -2);
	}
	inline void cleanup() { counter = 0; }
};

#ifndef _JIT_NO_PACKING
	} // namespace JIT {
#endif

#include <iostream>
int main() {
	using namespace JIT;
	COMPILATION_ENV env;
	env.error_str_maxlen = 64;
	env.init();
	env.report_error(L"Test", -114514);
	return 0;
}
