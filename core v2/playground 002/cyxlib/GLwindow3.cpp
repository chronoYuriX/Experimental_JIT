#ifndef _WINDOWS_
	#define WIN32_LEAD_AND_MEAN
	#define NOMINMAX
    #define UNICODE
	#include <windows.h>
#endif
#include <gl/gl.h>
#include <stdint.h>
#include <stdio.h>

HANDLE _g_hheap = GetProcessHeap();

int SaveBMP32(const char* path, HBITMAP hBMP, BYTE* data = NULL) {
    BITMAP BMP;
    if (!GetObject(hBMP, sizeof(BMP), &BMP)) return -1;
    BITMAPFILEHEADER BMP_file_header = { 0 };
    BITMAPINFOHEADER BMP_info_header = { 0 };
    BMP_info_header.biSize = sizeof(BITMAPINFOHEADER);
	BMP_info_header.biWidth = BMP.bmWidth;
	BMP_info_header.biHeight = BMP.bmHeight;
    BMP_info_header.biPlanes = 1;
	BMP_info_header.biBitCount = BMP.bmBitsPixel;
	BMP_info_header.biCompression = BI_RGB;
    int32_t stride = ((BMP.bmWidth * BMP.bmBitsPixel + 31) / 32) * 4;
    int32_t data_size = stride * BMP.bmHeight;
    BMP_file_header.bfType = 0x4D42; // "BM"
    BMP_file_header.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + data_size;
    BMP_file_header.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    FILE* file = fopen(path, "wb");
    if (!file) return -2;
    fwrite(&BMP_file_header, sizeof(BITMAPFILEHEADER), 1, file);
    fwrite(&BMP_info_header, sizeof(BITMAPINFOHEADER), 1, file);
    BYTE* bits = (BYTE*)malloc(data_size);
    if (bits) {
        HDC hDC = CreateCompatibleDC(0);
        SelectObject(hDC, hBMP);
        GetDIBits(hDC, hBMP, 0, BMP.bmHeight, bits, (BITMAPINFO*)&BMP_info_header, DIB_RGB_COLORS);
        DeleteDC(hDC);
        if (data == NULL) fwrite(bits, 1, data_size, file);
        else fwrite(data, 1, data_size, file);
        free(bits);
    }
    fclose(file);
    return 0;
}

void init_rand() {
	DWORD PID = GetCurrentProcessId(), tick = GetTickCount();
    srand(PID ^ tick);
}

