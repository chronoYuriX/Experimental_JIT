#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#ifndef _GNU_SOURCE
	#define _GNU_SOURCE
#endif
#include <math.h>
#ifndef NAN
	#define NAN 0.f / 0.f
#endif

#include <stdint.h>
#include <stdio.h>


// ### func generation 0 ###############################################################################################
void reportERROR(const wchar_t* str) {
	MessageBoxW(NULL, str, L"ERROR", MB_ICONERROR | MB_OK);
	exit(0);
}

// ### func generation 1 ###############################################################################################
struct EXEMEM_STRUCT {
	BYTE* EXEmem;
	DWORD counter;
	EXEMEM_STRUCT(DWORD EXEsize): counter(0),
		EXEmem((BYTE*)VirtualAlloc(NULL, EXEsize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE)) { }
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
		if (state != HIGH) reportERROR(L"Invalid hexadecimal command string");
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
		default: reportERROR(L"Undefined calculate operation");
	}
	if (operation == SQRT) destID = sourceID;
	BYTE regBYTE = 0xC0 | ((sourceID & 0x07) << 3) | (destID & 0x07);
	stEXEmem.command(1, regBYTE);
}
void addconstJA(EXEMEM_STRUCT& stEXEmem, float floatconst, DWORD EXEsize, DWORD* counter) {
	((float*)(stEXEmem.EXEmem + EXEsize))[(*counter)++] = floatconst;
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
	lsJITcode(DWORD max_len): counter(0) {
		code = (JITcode*)VirtualAlloc(NULL, max_len * sizeof(JITcode), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	}
	~lsJITcode() { VirtualFree(code, 0, MEM_RELEASE); }
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
	float*         consts;
	DWORD         *independent_const_locs, independent_const_counter;
	BYTE*          shared_mem;
	EXEMEM_STRUCT  stEXEmem;
	lsJITvar(DWORD code_size, DWORD stack_size, DWORD max_vars, DWORD max_consts): var_counter(2), const_counter(0),
			stack_counter(0), independent_const_counter(0), code_size(code_size), stack_size(stack_size),
			max_vars(max_vars), stEXEmem(code_size + max_consts * sizeof(float)) {
		using namespace JIT;
		shared_mem = (BYTE*)VirtualAlloc(
			NULL, max_vars * sizeof(JITvar) + max_consts * (sizeof(float) + sizeof(DWORD)),
			MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
		vars = (JITvar*)shared_mem;
		consts = (float*)(shared_mem + max_vars * sizeof(JITvar));
		independent_const_locs = (DWORD*)(shared_mem + max_vars * sizeof(JITvar) + max_consts * sizeof(float));
		vars[0] = JITvar(AT_REG, 0, I_DONT_CARE, "x");
		vars[1] = JITvar(AT_REG, 1, I_DONT_CARE, "y");
		regs[0] = regs[1] = OCCUPIED;
		memset(regs + 2, FREE, MAX_REGS - 2);
		stackOP(stEXEmem, ALLOC, stack_size);
	}
	~lsJITvar() {
		if (stEXEmem.EXEmem != NULL) VirtualFree(stEXEmem.EXEmem, 0, MEM_RELEASE);
		if (shared_mem != NULL) VirtualFree(shared_mem, 0, MEM_RELEASE);
		stEXEmem.EXEmem = NULL; shared_mem = NULL; vars = NULL; consts = NULL;
	}
	BYTE find_free_reg() {
		for (BYTE i = 0; i < JIT::MAX_REGS; i++) if (regs[i] == JIT::FREE) return i;
		return JIT::FULL;
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
				wprintf(L"(Swap %s-reg%d to stack%d)\n", vars[i].name, vars[i].regID, vars[i].stackID);
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
		if (vars[source].state == AT_REG) {
			wprintf(L"(Swap back called, but %s is at reg %d)\n", name, vars[source].regID);
			return vars[source].regID;
		}
		BYTE freereg = find_free_reg();
		if (freereg == FULL) freereg = swap(protected_regID);
		wprintf(L"(Swap back: %s, reg: %d)\n", name, freereg);
		memOP(stEXEmem, freereg, vars[source].stackID * sizeof(float), LOAD);
		vars[source].state = AT_REG;
		vars[source].regID = freereg;
		regs[freereg] = OCCUPIED;
		return freereg;
	}
	void new_var(const char* name) { strcpy(vars[var_counter++].name, name); }
	void init_var(const char* name, BYTE regID) {
		DWORD varID = find_var(name);
		wprintf(L"Alloc %s->reg%d\n", name, regID);
		vars[varID].state = JIT::AT_REG;
		vars[varID].regID = regID;
		vars[varID].stackID = JIT::I_DONT_CARE;
		regs[regID] = JIT::OCCUPIED;
	}
	void calc_regs(BYTE mode, BYTE sourceID, BYTE destID) {
		if (mode != JIT::COPY || sourceID != destID) {
			calcOP(stEXEmem, sourceID, destID, mode);
			wprintf(L"Calc: reg%d and reg%d\n", sourceID, destID);
		}
	}
	DWORD load_const(BYTE regID, float floatconst, bool independent) {
        DWORD constID = 0, independent_const_ID = JIT::I_DONT_CARE;
		bool notfound = 1;
		if (independent) {
			consts[const_counter] = floatconst;
			independent_const_ID = independent_const_counter;
			independent_const_locs[independent_const_counter++] = code_size + const_counter * sizeof(float);
			addconstJA(stEXEmem, NAN, code_size, &const_counter);
			constID = const_counter;
		} else {
			for (; constID < const_counter; constID++) if (consts[constID] == floatconst) { notfound = 0; break; }
			if (notfound) {
				consts[const_counter] = floatconst;
				addconstJA(stEXEmem, floatconst, code_size, &const_counter);
			}
		}
		wprintf(L"Load const %.2f to reg%d\n", floatconst, regID);
		loadconstJA(stEXEmem, regID, code_size, constID);
		regs[regID] = JIT::OCCUPIED;
		return independent_const_ID;
	}
	/*
	inline void __call_func_movarg(DWORD argID, BYTE regID) {
		if (vars[argID].state == AT_STACK) {
        	memOP(stEXEmem, regID, vars[argID].stackID * sizeof(float), LOAD);
        	vars[argID].state = AT_REG;
    	    vars[argID].regID = regID;
    	} else if (vars[arg1_id].regID != regID) {
        	calcOP(stEXEmem, vars[argID].regID, regID, COPY);
        	regs[vars[argID].regID] = FREE;
        	vars[argID].regID = regID;
    	}
	}*/
	void call_func(void* func, const char* result, const char* arg_1, const char* arg_2) {
    	using namespace JIT;
    	DWORD arg1_id = find_var(arg_1);
   		DWORD arg2_id = (strcmp(arg_2, VOID_NAME) != 0) ? find_var(arg_2) : I_DONT_CARE;

    	// 第一步：保存所有寄存器变量到栈（除了参数本身）
    	for (DWORD i = 0; i < var_counter; i++) {
        	if (vars[i].state == AT_REG && vars[i].regID < 6) {
            	if ((i == arg1_id && vars[i].regID == 0) || (i == arg2_id && vars[i].regID == 1)) continue;
            	if (vars[i].stackID == I_DONT_CARE) vars[i].stackID = stack_counter++;
            	memOP(stEXEmem, vars[i].regID, vars[i].stackID * sizeof(float), SAVE);
            	vars[i].state = AT_STACK;
            	regs[vars[i].regID] = FREE;
        	}
    	}

    	if (vars[arg1_id].state == AT_STACK) {
        	memOP(stEXEmem, 0, vars[arg1_id].stackID * sizeof(float), LOAD);
        	vars[arg1_id].state = AT_REG;
    	    vars[arg1_id].regID = 0;
    	} else if (vars[arg1_id].regID != 0) {
        	calcOP(stEXEmem, vars[arg1_id].regID, 0, COPY);
        	regs[vars[arg1_id].regID] = FREE;
        	vars[arg1_id].regID = 0;
    	}
    	regs[0] = OCCUPIED;

    	if (arg2_id != I_DONT_CARE) {
        	if (vars[arg2_id].state == AT_STACK) {
            	memOP(stEXEmem, 1, vars[arg2_id].stackID * sizeof(float), LOAD);
            	vars[arg2_id].state = AT_REG;
            	vars[arg2_id].regID = 1;
        	} else if (vars[arg2_id].regID != 1) {
            	calcOP(stEXEmem, vars[arg2_id].regID, 1, COPY);
            	regs[vars[arg2_id].regID] = FREE;
            	vars[arg2_id].regID = 1;
        	}
        	regs[1] = OCCUPIED;
    	}
    	callfuncJA(stEXEmem, func); // here!
    	DWORD resultID = find_var(result);
    	if (vars[resultID].state == AT_STACK) memOP(stEXEmem, 0, vars[resultID].stackID * sizeof(float), SAVE);
    	else if (vars[resultID].state == AT_REG) {
        	if (vars[resultID].regID != 0) {
            	calcOP(stEXEmem, 0, vars[resultID].regID, COPY);
            	regs[vars[resultID].regID] = FREE; vars[resultID].regID = 0;
        	}
    	} else reportERROR(L"Uninitialized variable");
    	vars[resultID].state = AT_REG; vars[resultID].regID = 0;
    	regs[0] = OCCUPIED;
	}
	BYTE* finish(const char* name) {
		using namespace JIT;
		wprintf(L"Finish! return %s at ", name);
		DWORD ret = find_var(name);
		if (vars[ret].state == AT_REG) {
			wprintf(L"reg%d\n", vars[ret].regID);
			if (vars[ret].regID != 0) calcOP(stEXEmem, 0, vars[ret].regID, COPY);
		} else if (vars[ret].state == AT_STACK) {
			wprintf(L"stack%d\n", vars[ret].stackID);
			memOP(stEXEmem, 0, vars[ret].stackID * sizeof(float), LOAD);
		}
		else reportERROR(L"Uninitialized variable");
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
	lsIDPCONST(DWORD max_consts): counter(0) {
		consts = (IDPCONST*)VirtualAlloc(
			NULL, max_consts * sizeof(IDPCONST), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	}
	~lsIDPCONST() {
		if (consts != NULL) VirtualFree(consts, 0, MEM_RELEASE);
		consts = NULL;
	}
	void append(const char* name) { strcpy(consts[counter++].name, name); }
	void config(const char* name, DWORD __stack_loc) {
		for (DWORD i = 0; i < counter; i++) if (strcmp(consts[i].name, name) == 0) {
			consts[i].stack_loc = __stack_loc;
			return;
		} reportERROR(L"No such independent const");
	}
	DWORD getloc(const char* name) {
		for (DWORD i = 0; i < counter; i++) if (strcmp(consts[i].name, name) == 0) return consts[i].stack_loc;
		reportERROR(L"No such independent const"); return JIT::I_DONT_CARE;
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
__fastcall void __pre_compile_sort(DWORD* arr, DWORD* mirror, int32_t left, int32_t right) {
	if (left >= right) return;
	int32_t mid = __pre_compile_sort_partiton(arr, mirror, left, right);
	__pre_compile_sort(arr, mirror, left, mid - 1);
	__pre_compile_sort(arr, mirror, mid + 1, right);
}
void pre_compile(const lsJITcode* source, lsJITvar* dest, DWORD max_vars) {
	using namespace JIT;
	PRE_COMPILE_VAR* vars_pre = (PRE_COMPILE_VAR*)VirtualAlloc(
		NULL, max_vars * sizeof(PRE_COMPILE_VAR), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
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
	VirtualFree(vars_pre, 0, MEM_RELEASE);
}
BYTE* compile(lsJITcode* source, lsJITvar* dest, DWORD max_vars, lsIDPCONST* changeables) {
	using namespace JIT;
	pre_compile(source, dest, max_vars);
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
					if (changeables != NULL) changeables->config(
						source->code[i].var_addition, dest->load_const(regID, source->code[i].data_i, 1));
					else reportERROR(L"Fake consts required but disabled");
				}
				else dest->load_const(regID, source->code[i].data_f, 0);
				break;
			} case CALLFUNC: dest->call_func(
				source->code[i].funcptr, source->code[i].var_addition, source->code[i].var_source,
				source->code[i].var_dest); break;
			case FINISH: return dest->finish(source->code[i].var_source);
			default: reportERROR(L"Unknown code");
		}
	}
	reportERROR(L"Unfinished code"); return NULL;
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
	lsEXPRTOKEN(DWORD max_tokens): counter(0) {
		tokens = (EXPRTOKEN*)VirtualAlloc(
			NULL, max_tokens * sizeof(EXPRTOKEN), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	}
	~lsEXPRTOKEN() {
		if (tokens != NULL) VirtualFree(tokens, 0, MEM_RELEASE);
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
					if (afterdot) reportERROR(L"Too many floating points");
					afterdot = 1;
				} else if (afterdot) floatval += float(expr[i] - '0') * (powval *= .1f);
				else floatval = floatval * 10.f + float(expr[i] - '0');
				i++;
			} while (__expr2token_isnum(expr[i]));
			dest->append(TRUE_CONST, NULL, floatval, NULLCHAR);
			i--;
		} else if (__expr2token_isop(expr[i])) dest->append(OPERATOR, NULL, NAN, expr[i]);
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
			else changeables->append(name);
			dest->append(type, name, NAN, NULLCHAR);
			i += namelen - 1;
		} else if (expr[i] == '(') dest->append(PAR_LEFT, NULL, NAN, NULLCHAR);
		else if (expr[i] == ')') dest->append(PAR_RIGHT, NULL, NAN, NULLCHAR);
		else if (expr[i] == ',') dest->append(COMMA, NULL, NAN, NULLCHAR);
		else reportERROR(L"Invalid char in expr");
	}
}

