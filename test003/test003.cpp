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
	SIZE_T count;
	EXEMEM_STRUCT(SIZE_T EXEsize): count(0),
		EXEmem((BYTE*)VirtualAlloc(NULL, EXEsize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE)) { }
	void command(BYTE total, ...) {
		va_list commands;
		va_start(commands, total);
		for (; total > 0; total--) EXEmem[count++] = BYTE(va_arg(commands, int)) & 0xFF;
		va_end(commands);
	}
	BYTE __HEXchar2halfBYTE(char c) {
		if ('0' <= c && c <= '9') return c - '0';
		else if ('A' <= c && c <= 'F') return c - 'A' + 10;
		return 0;
	}
	void command(const char* command_str, DWORD len = ~0) {
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
	void data(int intval) {
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
void stackOP(EXEMEM_STRUCT& sEXEmem, BYTE operation, DWORD size) {
	using namespace JIT;
	size = (size + 15) & ~15; // Compensate the return address, then align to 16 bytes
	if (operation == ALLOC)        sEXEmem.command("48 81 EC"); // sub rsp, stacksize -> alloca(stacksize)
	else if (operation == RECOVER) sEXEmem.command("48 81 C4"); // add rsp, stacksize -> free(stacksize)
	sEXEmem.data(size);
}
void memOP(EXEMEM_STRUCT& sEXEmem, BYTE regID, DWORD memloc, BYTE operation) {
	using namespace JIT;
	if (operation == SAVE)      sEXEmem.command("F3 0F 11"); // movss xmm? -> mem  (save)
	else if (operation == LOAD) sEXEmem.command("F3 0F 10"); // movss mem  -> xmm? (load)
	BYTE regBYTE = 0x84 | ((regID & 0x07) << 3); // 10___100: 32 bits shift, SIB following
	sEXEmem.command(1, regBYTE);
	sEXEmem.command("24"); // SIB: base = rsp
	sEXEmem.data(memloc);
}
void calcOP(EXEMEM_STRUCT& sEXEmem, BYTE sourceID, BYTE destID, BYTE operation) {
	using namespace JIT;
	switch (operation) {
		case ADD:  sEXEmem.command("F3 0F 58"); break; // addss
		case SUB:  sEXEmem.command("F3 0F 5C"); break; // subss
		case MUL:  sEXEmem.command("F3 0F 59"); break; // mulss
		case DIV:  sEXEmem.command("F3 0F 5E"); break; // divss
		case SQRT: sEXEmem.command("F3 0F 51"); break; // sqrtss
		case COPY: sEXEmem.command("F3 0F 10"); break; // movss
		default: reportERROR(L"Undefined calculate operation");
	}
	if (operation == SQRT) destID = sourceID;
	BYTE regBYTE = 0xC0 | ((sourceID & 0x07) << 3) | (destID & 0x07);
	sEXEmem.command(1, regBYTE);
}
void addconst(EXEMEM_STRUCT& sEXEmem, float floatconst, DWORD EXEsize, DWORD& counter) {
	((float*)(sEXEmem.EXEmem + EXEsize))[counter++] = floatconst;
}
void loadconst(EXEMEM_STRUCT& sEXEmem, BYTE destID, DWORD EXEsize, DWORD constID) {
	sEXEmem.command("F3 0F 10");
	BYTE regBYTE = (destID << 3) | 0x05; // 00___101: RIP + disp32
	sEXEmem.command(1, regBYTE);
	sEXEmem.data(EXEsize + constID * sizeof(float) - sEXEmem.count - sizeof(int));
}
void finishOP(EXEMEM_STRUCT& sEXEmem) {
	sEXEmem.command("C3");
}

// ### func generation 3 ###############################################################################################
namespace JIT {
	const BYTE FREE = 1, OCCUPIED = 2, // registers' states
		NOWHERE = 3, AT_REG = 4, AT_STACK = 5, AT_BOTH = 6, AT_BOTH_ASYNC = 7, // virtual variables' states
		ALL_REGS_FULL = ~0, ALL_REGS = 7, RESERVED_REG_ID = 7;
	const DWORD I_DONT_CARE = ~0, NOT_HERE = ~0, MAX_VAR_NAME = 16, MAX_V_VARS = 16, MAX_CONSTS = 16;
}
struct VIRTUAL_VAR {
	BYTE REGloc, state;
	DWORD MEMloc;
	char name[JIT::MAX_VAR_NAME];
	VIRTUAL_VAR(BYTE REGloc, DWORD MEMloc, BYTE state, const char* varname):
			REGloc(REGloc), MEMloc(MEMloc), state(state) { strcpy(name, varname); }
	VIRTUAL_VAR(): REGloc(BYTE(JIT::I_DONT_CARE)), MEMloc(JIT::I_DONT_CARE), state(JIT::NOWHERE) { }
};
struct JIT_COMMANDER {
	BYTE          registers[JIT::ALL_REGS];
	DWORD         EXEsize, constsize, stacksize, stack_count, v_var_count, const_count;
	VIRTUAL_VAR   v_vars[JIT::MAX_V_VARS];
	float         consts[JIT::MAX_CONSTS];
	EXEMEM_STRUCT sEXEmem;
	JIT_COMMANDER(DWORD max_vars, DWORD EXEsize, DWORD constsize, DWORD stacksize):
			EXEsize(EXEsize), constsize(constsize), stacksize(stacksize), stack_count(0), v_var_count(2),
			const_count(0), sEXEmem(EXEsize + constsize) {
		using namespace JIT;
		v_vars[0] = VIRTUAL_VAR(0, I_DONT_CARE, AT_REG, "x");
		v_vars[1] = VIRTUAL_VAR(1, I_DONT_CARE, AT_REG, "y");
		memset(registers, FREE, ALL_REGS);
		registers[0] = OCCUPIED;
		registers[1] = OCCUPIED;
		stackOP(sEXEmem, ALLOC, stacksize);
	}
	DWORD __findvar(const char* name) {
		for (DWORD i = 0; i < v_var_count; i++) if (strcmp(name, v_vars[i].name) == 0) return i;
		return JIT::NOT_HERE;
	}
	DWORD __newconst(float val) {
		for (DWORD i = 0; i < const_count; i++) if (val == consts[i]) return i;
		DWORD cur_const_count = const_count;
		addconst(sEXEmem, val, EXEsize, const_count);
		return cur_const_count;
	}
	BYTE __findfreereg() {
		using namespace JIT;
		for (BYTE i = 0; i < ALL_REGS; i++) if (registers[i] == FREE) return i;
		return ALL_REGS_FULL;
	}
	void set_v_var(const char* name, float val) {
		using namespace JIT;
		DWORD varID = __findvar(name);
		if (varID == NOT_HERE) {
			DWORD loadfrom = __newconst(val);
			strcpy(v_vars[v_var_count].name, name);
			BYTE freereg = __findfreereg();
			if (freereg == ALL_REGS_FULL) {
				loadconst(sEXEmem, RESERVED_REG_ID, EXEsize, loadfrom);
				memOP(sEXEmem, RESERVED_REG_ID, stack_count * sizeof(float), SAVE);
				v_vars[v_var_count].REGloc = BYTE(I_DONT_CARE); v_vars[v_var_count].MEMloc = stack_count++;
				v_vars[v_var_count].state = AT_STACK;
			} else {
				registers[freereg] = OCCUPIED;
				v_vars[v_var_count].REGloc = freereg;
				v_vars[v_var_count].MEMloc = I_DONT_CARE;
				v_vars[v_var_count].state = AT_REG;
				loadconst(sEXEmem, freereg, EXEsize, loadfrom);
			}
			v_var_count++;
		} else {
			switch (v_vars[varID].state) {
				case AT_REG: case AT_BOTH: case AT_BOTH_ASYNC: {
					loadconst(sEXEmem, v_vars[varID].REGloc, EXEsize, __newconst(val));
					if (v_vars[varID].state == AT_BOTH) v_vars[varID].state = AT_BOTH_ASYNC;
					break;
				} case AT_STACK: {
					loadconst(sEXEmem, RESERVED_REG_ID, EXEsize, __newconst(val));
					memOP(sEXEmem, RESERVED_REG_ID, v_vars[varID].MEMloc * sizeof(float), SAVE);
					break;
				} case NOWHERE: reportERROR(L"Invalid variable state ( while setting a variable' value )"); break;
			}
		}
	}
	bool SWAPsomething() { // return 0 if nothing is swapped
		using namespace JIT;
		bool swapped = 0;
		for (DWORD i = 0; i < v_var_count; i++) if (v_vars[i].state == AT_BOTH_ASYNC || v_vars[i].state == AT_BOTH) {
			v_vars[i].state = AT_STACK;
			registers[v_vars[i].REGloc] = FREE;
			swapped = 1;
		}
		return swapped;
	}
	void forceSWSP() {
		using namespace JIT;
		for (DWORD i = 0; i < v_var_count; i++) if (v_vars[i].state == AT_REG) {
			memOP(sEXEmem, RESERVED_REG_ID, stack_count * sizeof(float), SAVE);
			registers[v_vars[i].REGloc] = FREE;
			v_vars[i].REGloc = BYTE(I_DONT_CARE); v_vars[i].MEMloc = stack_count++;
			v_vars[i].state = AT_STACK;
		}
	}
	void calc_v_vars(const char* source_name, const char* dest_name, BYTE operation) {
		using namespace JIT;
		if (dest_name == NULL) dest_name = source_name;
		DWORD soueceID =__findvar(source_name), destID = __findvar(dest_name);
		if (soueceID == NOT_HERE || destID == NOT_HERE) reportERROR(L"Undefined variable name ( before calculating )");
		switch (v_vars[soueceID].state) {
			case AT_REG: case AT_BOTH: case AT_BOTH_ASYNC: {
				switch (v_vars[destID].state) {
					case AT_REG: case AT_BOTH: case AT_BOTH_ASYNC:
						calcOP(sEXEmem, v_vars[soueceID].REGloc, v_vars[destID].REGloc, operation); break;
					case AT_STACK: {
						memOP(sEXEmem, RESERVED_REG_ID, v_vars[destID].MEMloc * sizeof(float), LOAD);
						calcOP(sEXEmem, v_vars[soueceID].REGloc, RESERVED_REG_ID, operation);
						break;
					} case NOWHERE: reportERROR(L"Invalid variable state ( while calculating )"); break;
				}
				if (v_vars[destID].state == AT_BOTH) v_vars[destID].state = AT_BOTH_ASYNC;
				break;
			} case AT_STACK: {
				BYTE freereg = __findfreereg();
				if (freereg == ALL_REGS_FULL) {
					memOP(sEXEmem, RESERVED_REG_ID, v_vars[soueceID].MEMloc * sizeof(float), LOAD);
					switch (v_vars[destID].state) {
						case AT_REG: case AT_BOTH: case AT_BOTH_ASYNC:
							calcOP(sEXEmem, RESERVED_REG_ID, v_vars[destID].REGloc, operation); break;
						case AT_STACK: {
							if (!SWAPsomething()) forceSWSP();
							freereg = __findfreereg();
							memOP(sEXEmem, freereg, v_vars[destID].MEMloc * sizeof(float), LOAD);
							v_vars[destID].REGloc = freereg;
							v_vars[destID].state = AT_BOTH;
							registers[freereg] = OCCUPIED;
							calcOP(sEXEmem, RESERVED_REG_ID, freereg, operation);
							break;
						} case NOWHERE: reportERROR(L"Invalid variable state ( while calculating )"); break;
					}
					memOP(sEXEmem, RESERVED_REG_ID, v_vars[soueceID].MEMloc * sizeof(float), SAVE);
				} else {
					memOP(sEXEmem, freereg, v_vars[soueceID].MEMloc * sizeof(float), LOAD);
					v_vars[soueceID].REGloc = freereg;
					v_vars[soueceID].state = AT_BOTH_ASYNC;
					registers[freereg] = OCCUPIED;
					switch (v_vars[destID].state) {
						case AT_REG: case AT_BOTH: case AT_BOTH_ASYNC:
							calcOP(sEXEmem, freereg, v_vars[destID].REGloc, operation); break;
						case AT_STACK: {
							BYTE freereg2 = __findfreereg();
							if (freereg2 == ALL_REGS_FULL) {
								memOP(sEXEmem, RESERVED_REG_ID, v_vars[destID].MEMloc * sizeof(float), LOAD);
								calcOP(sEXEmem, freereg, RESERVED_REG_ID, operation);
							} else {
								memOP(sEXEmem, freereg2, v_vars[destID].MEMloc * sizeof(float), LOAD);
								v_vars[destID].REGloc = freereg2;
								v_vars[destID].state = AT_BOTH;
								registers[freereg2] = OCCUPIED;
								calcOP(sEXEmem, freereg, freereg2, operation);
							}
							break;
						} case NOWHERE: reportERROR(L"Invalid variable state ( while calculating )"); break;
					}
				}
			} case NOWHERE: reportERROR(L"Invalid variable state ( while calculating )"); break;
		}
	}
	void new_v_var(const char* name) {
		using namespace JIT;
		if (__findvar(name) != NOT_HERE) reportERROR(L"Repetitive variable name");
		BYTE freereg = __findfreereg();
		strcpy(v_vars[v_var_count].name, name);
		if (freereg == ALL_REGS_FULL) {
			v_vars[v_var_count].REGloc = (BYTE)I_DONT_CARE;
			v_vars[v_var_count].MEMloc = stack_count++;
			v_vars[v_var_count++].state = AT_STACK;
		} else {
			registers[freereg] = OCCUPIED;
			v_vars[v_var_count].REGloc = freereg;
			v_vars[v_var_count].MEMloc = (BYTE)I_DONT_CARE;
			v_vars[v_var_count++].state = AT_REG;
		}
	}
	void finish(const char* ret_name) {
		using namespace JIT;
		DWORD retID = __findvar(ret_name);
		if (retID == NOT_HERE) reportERROR(L"Undefined variable name ( while finishing )");
		else {
			switch (v_vars[retID].state) {
				case AT_REG: case AT_BOTH: case AT_BOTH_ASYNC:
					if (v_vars[retID].REGloc != 0) calcOP(sEXEmem, v_vars[retID].REGloc, 0, COPY); break;
				case AT_STACK: memOP(sEXEmem, 0, v_vars[retID].MEMloc * sizeof(float), LOAD); break;
				case NOWHERE: reportERROR(L"Invalid variable state ( while finishing )"); break;
			}
		}
		stackOP(sEXEmem, RECOVER, stacksize);
		finishOP(sEXEmem);
	}
};

// ### compile #########################################################################################################
BYTE* compile(DWORD vercount, DWORD EXEsize, DWORD constsize, DWORD stacksize) {
	using namespace JIT;
	EXEMEM_STRUCT sEXEmem(EXEsize + constsize);
	stackOP(sEXEmem, ALLOC, stacksize);
	
	memOP(sEXEmem, 0, 0, SAVE);
	memOP(sEXEmem, 1, 4, SAVE);
	calcOP(sEXEmem, 0, 0, MUL);
	calcOP(sEXEmem, 1, 1, MUL);
	calcOP(sEXEmem, 0, 1, ADD);
	calcOP(sEXEmem, 0, 0, SQRT);
	memOP(sEXEmem, 1, 0, LOAD);
	memOP(sEXEmem, 2, 4, LOAD);
	calcOP(sEXEmem, 0, 1, ADD);
	calcOP(sEXEmem, 0, 2, ADD);
	
	DWORD constcount = 0;
	loadconst(sEXEmem, 1, EXEsize, constcount);
	addconst(sEXEmem, 10.f, EXEsize, constcount);
	loadconst(sEXEmem, 2, EXEsize, constcount);
	addconst(sEXEmem,  9.f, EXEsize, constcount);
	calcOP(sEXEmem, 0, 1, MUL);
	calcOP(sEXEmem, 0, 2, ADD);
	
	stackOP(sEXEmem, RECOVER, stacksize);
	finishOP(sEXEmem);
	wprintf(L"Code section:\n"); showHEX(sEXEmem.EXEmem, sEXEmem.count, 16);
	wprintf(L"Const section:\n"); showHEX(sEXEmem.EXEmem + EXEsize, constcount * sizeof(float), 16);
	return sEXEmem.EXEmem;
}

BYTE* compile2(DWORD vercount, DWORD EXEsize, DWORD constsize, DWORD stacksize) {
	using namespace JIT;
	JIT_COMMANDER jit(16, EXEsize, constsize, stacksize);
	jit.new_v_var("val_1");
	jit.new_v_var("val_2");
	jit.calc_v_vars("val_1", "x", COPY);
	jit.calc_v_vars("val_2", "y", COPY);
	jit.calc_v_vars("x", "x", MUL);
	jit.calc_v_vars("y", "y", MUL);
	jit.calc_v_vars("x", "y", ADD);
	jit.calc_v_vars("x", NULL, SQRT);
	jit.calc_v_vars("x", "val_1", ADD);
	jit.calc_v_vars("x", "val_2", ADD);
	jit.set_v_var("const_1", 10.f);
	jit.set_v_var("const_2", 9.f);
	jit.calc_v_vars("x", "const_1", MUL);
	jit.calc_v_vars("x", "const_2", ADD);
	jit.finish("x");
	wprintf(L"(2) Code section:\n"); showHEX(jit.sEXEmem.EXEmem, jit.sEXEmem.count, 16);
	wprintf(L"(2) Const section:\n"); showHEX(jit.sEXEmem.EXEmem + jit.EXEsize, jit.const_count * sizeof(float), 16);
	return jit.sEXEmem.EXEmem;
}

// ### tests ###########################################################################################################
typedef float (*JITfunc)(float, float);
BYTE* compile_test_swap(const char* code, DWORD vercount, DWORD EXEsize, DWORD constsize, DWORD stacksize) {
    using namespace JIT;
    JIT_COMMANDER jit(16, EXEsize, constsize, stacksize);

    // 创建 8 个变量（超过 7 个可用寄存器）
    jit.new_v_var("a");  // 分配 xmm2
    jit.new_v_var("b");  // 分配 xmm3
    jit.new_v_var("c");  // 分配 xmm4
    jit.new_v_var("d");  // 分配 xmm5
    //jit.new_v_var("e");  // 分配 xmm6
    //jit.new_v_var("f");  // 分配 xmm7? 不，xmm7 是保留寄存器，所以这里会触发 forceSWSP！

    // 设置值
    jit.set_v_var("a", 1.f);
    jit.set_v_var("b", 2.f);
    jit.set_v_var("c", 3.f);
    jit.set_v_var("d", 4.f);
    //jit.set_v_var("e", 5.f);
    //jit.set_v_var("f", 6.f);  // 这里应该触发 forceSWSP

    // 做一些计算
    jit.calc_v_vars("a", "b", ADD);  // a = a + b
    jit.calc_v_vars("c", "d", ADD);  // c = c + d
    //jit.calc_v_vars("e", "f", ADD);  // e = e + f

    // 返回结果
    jit.finish("a");  // 返回 a = 1 + 2 = 3

    wprintf(L"(Swap Test 1) Code section:\n");
    showHEX(jit.sEXEmem.EXEmem, jit.sEXEmem.count, 16);
    wprintf(L"(Swap Test 1) Const section:\n");
    showHEX(jit.sEXEmem.EXEmem + jit.EXEsize, jit.const_count * sizeof(float), 16);

    return jit.sEXEmem.EXEmem;
}
BYTE* compile_test_async(const char* code, DWORD vercount, DWORD EXEsize, DWORD constsize, DWORD stacksize) {
    using namespace JIT;
    JIT_COMMANDER jit(16, EXEsize, constsize, stacksize);

    // 创建几个变量
    jit.new_v_var("acc");
    jit.new_v_var("temp");

    // 设置初始值
    jit.set_v_var("acc", 0.f);
    jit.set_v_var("temp", 1.f);

    // 多次使用 acc，使其在 AT_BOTH 和 AT_BOTH_ASYNC 之间切换
    jit.calc_v_vars("acc", "temp", ADD);  // acc = temp + acc
    jit.set_v_var("acc", 5.f);             // 重新设置 acc → 触发 AT_BOTH_ASYNC
    jit.calc_v_vars("acc", "temp", ADD);  // acc = temp + acc → 触发 SWAPsomething

    jit.finish("acc");

    wprintf(L"(Async Test) Code section:\n");
    showHEX(jit.sEXEmem.EXEmem, jit.sEXEmem.count, 16);

    return jit.sEXEmem.EXEmem;
}
BYTE* compile_test_complex(const char* code, DWORD vercount, DWORD EXEsize, DWORD constsize, DWORD stacksize) {
    using namespace JIT;
    JIT_COMMANDER jit(16, EXEsize, constsize, stacksize);

    // 创建大量变量模拟复杂计算
    jit.new_v_var("v1");
    jit.new_v_var("v2");
    jit.new_v_var("v3");
    jit.new_v_var("v4");
    jit.new_v_var("v5");

    // 设置值
    jit.set_v_var("v1", 10.f);
    jit.set_v_var("v2", 20.f);
    jit.set_v_var("v3", 30.f);
    jit.set_v_var("v4", 40.f);
    jit.set_v_var("v5", 50.f);

    // 复杂计算链
    jit.calc_v_vars("v2", "v1", ADD);   // v2 = 10+20 = 30
    jit.calc_v_vars("v4", "v3", ADD);   // v4 = 30+40 = 70
    jit.calc_v_vars("v4", "v2", MUL);   // v4 = 30 * 70 = 2100
    jit.calc_v_vars("v4", "v5", ADD);   // v4 = 50+2100 = 2150
    jit.calc_v_vars("v4", "v1", SUB);   // v1 = 2150-10 = 2140

    jit.finish("v4");  // 返回 2140

    wprintf(L"(Complex Test) Code section:\n");
    showHEX(jit.sEXEmem.EXEmem, jit.sEXEmem.count, 16);

    return jit.sEXEmem.EXEmem;
}

int main() {
    // 原始测试
    BYTE* EXEmem = compile(64, 2048, 128, 512);
    JITfunc func = (JITfunc)EXEmem;
    printf("Original: (sqrt(3 * 3+4 * 4)+3+4)*10+9 = %.2f (expect 129.00)\n\n", func(3.f, 4.f));
    VirtualFree(EXEmem, 0, MEM_RELEASE);

    // compile2 测试
    EXEmem = compile2(64, 2048, 128, 512);
    func = (JITfunc)EXEmem;
    printf("compile2: (sqrt(3 * 3+4 * 4)+3+4)*10+9 = %.2f (expect 129.00)\n\n", func(3.f, 4.f));
    VirtualFree(EXEmem, 0, MEM_RELEASE);

    // Swap 测试 1：大量变量
    EXEmem = compile_test_swap("", 64, 2048, 128, 512);
    func = (JITfunc)EXEmem;
    printf("Swap Test 1 (many vars): result = %.2f (expect 3.00)\n\n", func(0.f, 0.f));
    VirtualFree(EXEmem, 0, MEM_RELEASE);

    // Swap 测试 2：异步状态
    EXEmem = compile_test_async("", 64, 2048, 128, 512);
    func = (JITfunc)EXEmem;
    printf("Async Test: result = %.2f (expect 6.00)\n\n", func(0.f, 0.f));
    VirtualFree(EXEmem, 0, MEM_RELEASE);

    // Swap 测试 3：复杂表达式
    EXEmem = compile_test_complex("", 64, 2048, 128, 512);
    func = (JITfunc)EXEmem;
    printf("Complex Test: result = %.2f (expect 2140.00)\n\n", func(0.f, 0.f));
    VirtualFree(EXEmem, 0, MEM_RELEASE);

    return 0;
}
