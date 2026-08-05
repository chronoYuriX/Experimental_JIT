#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>

//#define __JIT_DEBUG
//#include "jit.h"
HANDLE __g_hheap = GetProcessHeap();
typedef float (*func2F_F)(float, float);
float ftest(float x, float y) {
	return y * sinf(x) - x * cosf(y) - 1.f;
	//return -x/2.f - y;
}
#include "cyxlib.h"


struct BGRA { BYTE b; BYTE g; BYTE r; BYTE a; };
struct POINTF { float x, y; };
struct LINEF { POINTF S, E; };
inline void dot(POINT pos, BGRA color, BYTE* bmp, int32_t bmp_x, RECT range) {
	if (range.left <= pos.x && pos.x < range.right && range.top <= pos.y && pos.y < range.bottom)
		((BGRA*)bmp)[pos.y * bmp_x + pos.x] = color;
}
/*
void straight(POINT p_1, POINT p_2, BGRA color, BYTE* bmp, int32_t bmp_x, RECT range) {
	POINT delta = { abs(p_2.x - p_1.x), -abs(p_2.y - p_1.y) },
		op = { (p_1.x < p_2.x) ? 1 : -1, (p_1.y < p_2.y) ? 1 : -1 };
	int32_t err = delta.x + delta.y, steps = 0;
    while (steps++ < 100) {
        dot(p_1, color, bmp, bmp_x, range);
        if (((p_1.x ^ p_2.x) | (p_1.y ^ p_2.y)) == 0) return;
        if ((err << 1) > delta.y) { err += delta.y; p_1.x += op.x; }
		if ((err << 1) < delta.x) { err += delta.x; p_1.y += op.y; }
    }
	printf("p_1(%d, %d), p_2(%d, %d)\n", p_1.x, p_1.y, p_2.x, p_2.y);
}*/
void straight(POINT p_1, POINT p_2, BGRA color, BYTE* bmp, int32_t bmp_x, RECT range) {
    int32_t dx = abs(p_2.x - p_1.x), sx = p_1.x < p_2.x ? 1 : -1,
		dy = -abs(p_2.y - p_1.y), sy = p_1.y < p_2.y ? 1 : -1,
    	err = dx + dy, e2;
    while (1) {
        dot(p_1, color, bmp, bmp_x, range);
        if (p_1.x == p_2.x && p_1.y == p_2.y) break;
        e2 = err << 1;
        if (e2 >= dy) { err += dy; p_1.x += sx; }
        if (e2 <= dx) { err += dx; p_1.y += sy; }
    }
}

struct RENDER_FUNC_INFO {
	func2F_F func;
	BYTE*    bmp;
	int32_t  bmp_x;
	POINT    offset;
	BGRA     color;
	RECT     range;
	float    magnify;
	BYTE     max_depth;
};
inline LINEF march_map(BIT_FIELD key, bool* found) {
	*found = 1;
	switch (key.field) {
		case _0b("1100"): case _0b("0011"): return { { 0.0f, 0.5f }, { 1.0f, 0.5f } };
		case _0b("1010"): case _0b("0101"): return { { 0.5f, 0.0f }, { 0.5f, 1.0f } };
		// case _0b("1000"): case _0b("0111"): return { { 0.0f, 0.5f }, { 0.5f, 0.0f } };
		// case _0b("1011"): case _0b("0100"): return { { 0.5f, 0.0f }, { 1.0f, 0.5f } };
		case _0b("1000"): case _0b("0111"): return { { 0.5f, 1.0f }, { 1.0f, 0.5f } };
		case _0b("1011"): case _0b("0100"): return { { 0.0f, 0.5f }, { 0.5f, 1.0f } };
		// case _0b("1101"): case _0b("0010"): return { { 0.0f, 0.5f }, { 0.5f, 1.0f } };
		// case _0b("1110"): case _0b("0001"): return { { 0.5f, 1.0f }, { 1.0f, 0.5f } };
		case _0b("1101"): case _0b("0010"): return { { 0.5f, 0.0f }, { 1.0f, 0.5f } };
		case _0b("1110"): case _0b("0001"): return { { 0.0f, 0.5f }, { 0.5f, 0.0f } };
	}
	*found = 0;
	return { 0 };
}
void march_9_grid(RENDER_FUNC_INFO* info, POINTF start, POINTF end, float step, BIT_FIELD _4_vals, BYTE depth) {
	POINTF mid = { (start.x + end.x) * .5f, (start.y + end.y) * .5f };
	bool found;
	LINEF march = march_map(_4_vals, &found);
	if (!found) return;
	if (depth == info->max_depth) {
		POINT p_1 = {
			int32_t((start.x + march.S.x * step) * info->magnify) + info->offset.x,
			int32_t((start.y + march.S.y * step) * info->magnify) + info->offset.y
		}, p_2 = {
			int32_t((start.x + march.E.x * step) * info->magnify) + info->offset.x,
			int32_t((start.y + march.E.y * step) * info->magnify) + info->offset.y
		};
		straight(p_1, p_2, info->color, info->bmp, info->bmp_x, info->range);
	} else {
		func2F_F func = info->func;
		bool _5_vals[5] = { /* [0] */   func(mid.x, start.y) > 0.f, /* [1] */
			func(start.x, mid.y) > 0.f, func(mid.x,   mid.y) > 0.f, func(end.x, mid.y) > 0.f,
			                /* [2] */   func(mid.x,   end.y) > 0.f  /* [3] */ };
		depth++;               //                        0  [0]  1
		step *= .5f;           //                       [1] [2] [3]
		BIT_FIELD _4_vals_new; //                        2  [4]  3
		_4_vals_new.set4(_4_vals.get_strict(0), _5_vals[0], _5_vals[1], _5_vals[2]); // Corner-0
		march_9_grid(info, start, mid, step, _4_vals_new, depth);
		_4_vals_new.set4(_5_vals[2], _5_vals[3], _5_vals[4], _4_vals.get_strict(3)); // Corner-3
		march_9_grid(info,   mid, end, step, _4_vals_new, depth);
		_4_vals_new.set4(_5_vals[0], _4_vals.get_strict(1), _5_vals[2], _5_vals[3]); // Corner-1
		march_9_grid(info, {   mid.x, start.y }, { end.x, mid.y }, step, _4_vals_new, depth);
		_4_vals_new.set4(_5_vals[1], _5_vals[2], _4_vals.get_strict(2), _5_vals[4]); // Corner-2
		march_9_grid(info, { start.x,   mid.y }, { mid.x, end.y }, step, _4_vals_new, depth);
	}
}

