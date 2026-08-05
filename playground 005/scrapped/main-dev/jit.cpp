#ifndef _WINDOWS_
	#define WIN32_LEAN_AND_MEAN
	#define NOMINMAX
	#define _GNU_SOURCE
	#include <windows.h>
#endif
#include <stdint.h>
#include <setjmp.h>
#include <stdlib.h>
#include "non-crt-math.h"

#define __JIT_DEBUG

// ### func generation 0 ###############################################################################################
jmp_buf __g_compiler_jmp;
HANDLE __g_hheap = GetProcessHeap();

wchar_t __g_error_str[64];
void __report_error(const wchar_t* str) {
	wcscpy(__g_error_str, str);
	#ifdef __JIT_DEBUG
		MessageBoxW(NULL, str, L"ERROR", MB_ICONERROR | MB_OK);
    #endif
    longjmp(__g_compiler_jmp, 1);
}

struct JIT_LOG {
	DWORD maxsize, counter;
	wchar_t* logdata;
	bool abandoned;
	HANDLE hout;
	JIT_LOG(DWORD __maxsize): maxsize(__maxsize + 1), counter(0), abandoned(0), hout(GetStdHandle(STD_OUTPUT_HANDLE)),
		logdata((wchar_t*)HeapAlloc(__g_hheap, 0, maxsize)) { }
	~JIT_LOG() {
		if (logdata) HeapFree(__g_hheap, 0, logdata);
		logdata = NULL;
	}
	void __printint(uint64_t n) {
		if (n == 0) { logdata[counter++] = L'0'; return; }
		wchar_t reversed[20];
		BYTE i = 0;
		while (n > 0) { reversed[i++] = L'0' + n % 10; n /= 10; }
		while (i > 0) logdata[counter++] = reversed[--i];
	}
	void log(const wchar_t* format, ...) {
		if (abandoned) return;
		va_list args;
		va_start(args, format);
		DWORD len = wcslen(format);
		for (DWORD i = 0; i < len; i++) {
			switch (format[i]) {
				case L'~': {
					char* source = (char*)va_arg(args, char*);
					DWORD source_len = strlen(source);
					counter += mbstowcs(logdata + counter, source, source_len);
					break;
				} case L'%': {
					double n = va_arg(args, double);
					if (NCM::isnan(n)) { wcscpy(logdata + counter, L"NaN"); counter += 3; break; }
					if (n < 0.) { logdata[counter++] = L'-'; n = -n; }
					if (NCM::isinf(n)) { wcscpy(logdata + counter, L"Inf"); counter += 3; break; }
					double int_part;
					n = NCM::split_double(n, &int_part); // n = modf(n, &int_part);
					__printint((uint64_t)int_part);
					logdata[counter++] = L'.';
					__printint(uint64_t(n  * 100.));
					break;
				} case L'$': __printint((uint64_t)va_arg(args, int)); break;
				case L'`': { logdata[counter++] = (wchar_t)va_arg(args, int); break; }
				default: { logdata[counter++] = wchar_t(format[i]); break; }
			}
		}
		va_end(args);
		logdata[counter] = L'\0';
	}
	void divide() { log(L"################################################################\n"); }
	void show() { WriteConsoleW(hout, logdata, counter, NULL, NULL); }
	void clear() { counter = 0; logdata[0] = L'\0'; }
};
#ifdef __JIT_DEBUG
	JIT_LOG __g_log(16384);
	#define __log(format, ...) __g_log.log(format, ##__VA_ARGS__)
    #define __showlog __g_log.show()
    #define __clearlog __g_log.clear()
    #define __divlog __g_log.divide()
#else
    #define __log(format, ...)
	#define __showlog
	#define __clearlog
	#define __divlog
#endif
#define __showlog_once __divlog; __showlog; __clearlog;

// ### func generation 1 ###############################################################################################
struct EXEMEM_STRUCT {
	BYTE* EXEmem;
	DWORD counter;
	EXEMEM_STRUCT(DWORD EXEsize): counter(0),
		EXEmem((BYTE*)VirtualAlloc(NULL, EXEsize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE)) { }
		// "VirtualFree" is at ~lsJITvar
	inline void command(BYTE total, ...) {
		va_list commands;
		va_start(commands, total);
		for (; total > 0; total--) EXEmem[counter++] = BYTE(va_arg(commands, int)) & 0xFF;
		va_end(commands);
	}
	inline BYTE __HEXchar2halfBYTE(char c) {
		if ('0' <= c && c <= '9') return c - '0';
		else if ('A' <= c && c <= 'F') return c - 'A' + 10;
		return 0;
	}
	inline void command(const char* command_str, DWORD len = ~0) {
		if (len == ~0) len = strlen(command_str) + 1;
		const BYTE HIGH = 1, LOW = 2, BIN = 3, JUMP = 4;
		BYTE state = HIGH, cur_x, cur_b;
		for (DWORD i = 0; i < len; i++) {
			switch (state) {
				case HIGH: {
					cur_x = __HEXchar2halfBYTE(command_str[i]) << 4;
					cur_b = (command_str[i] == '1') ? 4 : 0;
					state = LOW; break;
				} case LOW: {
					cur_x |= __HEXchar2halfBYTE(command_str[i]);
					cur_b |= (command_str[i] == '1') ? 2 : 0;
					state = JUMP; break;
				} case JUMP: {
					if (command_str[i] == ' ' || command_str[i] == '\0') {
						EXEmem[counter++] = cur_x;
						state = HIGH; break;
					}
					cur_b |= (command_str[i] == '1') ? 1 : 0;
					state = BIN; break;
				} case BIN: {
					if (command_str[i] == ' ' || command_str[i] == '\0') {
						EXEmem[counter++] = cur_b;
						state = HIGH; break;
					}
					cur_b = (cur_b << 1) | ((command_str[i] == '1') ? 1 : 0);
					break;
				}
			}
		}
		if (state != HIGH) __report_error(L"Invalid hexadecimal command string");
	}
	inline void data(int intval) {
		BYTE* bytes = (BYTE*)&intval;
		memcpy(EXEmem + counter, bytes, sizeof(int));
		counter += sizeof(int);
	}
	inline void data64(long long longval) {
		BYTE* bytes = (BYTE*)&longval;
		memcpy(EXEmem + counter, bytes, sizeof(long long));
		counter += sizeof(long long);
	}
};

