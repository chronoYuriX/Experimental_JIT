#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <stdio.h>

// add(int a, int b) => return a + b
unsigned char code_1[] = {
    0x48, 0x89, 0xC8,       // mov rax, rcx      ; rax = a
    0x48, 0x01, 0xD0,       // add rax, rdx      ; rax += b
    0xC3                    // ret
};
typedef int (*AddFunc)(int, int);

// circle(float x, float y) => return x * x + y * y
unsigned char code_2[] = {
    0xF3, 0x0F, 0x59, 0xC0,   // mulss xmm0, xmm0    ; xmm0 = x*x
    0xF3, 0x0F, 0x59, 0xC9,   // mulss xmm1, xmm1    ; xmm1 = y*y
    0xF3, 0x0F, 0x58, 0xC1,   // addss xmm0, xmm1    ; xmm0 = x*x + y*y
    0xC3                      // ret
};
typedef float (*CircleFunc)(float, float);

// LOL(float n) => x = 0 + 1 + 2 + ... until x >= n, then return x
unsigned char code_3[] = {
	// === 初始化 ===
    0x0F, 0x57, 0xC9,             // xorps  xmm1, xmm1      ; xmm1 = 0.0f (x)
    0x0F, 0x57, 0xD2,             // xorps  xmm2, xmm2      ; xmm2 = 0.0f
    0xB8, 0x00, 0x00, 0x80, 0x3F, // mov eax, 0x3F800000 ; 1.0f 的 IEEE 754
    0x66, 0x0F, 0x6E, 0xD0,       // movd   xmm2, eax       ; xmm2 = 1.0f (step)
    // === 循环开始（偏移 0x0D）===
    // loop_start:
    0x0F, 0x2E, 0xC8,             // ucomiss xmm1, xmm0     ; 比较 x 和 n
    0x73, 0x0E,                   // jae     done            ; x >= n → 跳出
    // x += step
    0xF3, 0x0F, 0x58, 0xCA,       // addss   xmm1, xmm2     ; x += step
    // step += 1.0f（从内存加载常量）
    0xF3, 0x0F, 0x58, 0x15,       // addss   xmm2, [rip+offset]
    0x08, 0x00, 0x00, 0x00,       // RIP 相对偏移 = 8（跳过下面两条指令到达常量区）
    // 跳回循环开始
    0xEB, 0xED,                   // jmp     loop_start      ; 相对偏移 -19 字节
    // === done: ===
    0xF3, 0x0F, 0x11, 0xC8,       // movss   xmm0, xmm1     ; 结果移到 xmm0
    0xC3,                         // ret
    // === 常量区 ===
    0x00, 0x00, 0x80, 0x3F        // float 1.0f（小端序）
};
typedef float (*LOLFunc)(float);

int main() {
    void *exec_mem = VirtualAlloc(NULL, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    
	memcpy(exec_mem, code_1, sizeof(code_1));
    AddFunc add = (AddFunc)exec_mem;
    printf("3 + 5 = %d\n", add(3, 5));
    
    memcpy(exec_mem, code_2, sizeof(code_2));
    CircleFunc circle = (CircleFunc)exec_mem;
	printf("3 * 3 + 4 * 4 = %.2f\n", circle(3.f, 4.f));
	
	memcpy(exec_mem, code_3, sizeof(code_3));
    LOLFunc LOL = (LOLFunc)exec_mem;
	printf("LOL %.2f\n", LOL(16.f));
    
    VirtualFree(exec_mem, 0, MEM_RELEASE);
    return 0;
}