typedef float (*func2F_F)(float, float);
typedef float (*func1F_F)(float);
namespace JIT {
	const char TEMPVAR_HEADER[MAX_VAR_NAME] = "__tmp";
	float math_log_2F(float base, float mantissa) { return logf(mantissa) / logf(base); }
	func2F_F math_pow = powf;
	func1F_F math_abs = fabsf, math_sin = sinf, math_cos = cosf, math_tan = tanf, math_log = logf;
}
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
	reportERROR(L"Unmatched parentheses --__token2code_pairpars");
	return I_DONT_CARE;
}
DWORD __token2code_findcomma(lsEXPRTOKEN* source, DWORD range_left, DWORD range_right) {
	using namespace JIT;
	DWORD left_counter = 1;
	for (DWORD i = range_left + 1; i < range_right; i++) {
		if (source->tokens[i].type == PAR_LEFT) left_counter++;
		else if (source->tokens[i].type == PAR_RIGHT) {
			if (left_counter == 0) reportERROR(L"Unmatched parentheses --__token2code_findcomma");
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
		case '+': opJA = ADD; break;
		case '-': opJA = SUB; break;
		case '*': opJA = MUL; break;
		case '/': opJA = DIV; break;
		case '^': {
			dest->callfunc((void*)math_pow, result_name, result_name, tempvar);
			return;
		} default: reportERROR(L"Undefined operator");
	}
	dest->calc(opJA, result_name, tempvar);
}
void* __token2code_translatefunc(char* funcname) {
	using namespace JIT;
	if (strcmp(funcname, "sin") == 0) return (void*)math_sin; if (strcmp(funcname, "cos") == 0) return (void*)math_cos;
	if (strcmp(funcname, "tan") == 0) return (void*)math_tan; if (strcmp(funcname, "abs") == 0) return (void*)math_abs;
	if (strcmp(funcname, "log") == 0) return (void*)math_log_2F;
	if (strcmp(funcname, "log1") == 0) return (void*)math_log;
	reportERROR(L"Unknown func name"); return NULL;
}
void __token2code_tempvar(
		lsEXPRTOKEN* source, lsJITcode* dest, DWORD range_left, DWORD range_right, char* result, DWORD* counter);
DWORD __token2code_forsee_args(
		lsEXPRTOKEN* source, lsJITcode* dest, DWORD current_loc, DWORD range_right, char* arg_1, char* arg_2,
		DWORD* counter) {
	using namespace JIT;
	wprintf(L"(Forsee called)\n");
	if (source->tokens[++current_loc].type != PAR_LEFT) reportERROR(L"Bad func syntax");
	DWORD arg_1_range_right = __token2code_findcomma(source, current_loc, range_right);
	bool have_2_args = 1;
	if (arg_1_range_right == I_DONT_CARE) {
		wprintf(L"  (Comma not found...)\n");
		arg_1_range_right = __token2code_pairpars(source, current_loc, range_right);
		have_2_args = 0;
	} else wprintf(L"  (Comma found!)\n");
	__token2code_tempvar(source, dest, current_loc + 1, arg_1_range_right, arg_1, counter);
	if (have_2_args) {
		DWORD arg_2_range_right = __token2code_pairpars(source, arg_1_range_right, range_right);
		__token2code_tempvar(source, dest, arg_1_range_right + 1, arg_2_range_right, arg_2, counter);
		return arg_2_range_right;
	} return arg_1_range_right;
}
void __token2code_tempvar(
		lsEXPRTOKEN* source, lsJITcode* dest, DWORD range_left, DWORD range_right, char* result, DWORD* counter) {
	using namespace JIT;
	strcpy(result, TEMPVAR_HEADER);
	DWORD strptr = strlen(TEMPVAR_HEADER);
	__token2code_tempname(result, &strptr, counter);
	wprintf(L"######### result: %s #########\n", result);
	dest->newvar(result);
	bool init_command = 1;
	char lastest_op = (char)I_DONT_CARE;
	for (DWORD i = range_left; i < range_right; i++) {
		switch (source->tokens[i].type) {
			case TRUE_CONST: {
				wprintf(L"True const: %.2f", source->tokens[i].floatval);
				if (init_command) dest->setconst(result, source->tokens[i].floatval, 0, NULL);
				else {
					char tempvar[MAX_VAR_NAME];
					strcpy(tempvar, TEMPVAR_HEADER);
					strptr = strlen(TEMPVAR_HEADER);
					__token2code_tempname(tempvar, &strptr, counter);
					dest->newvar(tempvar);
					dest->setconst(tempvar, source->tokens[i].floatval, 0, NULL);
					__token2code_calc(result, tempvar, lastest_op, dest);
					wprintf(L"(Calc: %c)", lastest_op);
				} wprintf(L"\n");
				break;
			} case FAKE_CONST: {
				wprintf(L"Fake const: %s\n", source->tokens[i].name);
				if (init_command) dest->setconst(result, NAN, 1, source->tokens[i].name);
				else {
					char tempvar[MAX_VAR_NAME];
					strcpy(tempvar, TEMPVAR_HEADER);
					strptr = strlen(TEMPVAR_HEADER);
					__token2code_tempname(tempvar, &strptr, counter);
					dest->newvar(tempvar);
					dest->setconst(tempvar, NAN, 1, source->tokens[i].name);
					__token2code_calc(result, tempvar, lastest_op, dest);
				}
				break;
			} case VARNAME: {
				wprintf(L"Var: %s", source->tokens[i].name);
				if (init_command) dest->calc(COPY, result, source->tokens[i].name);
				else {
					__token2code_calc(result, source->tokens[i].name, lastest_op, dest);
					wprintf(L"(Calc: %c)", lastest_op);
				}
				wprintf(L"\n");
				break;
			} case FUNCNAME: {
				wprintf(L"Func: %s\n", source->tokens[i].name);
    			char funcname[MAX_FUNC_NAME];
    			strcpy(funcname, source->tokens[i].name);
    			char arg_1[MAX_VAR_NAME], arg_2[MAX_VAR_NAME];
    			DWORD func_end = __token2code_forsee_args(source, dest, i, range_right, arg_1, arg_2, counter);
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
        			void* funcptr = __token2code_translatefunc(funcname);
        			if (init_command) dest->callfunc(funcptr, result, arg_1, arg_2);
        			else {
            			char tempvar[MAX_VAR_NAME];
            			strcpy(tempvar, TEMPVAR_HEADER);
            			DWORD strptr = strlen(TEMPVAR_HEADER);
            			__token2code_tempname(tempvar, &strptr, counter);
            			dest->newvar(tempvar);
            			dest->callfunc(funcptr, tempvar, arg_1, arg_2);
            			__token2code_calc(result, tempvar, lastest_op, dest);
        			}
    			}
				i = func_end;
    			break;
			} case PAR_LEFT: {
				wprintf(L"Parentheses\n");
				DWORD right_par_loc = __token2code_pairpars(source, i, range_right);
				char tempvar[MAX_VAR_NAME];
				__token2code_tempvar(source, dest, i + 1, right_par_loc, tempvar, counter);
				if (init_command) dest->calc(COPY, result, tempvar);
				else __token2code_calc(result, tempvar, lastest_op, dest);
				i = right_par_loc;
				break;
			} case OPERATOR: {
				wprintf(L"Operator: %c\n", source->tokens[i].singlechar);
				lastest_op = source->tokens[i].singlechar; break;
			}
			case PAR_RIGHT: reportERROR(L"Unmatched parentheses --__token2code_tempvar"); break;
			case COMMA: reportERROR(L"Comma out of func"); break;
			default: reportERROR(L"Unknown token type");
		}
		init_command = 0;
	}
	wprintf(L"######### end: %s #########\n", result);
}
void token2code(lsEXPRTOKEN* source, lsJITcode* dest) {
	char result[JIT::MAX_VAR_NAME];
	DWORD counter = 0;
	__token2code_tempvar(source, dest, 0, source->counter, result, &counter);
	dest->finish(result);
}