// ### func generation 2 ###############################################################################################
namespace JIT {
	const BYTE
		ALLOC =   1, RECOVER =   2, // stack operation
		SAVE  =   3, LOAD    =   4, // save/load register to/from stack
		ADD   =   5, SUB     =   6, MUL = 7, DIV = 8, SQRT = 9, // +-*/ and sqrt
		COPY  =  10; // copy one register to another
}
void stackOP(EXEMEM_STRUCT& stEXEmem, BYTE operation, DWORD size) {
	using namespace JIT;
	size = (size + 15) & ~15; // Compensate the return address, then align to 16 bytes
	if (operation == ALLOC)        stEXEmem.command("48 81 EC"); // sub rsp, stacksize -> alloca(stacksize)
	else if (operation == RECOVER) stEXEmem.command("48 81 C4"); // add rsp, stacksize -> free(stacksize)
	stEXEmem.data(size);
}
void memOP(EXEMEM_STRUCT& stEXEmem, BYTE regID, DWORD memloc, BYTE operation) {
	using namespace JIT;
	if (operation == SAVE)      stEXEmem.command("F3 0F 11"); // movss xmm? -> mem  (save)
	else if (operation == LOAD) stEXEmem.command("F3 0F 10"); // movss mem  -> xmm? (load)
	BYTE regBYTE = 0x84 | ((regID & 0x07) << 3); // 10___100: 32 bits shift, SIB following
	stEXEmem.command(1, regBYTE);
	stEXEmem.command("24"); // SIB: base = rsp
	stEXEmem.data(memloc);
}
void calcOP(EXEMEM_STRUCT& stEXEmem, BYTE sourceID, BYTE destID, BYTE operation) {
	using namespace JIT;
	switch (operation) {
		case ADD:  stEXEmem.command("F3 0F 58"); break; // addss
		case SUB:  stEXEmem.command("F3 0F 5C"); break; // subss
		case MUL:  stEXEmem.command("F3 0F 59"); break; // mulss
		case DIV:  stEXEmem.command("F3 0F 5E"); break; // divss
		case SQRT: stEXEmem.command("F3 0F 51"); break; // sqrtss
		case COPY: stEXEmem.command("F3 0F 10"); break; // movss
		default: __report_error(L"Undefined calculate operation");
	}
	if (operation == SQRT) destID = sourceID;
	BYTE regBYTE = 0xC0 | ((sourceID & 0x07) << 3) | (destID & 0x07);
	stEXEmem.command(1, regBYTE);
}
void addconstJA(EXEMEM_STRUCT& stEXEmem, float floatconst, DWORD EXEsize, DWORD counter) {
	((float*)(stEXEmem.EXEmem + EXEsize))[counter] = floatconst;
}
void loadconstJA(EXEMEM_STRUCT& stEXEmem, BYTE destID, DWORD EXEsize, DWORD constID) {
	stEXEmem.command("F3 0F 10");
	BYTE regBYTE = ((destID & 0x07) << 3) | 0x05; // 00___101: RIP + disp32
	stEXEmem.command(1, regBYTE);
	stEXEmem.data(EXEsize + constID * sizeof(float) - stEXEmem.counter - sizeof(int));
}
void callfuncJA(EXEMEM_STRUCT& stEXEmem, void* func) {
	stackOP(stEXEmem, JIT::ALLOC, 40); // 32 shadow space + 8 padding
	stEXEmem.command("48 B8"); // mov rax, imm64
	stEXEmem.data64(*(long long*)&func);
	stEXEmem.command("FF D0"); // call rax
	stackOP(stEXEmem, JIT::RECOVER, 40);
}
void finishOP(EXEMEM_STRUCT& stEXEmem) { stEXEmem.command("C3"); }

// ### func generation 3 ###############################################################################################
namespace JIT {
	const BYTE AT_REG = 1, AT_STACK = 2, INVALID = 3, FREE = 4, OCCUPIED = 5, FULL = ~0, MAX_REGS = 8;
	const DWORD I_DONT_CARE = ~0, MAX_VARS = 64, MAX_CONSTS = 64, MAX_VAR_NAME = 16, MAX_CODE = 64;
}
struct JITvar {
	BYTE regID, state;
	DWORD stackID;
	char name[JIT::MAX_VAR_NAME];
	JITvar(BYTE state, BYTE regID, DWORD stackID, const char* __name): state(state), regID(regID), stackID(stackID) {
		strcpy(name, __name);
	}
	JITvar(): state(BYTE(JIT::I_DONT_CARE)), regID(BYTE(JIT::I_DONT_CARE)), stackID(JIT::I_DONT_CARE) { }
};

