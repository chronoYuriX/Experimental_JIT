#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <stdint.h>
#include <stdio.h>

// ### func generation 0 ###############################################################################################
void reportERROR(const wchar_t* str) {
	MessageBoxW(NULL, str, L"ERROR", MB_ICONERROR | MB_OK);
}
void showHEX(const BYTE* hex, DWORD len, BYTE in1line = 4) {
	for (DWORD i = 0; i < len; i++) {
		wprintf(L"%02X ", hex[i]);
		if (i % in1line == in1line - 1) wprintf(L"\n");
	}
	wprintf(L"\n");
}

// ### func generation 1 ###############################################################################################
struct EXEMEM_STRUCT {
	BYTE* EXEmem;
	DWORD count;
	EXEMEM_STRUCT(DWORD EXEsize): count(0),
		EXEmem((BYTE*)VirtualAlloc(NULL, EXEsize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE)) { }
	inline void command(BYTE total, ...) {
		va_list commands;
		va_start(commands, total);
		for (; total > 0; total--) EXEmem[count++] = BYTE(va_arg(commands, int)) & 0xFF;
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
						EXEmem[count++] = cur_x;
						state = HIGH; break;
					}
					cur_b |= (command_str[i] == '1') ? 1 : 0;
					state = BIN; break;
				} case BIN: {
					if (command_str[i] == ' ' || command_str[i] == '\0') {
						EXEmem[count++] = cur_b;
						state = HIGH; break;
					}
					cur_b = (cur_b << 1) | ((command_str[i] == '1') ? 1 : 0);
					break;
				}
			}
		}
		if (state != HIGH) reportERROR(L"Invalid hexadecimal command string");
	}
	inline void data(int intval) {
		BYTE* bytes = (BYTE*)&intval;
		memcpy(EXEmem + count, bytes, sizeof(int));
		count += sizeof(int);
	}
};

// ### func generation 2 ###############################################################################################
namespace JIT {
	const BYTE
		ALLOC =   1, RECOVER =   2, // stack operation
		SAVE  =   3, LOAD    =   4, // save/load register to/from stack
		ADD   =   5, SUB     =   6, MUL     =   7, DIV = 8, SQRT = 9, // +-*/ and sqrt
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
		default: reportERROR(L"Undefined calculate operation");
	}
	if (operation == SQRT) destID = sourceID;
	BYTE regBYTE = 0xC0 | ((sourceID & 0x07) << 3) | (destID & 0x07);
	stEXEmem.command(1, regBYTE);
}
void addconst(EXEMEM_STRUCT& stEXEmem, float floatconst, DWORD EXEsize, DWORD* counter) {
	((float*)(stEXEmem.EXEmem + EXEsize))[(*counter)++] = floatconst;
}
void loadconst(EXEMEM_STRUCT& stEXEmem, BYTE destID, DWORD EXEsize, DWORD constID) {
	stEXEmem.command("F3 0F 10");
	BYTE regBYTE = (destID << 3) | 0x05; // 00___101: RIP + disp32
	stEXEmem.command(1, regBYTE);
	stEXEmem.data(EXEsize + constID * sizeof(float) - stEXEmem.count - sizeof(int));
}
void finishOP(EXEMEM_STRUCT& stEXEmem) {
	stEXEmem.command("C3");
}

// ### func generation 3 ###############################################################################################
namespace JIT {
	const BYTE AT_REG = 1, AT_STACK = 2, INVALID = 3, FREE = 4, OCCUPIED = 5, FULL = ~0, MAX_REGS = 4;
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
	const BYTE NEW = 1, CALC = 2, SETCONST = 3, FINISH = 4;
	const char VOID_NAME[] = "__void";
}
struct JITcode {
	BYTE op, mode;
	float data;
	char var_source[JIT::MAX_VAR_NAME], var_dest[JIT::MAX_VAR_NAME];
};
struct lsJITcode {
	DWORD len;
	JITcode code[JIT::MAX_CODE];
	lsJITcode(): len(0) { }
	void newvar(const char* name) {
		strcpy(code[len].var_source, name); strcpy(code[len].var_dest, JIT::VOID_NAME); code[len++].op = JIT::NEW;
	}
	void calc(BYTE __mode, const char* source, const char* dest) {
		strcpy(code[len].var_source, source);
		if (dest == NULL) strcpy(code[len].var_dest, source);
		else strcpy(code[len].var_dest, dest);
		code[len].op = JIT::CALC; code[len++].mode = __mode;
	}
	void setconst(const char* source, float floatconst) {
		strcpy(code[len].var_source, source); strcpy(code[len].var_dest, JIT::VOID_NAME);
		code[len].op = JIT::SETCONST; code[len++].data = floatconst;
	}
	void finish(const char* ret) {
		strcpy(code[len].var_source, ret); code[len++].op = JIT::FINISH;
	}
};