void configconst(lsJITcode* code, lsJITvar* vars, lsIDPCONST* consts, const char* name, float val) {
	DWORD loc = consts->getloc(name);
    float* target = (float*)(vars->stEXEmem.EXEmem + loc);
    *target = val;
}

// ### tests ###########################################################################################################
void showHEX(const BYTE* hex, DWORD len, BYTE in1line = 4) {
	for (DWORD i = 0; i < len; i++) {
		wprintf(L"%02X ", hex[i]);
		if (i % in1line == in1line - 1) wprintf(L"\n");
	}
	wprintf(L"\n");
}
// NEW = 1, CALC = 2, SETCONST = 3, SETCONST_IDP = 4, CALLFUNC = 5, FINISH = 6;
void showcode(lsJITcode* code) {
	using namespace JIT;
	for (DWORD i = 0; i < code->counter; i++) {
		switch (code->code[i].op) {
			case NEW: wprintf(L"New: %s\n", code->code[i].var_source); break;
			case CALC: {
				char mode;
				switch (code->code[i].mode) {
					case ADD:  mode = '+'; break; case SUB:  mode = '-'; break;
					case MUL:  mode = '*'; break; case DIV:  mode = '/'; break;
					case SQRT: mode = 'S'; break; case COPY: mode = 'C'; break;
				}
				wprintf(L"Calc: %s %c %s\n", code->code[i].var_source, mode, code->code[i].var_dest);
				break;
			} case SETCONST: wprintf(L"Set: %s = %.2f\n", code->code[i].var_source, code->code[i].data_f); break;
			case SETCONST_IDP:
				wprintf(L"Set(Independent): %s = %.2f\n", code->code[i].var_addition, code->code[i].data_f); break;
			case CALLFUNC: wprintf(L"Call: %p\n", code->code[i].funcptr); break;
			case FINISH: wprintf(L"Finish!\n"); break;
		}
	}
}
void showtoken(lsEXPRTOKEN* tokens) {
	using namespace JIT;
	for (DWORD i = 0; i < tokens->counter; i++) {
		switch (tokens->tokens[i].type) {
			case TRUE_CONST: wprintf(L"True const: %.2f\n", tokens->tokens[i].floatval); break;
			case FAKE_CONST: wprintf(L"Fake const: %s\n", tokens->tokens[i].name); break;
			case VARNAME: wprintf(L"Variable: %s\n", tokens->tokens[i].name); break;
			case OPERATOR: wprintf(L"OP: %c\n", tokens->tokens[i].singlechar); break;
			case FUNCNAME: wprintf(L"Function: %s\n", tokens->tokens[i].name); break;
			case PAR_LEFT: wprintf(L"(\n"); break;
			case PAR_RIGHT: wprintf(L")\n"); break;
			case COMMA: wprintf(L",\n"); break;
		}
	}
}