namespace JIT {
	const BYTE NEW = 1, CALC = 2, SETCONST = 3, SETCONST_IDP = 4, CALLFUNC = 5, FINISH = 6;
	const char VOID_NAME[] = "__void";
}
struct JITcode {
	BYTE  op, mode;
	char  var_source[JIT::MAX_VAR_NAME], var_dest[JIT::MAX_VAR_NAME], var_addition[JIT::MAX_VAR_NAME];
	union {
		float data_f;
		DWORD data_i;
		void* funcptr;
	};
};
struct lsJITcode {
	DWORD    counter;
	JITcode* code;
	lsJITcode(DWORD max_len): counter(0),
		code((JITcode*)HeapAlloc(__g_hheap, 0, max_len * sizeof(JITcode))) { }
	~lsJITcode() {
		if (code) HeapFree(__g_hheap, 0, code);
		code = NULL;
	}
	void newvar(const char* name) {
		strcpy(code[counter].var_source, name); strcpy(code[counter].var_dest, JIT::VOID_NAME);
		code[counter++].op = JIT::NEW;
	}
	void calc(BYTE __mode, const char* source, const char* dest) {
		strcpy(code[counter].var_source, source);
		if (dest == NULL) strcpy(code[counter].var_dest, source);
		else strcpy(code[counter].var_dest, dest);
		code[counter].op = JIT::CALC; code[counter++].mode = __mode;
	}
	void setconst(const char* source, float floatconst, bool independent, const char* tag) {
		if (independent) {
			strcpy(code[counter].var_addition, tag);
			code[counter].op = JIT::SETCONST_IDP;
		} else {
			code[counter].op = JIT::SETCONST;
			code[counter].data_f = floatconst;
		}
		strcpy(code[counter].var_source, source); strcpy(code[counter++].var_dest, JIT::VOID_NAME);
	}
	void callfunc(void* func, const char* result, const char* arg_1, const char* arg_2) {
		strcpy(code[counter].var_addition, result); strcpy(code[counter].var_source, arg_1);
		if (arg_2 == NULL) strcpy(code[counter].var_dest, JIT::VOID_NAME);
		else strcpy(code[counter].var_dest, arg_2);
		code[counter].op = JIT::CALLFUNC; code[counter++].funcptr = func;
	}
	void finish(const char* ret) { strcpy(code[counter].var_source, ret); code[counter++].op = JIT::FINISH; }
};