struct lsJITvar {
	BYTE          regs[JIT::MAX_REGS];
	DWORD         code_size, const_size, stack_size, var_counter, const_counter, stack_counter, step_counter;
	JITvar        vars[JIT::MAX_VARS];
	float         consts[JIT::MAX_CONSTS];
	EXEMEM_STRUCT stEXEmem;
	lsJITvar(DWORD code_size, DWORD const_size, DWORD stack_size): var_counter(2), const_counter(0), stack_counter(0),
			code_size(code_size), const_size(const_size), stack_size(stack_size), stEXEmem(code_size + const_size),
			step_counter(0) {
		using namespace JIT;
		vars[0] = JITvar(AT_REG, 0, I_DONT_CARE, "x");
		vars[1] = JITvar(AT_REG, 1, I_DONT_CARE, "y");
		regs[0] = regs[1] = OCCUPIED;
		memset(regs + 2, FREE, MAX_REGS - 2);
		stackOP(stEXEmem, ALLOC, stack_size);
	}
	BYTE find_free_reg() {
		using namespace JIT;
		for (BYTE i = 0; i < MAX_REGS; i++) if (regs[i] == FREE) return i;
		return FULL;
	}
	DWORD find_var(const char* name) {
		for (DWORD i = 0; i < var_counter; i++) if (strcmp(vars[i].name, name) == 0) return i;
		reportERROR(L"Undefined variable name"); return JIT::I_DONT_CARE;
	}
	BYTE swap(BYTE protected_regID) {
		using namespace JIT;
		for (DWORD i = 0; i < var_counter; i++) {
			if (vars[i].state == AT_REG && (protected_regID == I_DONT_CARE || vars[i].regID != protected_regID)) {
				if (vars[i].stackID == I_DONT_CARE) vars[i].stackID = stack_counter++;
				memOP(stEXEmem, vars[i].regID, vars[i].stackID * sizeof(float), SAVE);
				vars[i].state = AT_STACK;
				regs[vars[i].regID] = FREE;
				return vars[i].regID;
			}
		}
		reportERROR(L"Couldn't swap anything");
		return BYTE(JIT::I_DONT_CARE);
	}
	BYTE swap_back(const char* name, BYTE protected_regID) {
		using namespace JIT;
		DWORD source = find_var(name);
		if (vars[source].state == AT_REG) return vars[source].regID;
		BYTE freereg = find_free_reg();
		if (freereg == FULL) freereg = swap(protected_regID);
		memOP(stEXEmem, freereg, vars[source].stackID * sizeof(float), LOAD);
		vars[source].state = AT_REG;
		vars[source].regID = freereg;
		regs[freereg] = OCCUPIED;
		return freereg;
	}
	void new_var(const char* name) { strcpy(vars[var_counter++].name, name); }
	void init_var(const char* name, BYTE regID) {
		DWORD varID = find_var(name);
		vars[varID].state = JIT::AT_REG;
		vars[varID].regID = regID;
		regs[regID] = JIT::OCCUPIED;
	}
	void calc_regs(BYTE mode, BYTE sourceID, BYTE destID) {
		if (mode != JIT::COPY || sourceID != destID) calcOP(stEXEmem, sourceID, destID, mode);
	}
	void load_const(BYTE regID, float floatconst) {
        DWORD constID = 0;
		bool notfound = 1;
		for (; constID < const_counter; constID++) if (consts[constID] == floatconst) { notfound = 0; break; }
		if (notfound) {
			consts[const_counter] = floatconst;
			addconst(stEXEmem, floatconst, code_size, &const_counter);
		}
		loadconst(stEXEmem, regID, code_size, constID);
		regs[regID] = JIT::OCCUPIED;
	}
	BYTE* finish(const char* name) {
		using namespace JIT;
		DWORD ret = find_var(name);
		if (vars[ret].state == AT_REG) {
			if (vars[ret].regID != 0) calcOP(stEXEmem, 0, vars[ret].regID, COPY);
		} else if (vars[ret].state == AT_STACK) memOP(stEXEmem, 0, vars[ret].stackID, LOAD);
		else reportERROR(L"Uninitialized variable");
		stackOP(stEXEmem, RECOVER, stack_size);
		finishOP(stEXEmem);
		return stEXEmem.EXEmem;
	}
};