void test() {
	using namespace JIT;
	lsJITcode code(256);
	lsJITvar  vars(1024, 256, 64, 64);

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
	code.setconst("a", 10.f, 0, NULL);
	code.calc(MUL, "x", "a");
	code.newvar("b");
	code.setconst("b", 9.f, 0, NULL);
	code.calc(ADD, "x", "b");

	code.finish("x");
	BYTE* EXEmem = compile(&code, &vars, vars.max_vars, NULL);
	func2F_F func = (func2F_F)EXEmem;
	wprintf(L"Code section:\n"); showHEX(EXEmem, vars.stEXEmem.counter, 16);
	wprintf(L"Const section:\n"); showHEX(EXEmem + vars.code_size, vars.const_counter * sizeof(float), 16);
	wprintf(L"compile: (sqrt(3 * 3 + 4 * 4) + 3 + 4) * 10 + 9 = %.2f (expect 129.00)\n", func(3.f, 4.f));
}

void test_overflow() {
	using namespace JIT;
	lsJITcode code(256);
	lsJITvar  vars(1024, 256, 64, 64);

	code.newvar("a");
	code.setconst("a", 3.f, 0, NULL);
	code.newvar("b");
	code.setconst("b", 4.f, 0, NULL);
	code.newvar("c");
	code.setconst("c", 5.f, 0, NULL);
	code.newvar("d");
	code.setconst("d", 6.f, 0, NULL);
	code.newvar("e");
	code.setconst("e", 7.f, 0, NULL);
	code.newvar("f");
	code.setconst("f", 8.f, 0, NULL);
	code.newvar("g");
	code.setconst("g", 9.f, 0, NULL);

	code.calc(ADD, "x", "y");
	code.calc(ADD, "x", "a");
	code.calc(ADD, "x", "b");
	code.calc(ADD, "x", "c");
	code.calc(ADD, "x", "d");
	code.calc(ADD, "x", "e");
	code.calc(ADD, "x", "f");
	code.calc(ADD, "x", "g");
	code.finish("x");

	BYTE* EXEmem = compile(&code, &vars, vars.max_vars, NULL);
	func2F_F func = (func2F_F)EXEmem;
	wprintf(L"Code section:\n"); showHEX(EXEmem, vars.stEXEmem.counter, 16);
	wprintf(L"Const section:\n"); showHEX(EXEmem + vars.code_size, vars.const_counter * sizeof(float), 16);
	wprintf(L"compile: sigma 1~9 = %.2f (expect 45.00)\n", func(1.f, 2.f));
}