void render_func(RENDER_FUNC_INFO* info, POINTF start, POINTF end, float step) {
	bool val_map[256][256];
	float y = start.y;
	int y_count = 0, x_count;
	while (y <= end.y) {
		float x = start.x;
		x_count = 0;
		while (x <= end.x) {
			val_map[y_count][x_count++] = (info->func(x, y) > 0);
			x += step;
		}
		y_count++;
		y += step;
	}
	float current_y = start.y;
	for (int i = 0; i < y_count - 1; i++) {
		float current_x = start.x;
		for (int j = 0; j < x_count - 1; j++) {
			BIT_FIELD _4_vals;
			_4_vals.set4(val_map[i][j], val_map[i][j+1], val_map[i+1][j], val_map[i+1][j+1]);
			bool found;
			LINEF march = march_map(_4_vals, &found);
			/*
			{
				POINT p = {
					int32_t(current_x * info->magnify) + info->offset.x,
					int32_t(current_y * info->magnify) + info->offset.y
				};
				if (info->func(current_x, current_y) > 0)
					dot(p, BGRA{0, 0, 255}, info->bmp, info->bmp_x, info->range);
				else
					dot(p, BGRA{255, 0, 0}, info->bmp, info->bmp_x, info->range);
			}
			if (found) {
				POINT p_1 = {
					int32_t((current_x + march.S.x * step) * info->magnify) + info->offset.x,
					int32_t((current_y + march.S.y * step) * info->magnify) + info->offset.y
				}, p_2 = {
					int32_t((current_x + march.E.x * step) * info->magnify) + info->offset.x,
					int32_t((current_y + march.E.y * step) * info->magnify) + info->offset.y
				};
				straight(p_1, p_2, info->color, info->bmp, info->bmp_x, info->range);
			}
			*/
			march_9_grid(info, {current_x, current_y}, {current_x + step, current_y + step}, step, _4_vals, 0);
			current_x += step;
		}
		current_y += step;
	}
}