struct GL_CHAR {
	wchar_t* character;
};
struct GL_TEXT {
	wchar_t* text;
	GLuint textureID;
	SIZE text_size, texutre_size;
	static BITMAPINFO _create_BMI(SIZE BMPsize, WORD BMPbitcount) {
		BITMAPINFO BMI = { 0 };
    	BMI.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    	BMI.bmiHeader.biWidth       =  BMPsize.cx;
   		BMI.bmiHeader.biHeight      = -BMPsize.cy;
   		BMI.bmiHeader.biPlanes      = 1;
    	BMI.bmiHeader.biBitCount    = BMPbitcount;
    	BMI.bmiHeader.biCompression = BI_RGB;
    	BMI.bmiHeader.biSizeImage   = BMPsize.cx * BMPsize.cy * 4;
    	return BMI;
	}
	static inline int32_t _ceiling_pow2(int32_t n) {
		int32_t ceiling = 1;
		while (ceiling < n) ceiling <<= 1;
		return ceiling;
	}
	GL_TEXT(const wchar_t* _text, HDC htextDC) {
		HANDLE process_heap = GetProcessHeap();
		int32_t text_length = wcslen(_text);
		text = (wchar_t*)HeapAlloc(process_heap, 0, (text_length + 1) * sizeof(wchar_t));
		wcscpy(text, _text);

		GetTextExtentPoint32W(htextDC, text, text_length, &text_size);
		// printf("Size: %d x %d", text_size.cx, text_size.cy);
		text_size.cx = _ceiling_pow2(text_size.cx), text_size.cy = _ceiling_pow2(text_size.cy);
		// printf(" -> %d x %d\n", text_size.cx, text_size.cy);

		RECT text_range = RECT{ 0, 0, text_size.cx, text_size.cy };
		BITMAPINFO BMI = _create_BMI(text_size, 32);
		BYTE* text_data = NULL;
		HBITMAP htextBMP = CreateDIBSection(htextDC, &BMI, DIB_RGB_COLORS, (void**)&text_data, NULL, 0);
		/*
		for (int y = 0; y < text_size.cy; y++) {
        	for (int x = 0; x < text_size.cx; x++) {
        		unsigned long long offset = ((unsigned long long)y * text_size.cx + x) * 4;
        	    text_data[offset] = 128;
        	    text_data[offset + 1] = (unsigned char)(y * 16);
        	    text_data[offset + 2] = (unsigned char)(x * 8);
        	    text_data[offset + 3] = 255;
        	}
    	}
		*/
		SelectObject(htextDC, htextBMP);
		ExtTextOutW(htextDC, 0, 0, ETO_CLIPPED, &text_range, text, text_length, NULL);
		for (int y = 0; y < text_size.cy; y++) {
        	for (int x = 0; x < text_size.cx; x++) {
        		unsigned long long offset = ((unsigned long long)y * text_size.cx + x) * 4;
        	    text_data[offset + 3] = 255;
        	}
    	}
    	/*
		SaveBMP32("debug_out_3.bmp", htextBMP);
		SaveBMP32("debug_out_4.bmp", htextBMP, text_data);
		//system("debug_out.bmp");
		*/
    	glGenTextures(1, &textureID);
    	glBindTexture(GL_TEXTURE_2D, textureID);
    	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, text_size.cx, text_size.cy, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, text_data);
    	DeleteObject(htextBMP);
	}
	~GL_TEXT() { glDeleteTextures(1, &textureID); }
	void draw(POINT loc) {
		if (glIsTexture(textureID)) glBindTexture(GL_TEXTURE_2D, textureID);
		glBegin(GL_QUADS);
        	glTexCoord2f(0.0f, 0.0f); glVertex2i(loc.x, loc.y);
        	glTexCoord2f(1.0f, 0.0f); glVertex2i(loc.x + text_size.cx, loc.y);
        	glTexCoord2f(1.0f, 1.0f); glVertex2i(loc.x + text_size.cx, loc.y + text_size.cy);
        	glTexCoord2f(0.0f, 1.0f); glVertex2i(loc.x, loc.y + text_size.cy);
    	glEnd();
	}
};