float test_call_mul_add(float x, float y) { return x * y + x + y; }
void test_call() {
	using namespace JIT;
	lsJITcode code(256);
	lsJITvar  vars(1024, 256, 64, 64);

	code.newvar("unused");
	code.setconst("unused", 114514.f, 0, NULL);
	code.calc(SUB, "unused", "x");
	code.callfunc((void*)test_call_mul_add, "x", "x", "y");
	code.finish("x");

	BYTE* EXEmem = compile(&code, &vars, vars.max_vars, NULL);
	func2F_F func = (func2F_F)EXEmem;
	wprintf(L"Code section:\n"); showHEX(EXEmem, vars.stEXEmem.counter, 16);
	wprintf(L"Const section:\n"); showHEX(EXEmem + vars.code_size, vars.const_counter * sizeof(float), 16);
	wprintf(L"compile: 4 * 5 + 4 + 5 = %.2f (with interference, expect 29.00)\n", func(4.f, 5.f));
}

void test_expr_simple() {
	using namespace JIT;
	lsEXPRTOKEN tokens(64);
	lsIDPCONST  consts(64);
	lsJITcode   code(256);
	lsJITvar    vars(1024, 256, 64, 64);
	expr2token("1 + 2 * ((7 - 4) * 5 / sqrt(9)) - 10 + x + y", &tokens, &consts); // 5 + x + y
	showtoken(&tokens); wprintf(L"########################################\n");
	token2code(&tokens, &code);
	showcode(&code);    wprintf(L"########################################\n");
	BYTE* EXEmem = compile(&code, &vars, vars.max_vars, &consts);
	func2F_F func = (func2F_F)EXEmem;
	wprintf(L"Code section:\n"); showHEX(EXEmem, vars.stEXEmem.counter, 16);
	wprintf(L"Const section:\n"); showHEX(EXEmem + vars.code_size, vars.const_counter * sizeof(float), 16);
	wprintf(L"compile: 1 + 2 * ((7 - 4) * 5 / sqrt(9)) - 10 + x:1 + y:1 = %.2f (expect 7.00)\n", func(1.f, 1.f));
}