namespace CALC {
	const UINT_PTR TIMER_RESIZE_BUFFER = 1;
	const UINT TIMER_RESIZE_BUFFER_DELAY = 10000;
	const float BUFFER_OVERSIZE = 1.5f;
}
struct CALC_WINDOW {
	HWND        window_hwnd;
	int32_t     window_size_x, window_size_y;
	BYTE*       window_buffer;
	SIZE_T      window_buffer_xy;
	HDC         window_hDC, window_hmemDC;
	HBITMAP     window_hBMP;
	BITMAPINFO  window_BMI;
	PAINTSTRUCT window_PS;
	CALC_WINDOW(int32_t __window_size_x, int32_t __window_size_y): window_hwnd(NULL), window_buffer(NULL),
			window_size_x(__window_size_x), window_size_y(__window_size_y), window_buffer_xy(0) {
		ZeroMemory(&window_BMI, sizeof(BITMAPINFO));
    	window_BMI.bmiHeader.biSize   = sizeof(BITMAPINFOHEADER);
    	window_BMI.bmiHeader.biWidth  =  __window_size_x; window_BMI.bmiHeader.biHeight   = -__window_size_y;
		window_BMI.bmiHeader.biPlanes = 1;                window_BMI.bmiHeader.biBitCount = 32;
    	window_BMI.bmiHeader.biCompression = BI_RGB;
    	window_BMI.bmiHeader.biSizeImage   = window_size_x * window_size_y * 4;
	}
	static LRESULT CALLBACK __calc_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
		CALC_WINDOW* pthis = NULL;
        if (msg == WM_NCCREATE) {
            pthis = (CALC_WINDOW*)(((CREATESTRUCT*)lparam)->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)pthis);
        } else pthis = (CALC_WINDOW*)(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (pthis != NULL) return pthis->main_process(hwnd, msg, wparam, lparam);
        return DefWindowProcW(hwnd, msg, wparam, lparam);
    }
	LRESULT CALLBACK main_process(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	    switch (msg) {
    		case WM_CREATE: {
	            window_hDC = GetDC(hwnd);
            	window_hmemDC = CreateCompatibleDC(window_hDC);
            	window_hBMP = CreateCompatibleBitmap(window_hDC, window_size_x, window_size_y);
            	SelectObject(window_hmemDC, window_hBMP);
            	ReleaseDC(hwnd, window_hDC);
            	SetTimer(hwnd, CALC::TIMER_RESIZE_BUFFER, CALC::TIMER_RESIZE_BUFFER_DELAY, NULL);
            	break;
        	} case WM_PAINT: {
            	window_hDC = BeginPaint(hwnd, &window_PS);
            	SetDIBitsToDevice(
                	window_hmemDC, 0, 0, window_size_x, window_size_y,
					0, 0, 0, window_size_y, window_buffer, &window_BMI, DIB_RGB_COLORS);
            	BitBlt(window_hDC, 0, 0, window_size_x, window_size_y, window_hmemDC, 0, 0, SRCCOPY);
            	EndPaint(hwnd, &window_PS);
            	break;
        	} case WM_SIZE: {
        		window_size_x = (int32_t)LOWORD(lparam), window_size_y = (int32_t)HIWORD(lparam);
        		if ((SIZE_T)window_size_x * window_size_y > window_buffer_xy) {
					do window_buffer_xy = SIZE_T(window_buffer_xy * CALC::BUFFER_OVERSIZE);
					while (window_buffer_xy < (SIZE_T)window_size_x * window_size_y);
        			window_buffer = (BYTE*)HeapReAlloc(__g_hheap, 0, window_buffer, window_buffer_xy * 4);
				}
				break;
			} case WM_TIMER: {
				if (wparam == CALC::TIMER_RESIZE_BUFFER) {
					SIZE_T window_buffer_xy_current = (SIZE_T)window_size_x * window_size_y;
					if (window_buffer_xy_current == window_buffer_xy) break;
					window_buffer_xy = window_buffer_xy_current;
					if (window_buffer_xy == 0) window_buffer = (BYTE*)HeapReAlloc(__g_hheap, 0, window_buffer, 4);
					else window_buffer = (BYTE*)HeapReAlloc(__g_hheap, 0, window_buffer, window_buffer_xy * 4);
				}
				break;
			} case WM_CLOSE: DestroyWindow(hwnd); break;
    		case WM_DESTROY: {
            	if (window_hmemDC) {
                	SelectObject(window_hmemDC, window_hBMP); DeleteDC(window_hmemDC); window_hmemDC = NULL;
            	} if (window_hBMP) { DeleteObject(window_hBMP); window_hBMP = NULL; }
            	KillTimer(hwnd, CALC::TIMER_RESIZE_BUFFER);
            	PostQuitMessage(0);
				break;
			} case WM_ERASEBKGND: return 1;
    		default: return DefWindowProcW(hwnd, msg, wparam, lparam);
		}
    	return 0;
	}
	void __randname(wchar_t* dest, const wchar_t* header, WORD namelen) {
		wcscpy(dest, header);
		wchar_t tmpchr;
		for (WORD i = wcslen(header); i < namelen; i++) {
	    	tmpchr = rand() % 62;
        	if (tmpchr < 26) tmpchr += L'A';
        	else if (tmpchr < 52) tmpchr += L'a' - 26;
        	else tmpchr += L'0' - 52;
        	dest[i] = tmpchr;
    	}
    	dest[namelen] = L'\0';
	}
	POINT __locate() {
		HWND taskbar = FindWindowW(L"Shell_TrayWnd", NULL);
		RECT display_zone = { 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
    	if (taskbar != NULL) {
    		RECT taskbar_zone;
    		GetWindowRect(taskbar, &taskbar_zone);
    		int32_t taskbar_size_x = taskbar_zone.right - taskbar_zone.left,
				taskbar_size_y = taskbar_zone.bottom - taskbar_zone.top;
			if (taskbar_size_x > taskbar_size_y) {
				if (taskbar_size_y < taskbar_zone.top) display_zone.bottom -= taskbar_size_y; // @bottom
				else display_zone.top += taskbar_size_y; // @top
			} else if (taskbar_size_x < taskbar_zone.left) taskbar_zone.right -= taskbar_size_x; // @right
			else display_zone.left += taskbar_size_x; // @left
		}
		POINT mouse, window_loc;
		GetCursorPos(&mouse);
		if (mouse.x + (window_size_x >> 1) > display_zone.right) window_loc.x = display_zone.right - window_size_x;
		else if (mouse.x < (window_size_x >> 1) + display_zone.left) window_loc.x = display_zone.left;
		else window_loc.x = mouse.x - (window_size_x >> 1);
		if (mouse.y + (window_size_y >> 1) > display_zone.bottom) window_loc.y = display_zone.bottom - window_size_y;
		else if (mouse.y < (window_size_y >> 1) + display_zone.top) window_loc.y = display_zone.top;
		else window_loc.y = mouse.y - (window_size_y >> 1);
		return window_loc;
	}
	DWORD run() {
		window_buffer_xy = (SIZE_T)window_size_x * window_size_y;
		window_buffer = (BYTE*)HeapAlloc(__g_hheap, 0, window_buffer_xy * 4);
		ZeroMemory(window_buffer, window_buffer_xy * 4);
		{
			/*
			straight(POINT{ 10, 10 }, POINT{ 30, 20 }, POINT{ 0, 0}, BGRA{ 0, 255, 0 },
				window_buffer, window_size_x, RECT{ 0, 0, window_size_x, window_size_y });
			*/

			RENDER_FUNC_INFO info;
			info.bmp = window_buffer;
			info.bmp_x = window_size_x;
			info.color = BGRA{ 0, 255, 0 };
			info.magnify = 40.f;
			info.max_depth = 2;
			info.range = RECT{ 0, 0, window_size_x, window_size_y };
			info.offset = POINT{ window_size_x >> 1, window_size_y >> 1};
			/*
			EXPR_COMPILER_INFO compiler_info = {
				.max_tokens = 256,
				.max_vars = 64,
				.max_consts = 16,
				.max_funcs = 32,
				.code_size = 256,
				.stack_size = 256,
				.external_env = NULL
			};
			EXPR_COMPILER compiler(&compiler_info);
			BYTE result = compiler.compile_expr("sin(x) - y");
			*/
			//if (result == JIT::PASS) {
				//info.func = compiler.main_executable;
				info.func = ftest;
				render_func(&info, { -15.f, -10.f }, { 15.f, 10.f }, .5f);
			//} else wprintf(L"Error: %ls\n", __g_error_str);
		}
		wchar_t randname[256];
    	__randname(randname, L"CYX_CALC_", 255);
    	WNDCLASSW wc = { 0 };
    	wc.lpfnWndProc   = __calc_proc;
    	wc.hInstance     = GetModuleHandle(NULL);
    	wc.lpszClassName = randname;
    	RegisterClassW(&wc);
    	POINT window_loc = __locate();
    	wchar_t window_name[] = L"Calc";
    	window_hwnd = CreateWindowExW(
        	0, randname, window_name, WS_OVERLAPPEDWINDOW, window_loc.x, window_loc.y, window_size_x, window_size_y,
			NULL, NULL, GetModuleHandle(NULL), this);
		window_hDC = GetDC(window_hwnd);
    	ShowWindow(window_hwnd, SW_SHOW);
    	MSG msg;
    	while (GetMessage(&msg, NULL, 0, 0)) {
        	TranslateMessage(&msg);
        	DispatchMessage (&msg);
    	}
    	HeapFree(__g_hheap, 0, window_buffer);
    	return msg.wParam;
	}
};


int main() {
	DWORD pid = GetCurrentProcessId(), tick = GetTickCount();
    srand(pid ^ tick);
    CALC_WINDOW calc(1000, 600);
    calc.run();
    return 0;
}