struct lsJITvar {
	BYTE           regs[JIT::MAX_REGS];
	DWORD          code_size, const_size, stack_size, var_counter, const_counter, stack_counter, max_vars;
	JITvar*        vars;
	EXEMEM_STRUCT  stEXEmem;
	lsJITvar(DWORD code_size, DWORD stack_size, DWORD max_vars, DWORD max_consts): var_counter(2), const_counter(0),
			stack_counter(0), code_size(code_size), stack_size(stack_size),
			max_vars(max_vars), stEXEmem(code_size + max_consts * sizeof(float)),
			vars((JITvar*)HeapAlloc(__g_hheap, 0, max_vars * sizeof(JITvar))) {
		using namespace JIT;
		vars[0] = JITvar(AT_REG, 0, I_DONT_CARE, "x");
		vars[1] = JITvar(AT_REG, 1, I_DONT_CARE, "y");
		regs[0] = regs[1] = OCCUPIED;
		memset(regs + 2, FREE, MAX_REGS - 2);
		stackOP(stEXEmem, ALLOC, stack_size);
	}
	~lsJITvar() {
		if (stEXEmem.EXEmem) VirtualFree(stEXEmem.EXEmem, 0, MEM_RELEASE);
		if (vars) HeapFree(__g_hheap, 0, vars);
		stEXEmem.EXEmem = NULL; vars = NULL;
	}
	BYTE find_free_reg() {
		for (BYTE i = 0; i < JIT::MAX_REGS; i++) if (regs[i] == JIT::FREE) return i;
		return JIT::FULL;
	}
	DWORD find_var(const char* name) {
		for (DWORD i = 0; i < var_counter; i++) if (strcmp(vars[i].name, name) == 0) return i;
		__log(L"(find ~)\n", name);
		__report_error(L"Undefined variable name"); return JIT::I_DONT_CARE;
	}
	BYTE swap(BYTE protected_regID) {
		using namespace JIT;
		for (DWORD i = 0; i < var_counter; i++) {
			if (vars[i].state == AT_REG && (protected_regID == I_DONT_CARE || vars[i].regID != protected_regID)) {
				if (vars[i].stackID == I_DONT_CARE) vars[i].stackID = stack_counter++;
				__log(L"(Swap ~-reg$ to stack$)\n", vars[i].name, vars[i].regID, vars[i].stackID);
				memOP(stEXEmem, vars[i].regID, vars[i].stackID * sizeof(float), SAVE);
				vars[i].state = AT_STACK;
				regs[vars[i].regID] = FREE;
				return vars[i].regID;
			}
		}
		__report_error(L"Couldn't swap anything");
		return BYTE(JIT::I_DONT_CARE);
	}
	BYTE swap_back(const char* name, BYTE protected_regID) {
		using namespace JIT;
		DWORD source = find_var(name);
		if (vars[source].state == AT_REG) {
			__log(L"(Swap back called, but ~ is at reg $)\n", name, vars[source].regID);
			return vars[source].regID;
		}
		BYTE freereg = find_free_reg();
		if (freereg == FULL) freereg = swap(protected_regID);
		__log(L"(Swap back: ~, reg: $)\n", name, freereg);
		memOP(stEXEmem, freereg, vars[source].stackID * sizeof(float), LOAD);
		vars[source].state = AT_REG;
		vars[source].regID = freereg;
		regs[freereg] = OCCUPIED;
		return freereg;
	}
	void new_var(const char* name) { strcpy(vars[var_counter++].name, name); }
	void init_var(const char* name, BYTE regID) {
		DWORD varID = find_var(name);
		__log(L"Alloc ~->reg$\n", name, regID);
		vars[varID].state = JIT::AT_REG;
		vars[varID].regID = regID;
		vars[varID].stackID = JIT::I_DONT_CARE;
		regs[regID] = JIT::OCCUPIED;
	}
	void calc_regs(BYTE mode, BYTE sourceID, BYTE destID) {
		if (mode != JIT::COPY || sourceID != destID) {
			calcOP(stEXEmem, sourceID, destID, mode);
			__log(L"Calc: reg$ and reg$\n", sourceID, destID);
		}
	}
	DWORD load_const(BYTE regID, float floatconst) {
		DWORD stack_loc = code_size + const_counter * sizeof(float);
		addconstJA(stEXEmem, floatconst, code_size, const_counter);
		__log(L"Load const % to reg$\n", floatconst, regID);
		loadconstJA(stEXEmem, regID, code_size, const_counter++);
		regs[regID] = JIT::OCCUPIED;
		return stack_loc;
	}
	void __call_func_movarg(DWORD argID, BYTE regID) {
		using namespace JIT;
		if (vars[argID].state == AT_STACK) {
        	memOP(stEXEmem, regID, vars[argID].stackID * sizeof(float), LOAD);
    	} else if (vars[argID].state == AT_REG) {
			if (vars[argID].regID != regID)
        	calcOP(stEXEmem, vars[argID].regID, regID, COPY);
        	regs[vars[argID].regID] = FREE;
        	if (vars[argID].stackID == I_DONT_CARE) vars[argID].stackID = stack_counter++;
        	memOP(stEXEmem, regID, vars[argID].stackID * sizeof(float), SAVE);
        	vars[argID].state = AT_STACK;
    	}
    	regs[regID] = FREE; // Occupied but will be free after calling
	}
	void call_func(void* func, const char* result, const char* arg_1, const char* arg_2) {
    	using namespace JIT;
    	DWORD arg_1_ID = find_var(arg_1);
		DWORD arg_2_ID = (strcmp(arg_2, VOID_NAME) != 0) ? find_var(arg_2) : I_DONT_CARE;
		{
			__log(L"Call func, arg1 at ");
			if (vars[arg_1_ID].state == AT_STACK) __log(L"stack$, ", vars[arg_1_ID].stackID);
			else __log(L"reg$", vars[arg_1_ID].regID);
			if (arg_2_ID != I_DONT_CARE) {
   				__log(L", arg2 at ");
				if (vars[arg_2_ID].state == AT_STACK) __log(L"stack$\n", vars[arg_2_ID].stackID);
				else __log(L"reg$", vars[arg_2_ID].regID);
			}
			__log(L"\n");
		}
    	for (DWORD i = 0; i < var_counter; i++) {
        	if (vars[i].state == AT_REG) {
            	if ((i == arg_1_ID && vars[i].regID == 0) || (i == arg_2_ID && vars[i].regID == 1)) continue;
            	if (vars[i].stackID == I_DONT_CARE) vars[i].stackID = stack_counter++;
            	memOP(stEXEmem, vars[i].regID, vars[i].stackID * sizeof(float), SAVE);
            	vars[i].state = AT_STACK;
            	regs[vars[i].regID] = FREE;
        	}
    	}
    	__call_func_movarg(arg_1_ID, 0);
    	if (arg_2_ID != I_DONT_CARE) __call_func_movarg(arg_2_ID, 1);
    	callfuncJA(stEXEmem, func); // here!
    	DWORD resultID = find_var(result);
    	vars[resultID].state = AT_REG; vars[resultID].regID = 0;
    	regs[0] = OCCUPIED;
	}
	BYTE* finish(const char* name) {
		using namespace JIT;
		__log(L"Finish! return ~ at ", name);
		DWORD ret = find_var(name);
		if (vars[ret].state == AT_REG) {
			__log(L"reg$\n", vars[ret].regID);
			if (vars[ret].regID != 0) calcOP(stEXEmem, 0, vars[ret].regID, COPY);
		} else if (vars[ret].state == AT_STACK) {
			__log(L"stack$\n", vars[ret].stackID);
			memOP(stEXEmem, 0, vars[ret].stackID * sizeof(float), LOAD);
		} else __report_error(L"Uninitialized variable");
		stackOP(stEXEmem, RECOVER, stack_size);
		finishOP(stEXEmem);
		return stEXEmem.EXEmem;
	}
};