void test_expr() {
	using namespace JIT;
	lsEXPRTOKEN tokens(64);
	lsIDPCONST  consts(64);
	expr2token("1 + 2 * (3 - 4 / sqrt(x)   ) ^ (log(a, b) - (y - n))", &tokens, &consts);
	showtoken(&tokens);
	wprintf(L"########################################\n");
	for (DWORD i = 0; i < consts.counter; i++) wprintf(L"%s\n", consts.consts[i].name);
	wprintf(L"########################################\n");
	lsJITcode code(256);
	lsJITvar  vars(1024, 256, 64, 64);
	wprintf(L"Step.1\n");
	token2code(&tokens, &code);
	wprintf(L"Step.2\n");
	BYTE* EXEmem = compile(&code, &vars, vars.max_vars, &consts);
	wprintf(L"Step.3\n");
	//configconst(&code, &vars, &consts, "a", 3.f);
	//configconst(&code, &vars, &consts, "b", 27.f);
	//configconst(&code, &vars, &consts, "n", 4.f);
	wprintf(L"Step.4\n");
	func2F_F func = (func2F_F)EXEmem;
	wprintf(L"Code section:\n"); showHEX(EXEmem, vars.stEXEmem.counter, 16);
	wprintf(L"Const section:\n"); showHEX(EXEmem + vars.code_size, vars.const_counter * sizeof(float), 16);
	wprintf(L"compile: 1 + 2 * (3 - 4 / sqrt(x: 4.0)) ^ (log(a: 3.0, b: 27.0) - (y: 5.0 - n: 4.0)) = %.2f\n", func(4.f, 5.f));
}


int main() {
	//test();
	//test_overflow();
	//test_call();
	test_expr_simple();
	//test_expr();
	return 0;
}

