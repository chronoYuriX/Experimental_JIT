#ifndef _WINDOWS_
	// #define WIN32_LEAN_AND_MEAN
	#define NOMINMAX
    #define UNICODE
	#include <windows.h>
#endif
#include <gl/gl.h>
#include <stdint.h>

HANDLE _g_hheap = GetProcessHeap();

struct float_pair {
	float x, y;
	float_pair(float _x, float _y): x(_x), y(_y) { }
	float_pair() = default;
};

GLuint create_BGRA_texture(SIZE size, BYTE* img_data) {
	GLuint textureID;
	glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.cx, size.cy, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, img_data);
	return textureID;
}

struct GL_CHAR {
	wchar_t single_char;
	GLuint textureID;
	SIZE original_size, texture_size;
	static BITMAPINFO _create_BMI(SIZE BMPsize) {
		BITMAPINFO BMI = { 0 };
    	BMI.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    	BMI.bmiHeader.biWidth = BMPsize.cx;   BMI.bmiHeader.biHeight = -BMPsize.cy;
   		BMI.bmiHeader.biPlanes = 1;           BMI.bmiHeader.biBitCount = 32;
    	BMI.bmiHeader.biCompression = BI_RGB; BMI.bmiHeader.biSizeImage = BMPsize.cx * BMPsize.cy * 4;
    	return BMI;
	}
	static inline int32_t _ceiling_pow2(int32_t n) {
		int32_t ceiling = 1;
		while (ceiling < n) ceiling <<= 1;
		return ceiling;
	}
	static inline SIZE _get_texture_size(SIZE _original_size) {
		return SIZE{ _ceiling_pow2(_original_size.cx), _ceiling_pow2(_original_size.cy) };
	}
	GL_CHAR(wchar_t _single_char, HDC htextDC): single_char(_single_char) {
		GetTextExtentPoint32W(htextDC, &single_char, 1, &original_size);
		texture_size = _get_texture_size(original_size);
		BITMAPINFO BMI = _create_BMI(original_size);
		BYTE* texture_data;
		HBITMAP htextBMP = CreateDIBSection(htextDC, &BMI, DIB_RGB_COLORS, (void**)&texture_data, NULL, 0);
		RECT texture_zone = RECT{ 0, 0, original_size.cx, original_size.cy };
		SelectObject(htextDC, htextBMP);
		ExtTextOutW(htextDC, 0, 0, ETO_CLIPPED, &texture_zone, &single_char, 1, NULL);
		textureID = create_BGRA_texture(original_size, texture_data);
		DeleteObject(htextBMP);
	}
	void draw(POINT point00) {
		if (glIsTexture(textureID)) glBindTexture(GL_TEXTURE_2D, textureID);
		float_pair texture_mag_rate(
			float(original_size.cx) / float(texture_size.cx), float(original_size.cy) / float(texture_size.cy));
		POINT edge{ point00.x + original_size.cx, point00.y + original_size.cy };
		glBegin(GL_QUADS);
        	glTexCoord2f(              0.0f,               0.0f); glVertex2i(point00.x, point00.y);
        	glTexCoord2f(texture_mag_rate.x,               0.0f); glVertex2i(   edge.x, point00.y);
        	glTexCoord2f(texture_mag_rate.x, texture_mag_rate.y); glVertex2i(   edge.x,    edge.y);
        	glTexCoord2f(              0.0f, texture_mag_rate.y); glVertex2i(point00.x,    edge.y);
    	glEnd();
	}
	~GL_CHAR() { glDeleteTextures(1, &textureID); }
};

struct GL_CHARSET {
	GL_CHAR* chars;
	size_t char_counter, max_chars;
	float expansion;
	GL_CHARSET(size_t _max_chars, float _expansion): max_chars(_max_chars), char_counter(0), expansion(_expansion),
		chars((GL_CHAR*)HeapAlloc(_g_hheap, 0, _max_chars * sizeof(GL_CHAR))) { }
	~GL_CHARSET() {
		if (chars != NULL) {
			HeapFree(_g_hheap, 0, chars);
			chars = NULL;
		}
	}/*
	void draw(wchar_t single_char) {
		d
	}*/
};

struct GL_TEXT {
	wchar_t* text;
	GLuint   textureID;
	SIZE     original_size, texture_size;
	bool     independent;
	static inline int32_t _ceiling_pow2(int32_t n) {
		int32_t ceiling = 1;
		while (ceiling < n) ceiling <<= 1;
		return ceiling;
	}
	GL_TEXT(const wchar_t* _text, HDC htextDC, bool _independent = 1): independent(_independent) {
		HANDLE process_heap = GetProcessHeap();
		int32_t text_length = wcslen(_text);
		text = (wchar_t*)HeapAlloc(process_heap, 0, (text_length + 1) * sizeof(wchar_t));
		wcscpy(text, _text);
		if (independent) {
			GetTextExtentPoint32W(htextDC, text, text_length, &original_size);
			texture_size = GL_CHAR::_get_texture_size(original_size);
			RECT texture_zone = RECT{ 0, 0, texture_size.cx, texture_size.cy };
			BITMAPINFO BMI = GL_CHAR::_create_BMI(texture_size);
			BYTE* texture_data;
			HBITMAP htextBMP = CreateDIBSection(htextDC, &BMI, DIB_RGB_COLORS, (void**)&texture_data, NULL, 0);
			SelectObject(htextDC, htextBMP);
			ExtTextOutW(htextDC, 0, 0, ETO_CLIPPED, &texture_zone, text, text_length, NULL);
			int32_t texture_size_xy = texture_size.cx * texture_size.cy;
			for (int32_t i = 0; i < texture_size_xy; i++) texture_data[i * 4 + 3] = 255;
    		textureID = create_BGRA_texture(texture_size, texture_data);
    		DeleteObject(htextBMP);
		}
	}
	~GL_TEXT() { if (independent) glDeleteTextures(1, &textureID); }
	void draw(POINT point00/*, GL_CHARSET* charset*/) {
		// if (independent) {
			if (glIsTexture(textureID)) glBindTexture(GL_TEXTURE_2D, textureID);
			float_pair texture_mag_rate(
				float(original_size.cx) / float(texture_size.cx), float(original_size.cy) / float(texture_size.cy));
			POINT edge{ point00.x + original_size.cx, point00.y + original_size.cy };
			glBegin(GL_QUADS);
        		glTexCoord2f(               0.f,                0.f); glVertex2i(point00.x, point00.y);
        		glTexCoord2f(texture_mag_rate.x,                0.f); glVertex2i(   edge.x, point00.y);
        		glTexCoord2f(texture_mag_rate.x, texture_mag_rate.y); glVertex2i(   edge.x,    edge.y);
        		glTexCoord2f(               0.f, texture_mag_rate.y); glVertex2i(point00.x,    edge.y);
    		glEnd();/*
		} else {
			for (int32_t i = 0; text[i] != L'\0'; i++) point00.x += charset->draw(text[i]);
		}*/
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
		TEXT_DEFAULT[] = L"Segoe UI";
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
		LF.lfHeight = -12;
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
	DWORD PID = GetCurrentProcessId(), tick = GetTickCount();
    srand(PID ^ tick);
	GL_WINDOW top(400, 300);
	top.mainloop();
	return 0;
}