struct GL_WINDOW {
	HDC      hDC, htextDC;
	HWND     hwnd;
	HGLRC    hRC;
	HFONT    hfont;
	int32_t  window_size_x, window_size_y;
	uint8_t  current_dimension;
	static constexpr uint16_t WINDOW_CLASSNAME_LEN = 256;
	static constexpr wchar_t WINDOW_CLASSNAME_HEADER[] = L"CYX_GL_WINDOW_", WINDOW_TITLE[] = L"OpenGL Demo",
		TEXT_DEFAULT[] = L"Consolas";
	static constexpr double GL_VISION_NEAR = 1., GL_VISION_FAR = 100.;
	static constexpr COLORREF FONT_DEFAULT_COLOR = RGB(255, 255, 255);
	static constexpr PIXELFORMATDESCRIPTOR _create_PFD() {
		PIXELFORMATDESCRIPTOR PFD = { 0 };
    	PFD.nSize = sizeof(PFD);
    	PFD.nVersion = 1;
    	PFD.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    	PFD.iPixelType = PFD_TYPE_RGBA;
    	PFD.cColorBits = 24;
    	PFD.cDepthBits = 24;
    	PFD.iLayerType = PFD_MAIN_PLANE;
    	return PFD;
	}
	static constexpr LOGFONTW _create_default_logfont() {
		LOGFONTW LF = { 0 };
		LF.lfHeight = -10;
		LF.lfCharSet = DEFAULT_CHARSET;
		__builtin_memcpy(LF.lfFaceName, TEXT_DEFAULT, sizeof(TEXT_DEFAULT));
		return LF;
	}
	LRESULT CALLBACK _window_proc_main(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    	switch (message) {
        	case WM_CREATE: break;
        	case WM_CLOSE: DestroyWindow(hwnd); break;
        	case WM_DESTROY: PostQuitMessage(0); break;
        	case WM_SIZE: {
        		if (wparam == SIZE_RESTORED || wparam == SIZE_MAXIMIZED || wparam == SIZE_MINIMIZED)
					resize(LOWORD(lparam), HIWORD(lparam), 0);
				break;
			}
        	default: return DefWindowProcW(hwnd, message, wparam, lparam);
    	}
    	return 0;
	}
	static LRESULT CALLBACK _window_proc_wrapper(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
		GL_WINDOW* pthis = NULL;
        if (msg == WM_NCCREATE) {
            pthis = (GL_WINDOW*)(((CREATESTRUCT*)lparam)->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)pthis);
        } else pthis = (GL_WINDOW*)(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (pthis != NULL) return pthis->_window_proc_main(hwnd, msg, wparam, lparam);
        return DefWindowProcW(hwnd, msg, wparam, lparam);
    }
	static WNDCLASSW _create_wndclass_template() {
		WNDCLASSW wndclass = { 0 };
    	wndclass.style       = CS_OWNDC;
    	wndclass.lpfnWndProc = _window_proc_wrapper;
    	wndclass.hInstance   = GetModuleHandleW(NULL);
    	wndclass.hIcon       = LoadIcon(NULL, IDI_APPLICATION);
    	wndclass.hCursor     = LoadCursor(NULL, IDC_ARROW);
		return wndclass;
	}
	static void _create_rand_classname(wchar_t* dest, const wchar_t* header, uint16_t namelen) {
		wcscpy(dest, header);
		for (uint16_t i = wcslen(header); i < namelen; i++) {
	    	wchar_t current_char = rand() % 62;
        	if (current_char < 26) current_char += L'A';
        	else if (current_char < 52) current_char += L'a' - 26;
        	else current_char += L'0' - 52;
        	dest[i] = current_char;
    	}
    	dest[namelen] = L'\0';
	}
	POINT _locate_window() {
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
	void _create_window() {
		WNDCLASSW wndclass = _create_wndclass_template();
		wchar_t window_classname[WINDOW_CLASSNAME_LEN];
		_create_rand_classname(window_classname, WINDOW_CLASSNAME_HEADER, WINDOW_CLASSNAME_LEN - 1);
    	wndclass.lpszClassName = window_classname;
    	RegisterClassW(&wndclass);
		POINT window_loc = _locate_window();
    	hwnd = CreateWindowExW(0, window_classname, WINDOW_TITLE,
			WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX,
			window_loc.x, window_loc.y, window_size_x, window_size_y,
			NULL, NULL, GetModuleHandleW(NULL), this);
		hDC = GetDC(hwnd);
    	ShowWindow(hwnd, SW_SHOW);
	}
	void _init_font() {
		LOGFONT LF = _create_default_logfont();
    	hfont = CreateFontIndirectW(&LF);
    	htextDC = CreateCompatibleDC(NULL);
    	SelectObject(htextDC, hfont);
    	SetTextColor(htextDC, FONT_DEFAULT_COLOR);
    	SetBkMode(htextDC, TRANSPARENT);
	}
	void _recover_font() {
		DeleteObject(hfont);
		ReleaseDC(NULL, htextDC);
	}
	GL_WINDOW(int32_t _size_x, int32_t _size_y): window_size_x(_size_x), window_size_y(_size_y), current_dimension(3) {
		_create_window();
		PIXELFORMATDESCRIPTOR PFD = _create_PFD();
		int32_t formatID = ChoosePixelFormat(hDC, &PFD);
    	SetPixelFormat(hDC, formatID, &PFD);
    	hRC = wglCreateContext(hDC);
    	wglMakeCurrent(hDC, hRC);
		glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS);
		glEnable(GL_TEXTURE_2D); glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    	glMatrixMode(GL_PROJECTION);
    	glLoadIdentity();
    	double aspect = (double)_size_x / (double)_size_y;
    	glFrustum(-aspect, aspect, -1., 1., GL_VISION_NEAR, GL_VISION_FAR);
    	glMatrixMode(GL_MODELVIEW);
    	glLoadIdentity();
    	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    	_init_font();
	}
	~GL_WINDOW() {
		wglMakeCurrent(NULL, NULL);
    	wglDeleteContext(hRC);
    	_recover_font();
	}
	void resize(int32_t _size_x, int32_t _size_y, bool isproactive) {
		if (isproactive) SetWindowPos(hwnd, NULL, 0, 0, _size_x, _size_y, SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOZORDER);
		glViewport(0, 0, _size_x, _size_y);
    	glMatrixMode(GL_PROJECTION);
    	glLoadIdentity();
    	double aspect = (double)_size_x / (double)_size_y;
    	glFrustum(-aspect, aspect, -1., 1., GL_VISION_NEAR, GL_VISION_FAR);
    	glMatrixMode(GL_MODELVIEW);
		window_size_x = _size_x; window_size_y = _size_y;
	}
	void switch_dimension(uint8_t dimension) {
		if (dimension == current_dimension) return;
		if (dimension == 3) {
			glEnable(GL_DEPTH_TEST);
			glDisable(GL_LINE_SMOOTH);
        	glDisable(GL_BLEND);
    		glMatrixMode(GL_PROJECTION);
    		glLoadIdentity();
    		double aspect = (double)window_size_x / (double)window_size_y;
    		glFrustum(-aspect, aspect, -1., 1., GL_VISION_NEAR, GL_VISION_FAR);
		} else if (dimension == 2) {
			glDisable(GL_DEPTH_TEST);
			glEnable(GL_LINE_SMOOTH); glHint(GL_LINE_SMOOTH_HINT, GL_FASTEST);
			glEnable(GL_BLEND);       glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    		glMatrixMode(GL_PROJECTION);
    		glLoadIdentity();
    		glOrtho(0., (double)window_size_x, (double)window_size_y, 0., -1., 1.);
		} else return;
		glMatrixMode(GL_MODELVIEW);
    	glLoadIdentity();
    	current_dimension = dimension;
	}
	DWORD WINAPI mainloop() {
		MSG msg;
		/*
		while (GetMessageW(&msg, NULL, 0, 0)) {
        	TranslateMessage(&msg);
        	DispatchMessageW(&msg);
    	}
		*/
		float theta = 0.f;
		int cnt = 0;
		timeBeginPeriod(1);
		GL_TEXT* text;
		while (1) {
        	if (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            	if (msg.message == WM_QUIT) break;
            	else {
            	    TranslateMessage(&msg);
        	    	DispatchMessageW(&msg);
        	    }
        	} else {
        	    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        	    switch_dimension(3);
        	    glTranslatef(0.f, 0.f, -8.f);
        	    glRotatef(theta, .3f, 1.f, 0.f);
        	    glBegin(GL_QUADS);
					glColor3f(1.f, 0.f, 0.f); glVertex3f(-1.f,  1.f, 0.f);
					glColor3f(0.f, 1.f, 0.f); glVertex3f( 1.f,  1.f, 0.f);
					glColor3f(0.f, 0.f, 1.f); glVertex3f( 1.f, -1.f, 0.f);
					glColor3f(0.f, 1.f, 0.f); glVertex3f(-1.f, -1.f, 0.f);
        	    glEnd();

        	    switch_dimension(2);
        	    RECT text_range = RECT{ 20, 10, 120, 70 };
        	    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
				glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
				if (cnt > 100) {
					glEnable(GL_TEXTURE_2D);
					text->draw(POINT{ text_range.left, text_range.top });
					glDisable(GL_TEXTURE_2D);
				}
				else {
					if (cnt == 100) text = new GL_TEXT(L"12345", htextDC);
					cnt++;
				}

        	    glLineWidth(2.0f);
        	    glBegin(GL_LINE_LOOP);
        			glColor3f(1.f, 1.f, 0.f);
        			glVertex2i(text_range.left , text_range.top);
        			glVertex2i(text_range.right, text_range.top);
        			glVertex2i(text_range.right, text_range.bottom);
        			glVertex2i(text_range.left , text_range.bottom);
    			glEnd();

        	    glFlush();
    			SwapBuffers(hDC);
        	    theta += .3f;
        	    Sleep(16);
        	}
    	}
    	delete text;
    	timeEndPeriod(1);
    	ReleaseDC(hwnd, hDC);
    	DestroyWindow(hwnd);
    	return msg.wParam;
	}
};

int main() {
	init_rand();
	GL_WINDOW top(400, 300);
	top.mainloop();
	return 0;
}