// ### func generation 4 ###############################################################################################
struct PRE_COMPILE_VAR {
	char name[JIT::MAX_VAR_NAME];
	DWORD start, end;
};
struct IDPCONST {
	char name[JIT::MAX_VAR_NAME];
	DWORD stack_loc;
};
struct lsIDPCONST {
	DWORD     counter;
	IDPCONST* consts;
	lsIDPCONST(DWORD max_consts): counter(0),
		consts((IDPCONST*)HeapAlloc(__g_hheap, 0, max_consts * sizeof(IDPCONST))) { }
	~lsIDPCONST() {
		if (consts != NULL) HeapFree(__g_hheap, 0, consts);
		consts = NULL;
	}
	void append(const char* name, DWORD __stack_loc) {
		for (DWORD i = 0; i < counter; i++) if (strcmp(consts[i].name, name) == 0) {
			consts[i].stack_loc = __stack_loc;
			return;
		}
		strcpy(consts[counter].name, name);
		consts[counter++].stack_loc = __stack_loc;
	}
	float* getloc(const char* name, BYTE* EXEmem) {
		for (DWORD i = 0; i < counter; i++) if (strcmp(consts[i].name, name) == 0)
			return (float*)(EXEmem + consts[i].stack_loc);
		__report_error(L"No such independent const"); return NULL;
	}
};
inline void __pre_compile_sort_exchange(DWORD* a, DWORD* b) { DWORD temp = *a; *a = *b; *b = temp; }
inline DWORD __pre_compile_sort_partiton(DWORD* arr, DWORD* mirror, int32_t left, int32_t right) {
	int32_t mid = left, index = left + 1; right++;
	for (DWORD i = index; i < right; i++) if (arr[i] > arr[mid]) {
		__pre_compile_sort_exchange(mirror + i, mirror + index);
		__pre_compile_sort_exchange(arr + i, arr + (index++));
	} index--;
	__pre_compile_sort_exchange(mirror + mid, mirror + index);
	__pre_compile_sort_exchange(arr + mid, arr + index);
	return index;
}
void __pre_compile_sort(DWORD* arr, DWORD* mirror, int32_t left, int32_t right) {
	if (left >= right) return;
	int32_t mid = __pre_compile_sort_partiton(arr, mirror, left, right);
	__pre_compile_sort(arr, mirror, left, mid - 1);
	__pre_compile_sort(arr, mirror, mid + 1, right);
}
void pre_compile(const lsJITcode* source, lsJITvar* dest, DWORD max_vars) {
	using namespace JIT;
	PRE_COMPILE_VAR* vars_pre = (PRE_COMPILE_VAR*)__builtin_alloca(dest->max_vars * sizeof(PRE_COMPILE_VAR));
	DWORD var_counter_pre = 0;
	for (DWORD i = 0; i < source->counter; i++) {
		for (DWORD j = 0; j < var_counter_pre; j++) if (strcmp(vars_pre[j].name, source->code[i].var_source) == 0 ||
			strcmp(vars_pre[j].name, source->code[i].var_dest) == 0) vars_pre[j].end = i;
		if (source->code[i].op == NEW) {
			strcpy(vars_pre[var_counter_pre].name, source->code[i].var_source);
			vars_pre[var_counter_pre].start = i; vars_pre[var_counter_pre++].end = i;
		}
	}
	DWORD sortby[MAX_VARS], index[MAX_VARS];
	for (DWORD i = 0; i < var_counter_pre; i++) { sortby[i] = vars_pre[i].end; index[i] = i; }
	__pre_compile_sort(sortby, index, 0, (int32_t)var_counter_pre - 1);
	for (DWORD i = 0; i < var_counter_pre; i++) dest->new_var(vars_pre[index[i]].name);
}
BYTE* compile(lsJITcode* source, lsJITvar* dest, lsIDPCONST* changeables) {
	using namespace JIT;
	pre_compile(source, dest, dest->max_vars);
	for (DWORD i = 0; i < source->counter; i++) {
		switch (source->code[i].op) {
			case NEW: {
				BYTE freereg = dest->find_free_reg();
				if (freereg == FULL) freereg = dest->swap((BYTE)I_DONT_CARE);
				dest->init_var(source->code[i].var_source, freereg);
				break;
			} case CALC: {
				BYTE reg_source = dest->swap_back(source->code[i].var_source, (BYTE)I_DONT_CARE);
				BYTE reg_dest = dest->swap_back(source->code[i].var_dest, reg_source);
				dest->calc_regs(source->code[i].mode, reg_source, reg_dest);
				break;
			} case SETCONST: case SETCONST_IDP: {
				BYTE regID = dest->swap_back(source->code[i].var_source, (BYTE)I_DONT_CARE);
				if (source->code[i].op == SETCONST_IDP) {
					if (changeables != NULL) changeables->append(
						source->code[i].var_addition, dest->load_const(regID, NCM::NAN));
					else __report_error(L"Fake consts required but disabled");
				} else dest->load_const(regID, source->code[i].data_f);
				break;
			} case CALLFUNC: dest->call_func(
				source->code[i].funcptr, source->code[i].var_addition, source->code[i].var_source,
				source->code[i].var_dest); break;
			case FINISH: return dest->finish(source->code[i].var_source);
			default: __report_error(L"Unknown code");
		}
	}
	__report_error(L"Unfinished code"); return NULL;
}

// ### func generation 5 ###############################################################################################
namespace JIT {
	const BYTE TRUE_CONST = 1, FAKE_CONST = 2, VARNAME = 3, OPERATOR = 4, FUNCNAME = 5, PAR_LEFT = 6, PAR_RIGHT = 7,
		COMMA = 8;
	const DWORD MAX_FUNC_NAME = 16;
	const char NULLCHAR = '\0';
}
constexpr DWORD __token_maxconst(DWORD a, DWORD b) { return (a > b) ? a : b; }
struct EXPRTOKEN {
	BYTE type;
	union {
		float floatval;
		char name[__token_maxconst(JIT::MAX_FUNC_NAME, JIT::MAX_VAR_NAME)];
		char singlechar;
	};
};
struct lsEXPRTOKEN {
	DWORD counter;
	EXPRTOKEN* tokens;
	lsEXPRTOKEN(DWORD max_tokens): counter(0),
		tokens((EXPRTOKEN*)HeapAlloc(__g_hheap, 0, max_tokens * sizeof(EXPRTOKEN))) { }
	~lsEXPRTOKEN() {
		if (tokens != NULL) HeapFree(__g_hheap, 0, tokens);
		tokens = NULL;
	}
	void append(BYTE type, const char* name, float floatval, char singlechar) {
		using namespace JIT;
		switch (type) {
			case TRUE_CONST: tokens[counter].floatval = floatval; break;
			case OPERATOR: tokens[counter].singlechar = singlechar; break;
			case VARNAME: case FUNCNAME: case FAKE_CONST: strcpy(tokens[counter].name, name); break;
		}
		tokens[counter++].type = type;
	}
};