struct PRE_COMPILE_VAR {
	char name[JIT::MAX_VAR_NAME];
	DWORD start, end;
};
inline void pre_compile_sort_exchange(DWORD* a, DWORD* b) { DWORD temp = *a; *a = *b; *b = temp; }
inline DWORD pre_compile_sort_partiton(DWORD* arr, DWORD* mirror, int32_t left, int32_t right) {
	int32_t mid = left, index = left + 1; right++;
	for (DWORD i = index; i < right; i++) if (arr[i] > arr[mid]) {
		pre_compile_sort_exchange(mirror + i, mirror + index);
		pre_compile_sort_exchange(arr + i, arr + (index++));
	} index--;
	pre_compile_sort_exchange(mirror + mid, mirror + index);
	pre_compile_sort_exchange(arr + mid, arr + index);
	return index;
}
__fastcall void pre_compile_sort(DWORD* arr, DWORD* mirror, int32_t left, int32_t right) {
	if (left >= right) return;
	int32_t mid = pre_compile_sort_partiton(arr, mirror, left, right);
	pre_compile_sort(arr, mirror, left, mid - 1);
	pre_compile_sort(arr, mirror, mid + 1, right);
}
void pre_compile(const lsJITcode* source, lsJITvar* dest) {
	using namespace JIT;
	PRE_COMPILE_VAR vars_pre[MAX_VARS];
	DWORD var_counter_pre = 0;
	for (DWORD i = 0; i < source->len; i++) {
		for (DWORD j = 0; j < var_counter_pre; j++) if (strcmp(vars_pre[j].name, source->code[i].var_source) == 0 ||
			strcmp(vars_pre[j].name, source->code[i].var_dest) == 0) vars_pre[j].end = i;
		if (source->code[i].op == NEW) {
			strcpy(vars_pre[var_counter_pre].name, source->code[i].var_source);
			vars_pre[var_counter_pre].start = i; vars_pre[var_counter_pre++].end = i;
		}
	}
	DWORD sortby[MAX_VARS], index[MAX_VARS];
	for (DWORD i = 0; i < var_counter_pre; i++) { sortby[i] = vars_pre[i].end; index[i] = i; }
	pre_compile_sort(sortby, index, 0, (int32_t)var_counter_pre - 1);
	for (DWORD i = 0; i < var_counter_pre; i++) dest->new_var(vars_pre[index[i]].name);
}
BYTE* compile(lsJITcode* source, lsJITvar* dest) {
	using namespace JIT;
	pre_compile(source, dest);
	for (DWORD i = 0; i < source->len; i++) {
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
			} case SETCONST: {
				BYTE regID = dest->swap_back(source->code[i].var_source, (BYTE)I_DONT_CARE);
				dest->load_const(regID, source->code[i].data);
				break;
			} case FINISH: return dest->finish(source->code[i].var_source);
			default: reportERROR(L"Unknown code");
		}
	}
	reportERROR(L"Unfinished code"); return NULL;
}

// ### tests ###########################################################################################################
typedef float (*func2F_F)(float, float);
void test() {
	using namespace JIT;
	lsJITcode code;
	lsJITvar  vars(1024, 256, 256);
	
	code.newvar("m");
	code.calc(COPY, "m", "x");
	code.newvar("n");
	code.calc(COPY, "n", "y");
	
	code.calc(MUL, "x", "x");
	code.calc(MUL, "y", "y");
	code.calc(ADD, "x", "y");
	code.calc(SQRT, "x", NULL);
	
	code.calc(ADD, "x", "m");
	code.calc(ADD, "x", "n");

	code.newvar("a");
	code.setconst("a", 10.f);
	code.calc(MUL, "x", "a");
	code.newvar("b");
	code.setconst("b", 9.f);
	code.calc(ADD, "x", "b");

	code.finish("x");
	BYTE* EXEmem = compile(&code, &vars);
	func2F_F func = (func2F_F)EXEmem;
	wprintf(L"Code section:\n"); showHEX(EXEmem, vars.stEXEmem.count, 16);
	wprintf(L"Const section:\n"); showHEX(EXEmem + vars.code_size, vars.const_counter * sizeof(float), 16);
	wprintf(L"compile: (sqrt(3 * 3 + 4 * 4) + 3 + 4) * 10 + 9 = %.2f (expect 129.00)\n", func(3.f, 4.f));
	VirtualFree(EXEmem, 0, MEM_RELEASE);
}

void test_overflow() {
	using namespace JIT;
	lsJITcode code;
	lsJITvar  vars(1024, 256, 256);
	
	code.newvar("a");
	code.setconst("a", 3.f);
	code.newvar("b");
	code.setconst("b", 4.f);
	code.newvar("c");
	code.setconst("c", 5.f);
	code.newvar("d");
	code.setconst("d", 6.f);
	code.newvar("e");
	code.setconst("e", 7.f);
	code.newvar("f");
	code.setconst("f", 8.f);
	code.newvar("g");
	code.setconst("g", 9.f);
	
	code.calc(ADD, "x", "y");
	code.calc(ADD, "x", "a");
	code.calc(ADD, "x", "b");
	code.calc(ADD, "x", "c");
	code.calc(ADD, "x", "d");
	code.calc(ADD, "x", "e");
	code.calc(ADD, "x", "f");
	code.calc(ADD, "x", "g");
	code.finish("x");
	
	BYTE* EXEmem = compile(&code, &vars);
	func2F_F func = (func2F_F)EXEmem;
	wprintf(L"Code section:\n"); showHEX(EXEmem, vars.stEXEmem.count, 16);
	wprintf(L"Const section:\n"); showHEX(EXEmem + vars.code_size, vars.const_counter * sizeof(float), 16);
	wprintf(L"compile: sigma 1~9 = %.2f (expect 45.00)\n", func(1.f, 2.f));
	VirtualFree(EXEmem, 0, MEM_RELEASE);
}





int main() {
	test();
	test_overflow();
	return 0;
}

