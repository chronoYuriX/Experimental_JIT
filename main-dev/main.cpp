#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <stdint.h>

#define __JIT_DEBUG
#include "jit.h"
HANDLE __g_hheap;

namespace CALC {
	const UINT_PTR TIMER_RESIZE_BUFFER = 1;
	const UINT TIMER_RESIZE_BUFFER_DELAY = 10000;
	const float BUFFER_OVERSIZE = 1.5f;
}
#include <stdio.h>
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
	//JITinit();
	__g_hheap = GetProcessHeap();
	DWORD pid = GetCurrentProcessId(), tick = GetTickCount();
    srand(pid ^ tick);
    CALC_WINDOW calc(400, 300);
    calc.run();
    return 0;
}