inline bool __expr2token_isnum(char c) { return ('0' <= c && c <= '9') || c == '.'; }
inline bool __expr2token_isop(char c) { return c == '+' || c == '-' || c == '*' || c == '/' || c == '^'; }
inline bool __expr2token_isabc(char c) { return ('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z'); }
void expr2token(const char* expr, lsEXPRTOKEN* dest, lsIDPCONST* changeables) {
	using namespace JIT;
	DWORD exprlen = strlen(expr);
	for (DWORD i = 0; i < exprlen; i++) {
		if (expr[i] == ' ') continue;
		if (__expr2token_isnum(expr[i])) {
			float floatval = 0.f, powval = 1.f;
			bool afterdot = 0;
			do {
				if (expr[i] == '.') {
					if (afterdot) __report_error(L"Too many floating points");
					afterdot = 1;
				} else if (afterdot) floatval += float(expr[i] - '0') * (powval *= .1f);
				else floatval = floatval * 10.f + float(expr[i] - '0');
				i++;
			} while (__expr2token_isnum(expr[i]));
			dest->append(TRUE_CONST, NULL, floatval, NULLCHAR);
			i--;
		} else if (__expr2token_isop(expr[i])) dest->append(OPERATOR, NULL, NCM::NAN, expr[i]);
		else if (__expr2token_isabc(expr[i]) || expr[i] == '_') {
			bool spaced = 0, isfunc = 0;
			DWORD j = i, namelen = 1;
			while (++j < exprlen) {
				if (__expr2token_isabc(expr[j]) || __expr2token_isnum(expr[j]) || expr[j] == '_') {
					if (spaced) break;
					namelen++; continue;
				} if (expr[j] == ' ') { spaced = 1; continue; }
				if (expr[j] == '(') { isfunc = 1; break; }
				if (__expr2token_isop(expr[j])) break;
			}
			char name[MAX_VAR_NAME]; memcpy(name, expr + i, namelen); name[namelen] = '\0';
			BYTE type = FAKE_CONST;
			if (isfunc) type = FUNCNAME;
			else if (strcmp(name, "x") == 0 || strcmp(name, "y") == 0) type = VARNAME;
			dest->append(type, name, NCM::NAN, NULLCHAR);
			if (type == FAKE_CONST) __log(L"(got independent const ~)\n", name);
			i += namelen - 1;
		} else if (expr[i] == '(') dest->append(PAR_LEFT, NULL, NCM::NAN, NULLCHAR);
		else if (expr[i] == ')') dest->append(PAR_RIGHT, NULL, NCM::NAN, NULLCHAR);
		else if (expr[i] == ',') dest->append(COMMA, NULL, NCM::NAN, NULLCHAR);
		else __report_error(L"Invalid char in expr");
	}
}

typedef float (*func2F_F)(float, float);
typedef float (*func1F_F)(float);
namespace JIT { const char TEMPVAR_HEADER[MAX_VAR_NAME] = "__tmp"; }
DWORD __token2code_pairpars(lsEXPRTOKEN* source, DWORD range_left, DWORD range_right) {
	using namespace JIT;
	DWORD left_counter = 1;
	for (DWORD i = range_left + 1; i < range_right; i++) {
		if (source->tokens[i].type == PAR_LEFT) left_counter++;
		else if (source->tokens[i].type == PAR_RIGHT) {
			if (left_counter == 1) return i;
			left_counter--;
		}
	}
	__report_error(L"Unmatched parentheses --__token2code_pairpars");
	return I_DONT_CARE;
}
DWORD __token2code_findcomma(lsEXPRTOKEN* source, DWORD range_left, DWORD range_right) {
	using namespace JIT;
	DWORD left_counter = 1;
	for (DWORD i = range_left + 1; i < range_right; i++) {
		if (source->tokens[i].type == PAR_LEFT) left_counter++;
		else if (source->tokens[i].type == PAR_RIGHT) {
			if (left_counter == 0) __report_error(L"Unmatched parentheses --__token2code_findcomma");
			if (--left_counter == 0) return I_DONT_CARE;
		} else if (source->tokens[i].type == COMMA && left_counter == 1) return i;
	}
	return I_DONT_CARE; // Comma not found
}
inline char __token2code_tempname_singlechar(DWORD* counter_copy) {
	BYTE remainder = *counter_copy % 62; // 62 = 26 + 26 + 10
	*counter_copy /= 62;
	if (remainder < 26) return 'A' + remainder;
	else if (remainder < 52) return 'a' + (remainder - 26);
	return '0' + (remainder - 52);
}
void __token2code_tempname(char* dest, DWORD* strptr, DWORD* counter) {
	if (*counter == 0) dest[(*strptr)++] = 'A';
	else {
		DWORD counter_copy = *counter;
		do dest[(*strptr)++] = __token2code_tempname_singlechar(&counter_copy);
		while (counter_copy > 0);
	}
	dest[*strptr] = '\0';
	(*counter)++;
}
void __token2code_calc(char* result_name, char* tempvar, char lastest_op, lsJITcode* dest) {
	using namespace JIT;
	BYTE opJA;
	switch (lastest_op) {
		case '+': opJA = ADD; break; case '-': opJA = SUB; break;
		case '*': opJA = MUL; break; case '/': opJA = DIV; break;
		case '^': {
			dest->callfunc((void*)NCM::powf, result_name, result_name, tempvar);
			return;
		} default: __report_error(L"Undefined operator");
	}
	dest->calc(opJA, result_name, tempvar);
}
struct JITFUNC {
	char name[JIT::MAX_FUNC_NAME];
	void* funcptr;
};
struct lsJITFUNC {
	DWORD counter, max_funcs;
	JITFUNC* funcs;
	lsJITFUNC(DWORD max_funcs, bool avtivate_builtins): max_funcs(max_funcs), counter(0),
			funcs((JITFUNC*)HeapAlloc(__g_hheap, 0, max_funcs * sizeof(JITFUNC))) {
		using namespace JIT;
		if (avtivate_builtins && max_funcs >= 21) {
			append("sin", (void*)NCM::sinf); append("cos", (void*)NCM::cosf); append("tan", (void*)NCM::tanf);
			append("csc", (void*)NCM::cscf); append("sec", (void*)NCM::secf); append("cot", (void*)NCM::cotf);
			append("asin", (void*)NCM::asinf); append("acos", (void*)NCM::acosf); append("atan", (void*)NCM::atanf);
			append("acsc", (void*)NCM::acscf); append("asec", (void*)NCM::asecf); append("acot", (void*)NCM::acotf);
			append("sinh", (void*)NCM::sinhf); append("cosh", (void*)NCM::coshf); append("tanh", (void*)NCM::tanhf);
			append("csch", (void*)NCM::cschf); append("sech", (void*)NCM::sechf); append("coth", (void*)NCM::cothf);
			append("abs", (void*)NCM::absf); append("log", (void*)NCM::logf2); append("log1", (void*)NCM::logf);
		}
	}
	~lsJITFUNC() {
		if (funcs) HeapFree(__g_hheap, 0, funcs);
		funcs = NULL;
	}
	void append(const char* name, void* funcptr) {
		strcpy(funcs[counter].name, name);
		funcs[counter++].funcptr = funcptr;
	}
	void* find_func(const char* name) {
		for (DWORD i = 0; i < counter; i++) if (strcmp(funcs[i].name, name) == 0) return funcs[i].funcptr;
		__report_error(L"Unknown func name"); return NULL;
	}
};
void __token2code_tempvar(
		lsEXPRTOKEN* source, lsJITcode* dest, DWORD range_left, DWORD range_right, char* result, DWORD* counter,
		lsJITFUNC* funcenv);
DWORD __token2code_forsee_args(
		lsEXPRTOKEN* source, lsJITcode* dest, DWORD current_loc, DWORD range_right, char* arg_1, char* arg_2,
		DWORD* counter, lsJITFUNC* funcenv, bool* have_2_args) {
	using namespace JIT;
	__log(L"(Forsee called)\n");
	if (source->tokens[++current_loc].type != PAR_LEFT) __report_error(L"Bad func syntax");
	DWORD arg_1_range_right = __token2code_findcomma(source, current_loc, range_right);
	*have_2_args = 1;
	if (arg_1_range_right == I_DONT_CARE) {
		__log(L"  (Comma not found...)\n");
		arg_1_range_right = __token2code_pairpars(source, current_loc, range_right);
		*have_2_args = 0;
	} else __log(L"  (Comma found!)\n");
	__token2code_tempvar(source, dest, current_loc + 1, arg_1_range_right, arg_1, counter, funcenv);
	if (*have_2_args) {
		DWORD arg_2_range_right = __token2code_pairpars(source, arg_1_range_right, range_right);
		__token2code_tempvar(source, dest, arg_1_range_right + 1, arg_2_range_right, arg_2, counter, funcenv);
		return arg_2_range_right;
	}
	return arg_1_range_right;
}
void __token2code_tempvar(
		lsEXPRTOKEN* source, lsJITcode* dest, DWORD range_left, DWORD range_right, char* result, DWORD* counter,
		lsJITFUNC* funcenv) {
	using namespace JIT;
	strcpy(result, TEMPVAR_HEADER);
	DWORD strptr = strlen(TEMPVAR_HEADER);
	__token2code_tempname(result, &strptr, counter);
	__log(L"######### result: ~ #########\n", result);
	dest->newvar(result);
	bool init_command = 1;
	char lastest_op = (char)I_DONT_CARE;
	for (DWORD i = range_left; i < range_right; i++) {
		switch (source->tokens[i].type) {
			case TRUE_CONST: {
				__log(L"True const: %", source->tokens[i].floatval);
				if (init_command) dest->setconst(result, source->tokens[i].floatval, 0, NULL);
				else {
					char tempvar[MAX_VAR_NAME];
					strcpy(tempvar, TEMPVAR_HEADER);
					strptr = strlen(TEMPVAR_HEADER);
					__token2code_tempname(tempvar, &strptr, counter);
					dest->newvar(tempvar);
					dest->setconst(tempvar, source->tokens[i].floatval, 0, NULL);
					__token2code_calc(result, tempvar, lastest_op, dest);
					__log(L"(Calc: `)", lastest_op);
				} __log(L"\n");
				break;
			} case FAKE_CONST: {
				__log(L"Fake const: ~\n", source->tokens[i].name);
				if (init_command) dest->setconst(result, NCM::NAN, 1, source->tokens[i].name);
				else {
					char tempvar[MAX_VAR_NAME];
					strcpy(tempvar, TEMPVAR_HEADER);
					strptr = strlen(TEMPVAR_HEADER);
					__token2code_tempname(tempvar, &strptr, counter);
					dest->newvar(tempvar);
					dest->setconst(tempvar, NCM::NAN, 1, source->tokens[i].name);
					__token2code_calc(result, tempvar, lastest_op, dest);
				}
				break;
			} case VARNAME: {
				__log(L"Var: ~", source->tokens[i].name);
				if (init_command) dest->calc(COPY, result, source->tokens[i].name);
				else {
					__token2code_calc(result, source->tokens[i].name, lastest_op, dest);
					__log(L"(Calc: `)", lastest_op);
				}
				__log(L"\n");
				break;
			} case FUNCNAME: {
				__log(L"Func: ~\n", source->tokens[i].name);
    			char funcname[MAX_FUNC_NAME];
    			strcpy(funcname, source->tokens[i].name);
    			char arg_1[MAX_VAR_NAME], arg_2[MAX_VAR_NAME];
    			bool have_2_args;
    			DWORD func_end = __token2code_forsee_args(
					source, dest, i, range_right, arg_1, arg_2, counter, funcenv, &have_2_args);
    			if (strcmp(funcname, "sqrt") == 0) {
        			if (init_command) {
    					dest->calc(COPY, result, arg_1);
            			dest->calc(SQRT, result, NULL);
        			} else {
            			char tempvar[MAX_VAR_NAME];
            			strcpy(tempvar, TEMPVAR_HEADER);
            			DWORD strptr = strlen(TEMPVAR_HEADER);
            			__token2code_tempname(tempvar, &strptr, counter);
            			dest->newvar(tempvar);
            			dest->calc(COPY, tempvar, arg_1);
            			dest->calc(SQRT, tempvar, NULL);
            			__token2code_calc(result, tempvar, lastest_op, dest);
        			}
    			} else {
        			void* funcptr = funcenv->find_func(funcname);
        			if (init_command) dest->callfunc(funcptr, result, arg_1, have_2_args ? arg_2 : NULL);
        			else {
            			char tempvar[MAX_VAR_NAME];
            			strcpy(tempvar, TEMPVAR_HEADER);
            			DWORD strptr = strlen(TEMPVAR_HEADER);
            			__token2code_tempname(tempvar, &strptr, counter);
            			dest->newvar(tempvar);
            			dest->callfunc(funcptr, tempvar, arg_1, have_2_args ? arg_2 : NULL);
            			__token2code_calc(result, tempvar, lastest_op, dest);
        			}
    			}
				i = func_end;
    			break;
			} case PAR_LEFT: {
				__log(L"Parentheses\n");
				DWORD right_par_loc = __token2code_pairpars(source, i, range_right);
				char tempvar[MAX_VAR_NAME];
				__token2code_tempvar(source, dest, i + 1, right_par_loc, tempvar, counter, funcenv);
				if (init_command) dest->calc(COPY, result, tempvar);
				else __token2code_calc(result, tempvar, lastest_op, dest);
				i = right_par_loc;
				break;
			} case OPERATOR: {
				__log(L"Operator: `\n", source->tokens[i].singlechar);
				lastest_op = source->tokens[i].singlechar; break;
			}
			case PAR_RIGHT: __report_error(L"Unmatched parentheses --__token2code_tempvar"); break;
			case COMMA: __report_error(L"Comma out of func"); break;
			default: __report_error(L"Unknown token type");
		}
		init_command = 0;
	}
	__log(L"######### end: ~ #########\n", result);
}
void token2code(lsEXPRTOKEN* source, lsJITcode* dest, lsJITFUNC* funcenv) {
	char result[JIT::MAX_VAR_NAME];
	DWORD counter = 0;
	__token2code_tempvar(source, dest, 0, source->counter, result, &counter, funcenv);
	dest->finish(result);
}

// ### func generation 6 ###############################################################################################
namespace JIT { const BYTE PASS = 1, FAIL = 2; }
struct EXPR_COMPILER_INFO {
	DWORD max_tokens, max_vars, max_consts, max_funcs, code_size, stack_size;
	lsJITFUNC* external_env;
};
struct EXPR_COMPILER {
	EXPR_COMPILER_INFO info;
	func2F_F    main_executable;
	lsEXPRTOKEN tokens;
	lsIDPCONST  consts;
	lsJITFUNC*  pfuncenv;
	lsJITcode   code;
	lsJITvar    vars;
	bool        use_builtin_env;
	EXPR_COMPILER(EXPR_COMPILER_INFO* __info):
			tokens(__info->max_tokens), consts(__info->max_consts), code(__info->code_size), main_executable(NULL),
			vars(__info->code_size, __info->stack_size, __info->max_vars, __info->max_consts) {
		memcpy(&info, __info, sizeof(EXPR_COMPILER_INFO));
		if (__info->external_env == NULL) {
			use_builtin_env = 1;
			pfuncenv = new lsJITFUNC(__info->max_funcs, 1);
		} else {
			use_builtin_env = 0;
			pfuncenv = __info->external_env;
		}
	}
	~EXPR_COMPILER() {
		if (use_builtin_env && pfuncenv != NULL) delete pfuncenv;
		pfuncenv = NULL;
	}
	BYTE compile_expr(const char* expr) {
		if(!setjmp(__g_compiler_jmp)) {
			expr2token(expr, &tokens, &consts);
			token2code(&tokens, &code, pfuncenv);
			main_executable = (func2F_F)compile(&code, &vars, &consts);
		} else return JIT::FAIL;
		return JIT::PASS;
	}
	void config(const char* name, float val) {
		float* target = consts.getloc(name, vars.stEXEmem.EXEmem);
    	*target = val;
	}
	void cleanup() {
		main_executable = NULL;
		tokens.counter     = 0; consts.counter     = 0; code.counter     = 0;
		vars.const_counter = 0; vars.stack_counter = 0; vars.var_counter = 0;
	}
	inline float run(float x, float y) { return main_executable(x, y); }
};

// ### testout #########################################################################################################
/*
#include <stdio.h>
int main() {
	EXPR_COMPILER_INFO info = {
		.max_tokens = 256,
		.max_vars = 64,
		.max_consts = 64,
		.max_funcs = 64,
		.code_size = 1024,
		.stack_size = 8192,
		.external_env = NULL
	};
	EXPR_COMPILER compiler(&info);
	BYTE result = compiler.compile_expr("log(x, y) + a + b");
	compiler.config("a", 1.f);
	compiler.config("b", 2.f);
	if (result == JIT::PASS) wprintf(L"compile: %.2f (expect 7.00)\n", compiler.run(2.f, 16.f));
 else wprintf(L"Error: %ls\n", __g_error_str);
	__showlog_once;
	compiler.config("a", 3.f);
	if (result == JIT::PASS) wprintf(L"compile: %.2f (expect 9.00)\n", compiler.run(2.f, 16.f));
	return 0;
}
*/
