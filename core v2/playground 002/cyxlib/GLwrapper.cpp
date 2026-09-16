#ifndef _WINDOWS_
	#define WIN32_LEAD_AND_MEAN
	#define NOMINMAX
	#include <windows.h>
#endif
#include <gl/gl.h>
#include <stdint.h>


struct GLENV {
	HDC     hmemDC;
	HBITMAP hBMP, holdBMP;
	BYTE*   DIBbits;
	HGLRC   hRC;
	int32_t size_x, size_y;
	GLENV(const GLENV&) = delete;
	GLENV& operator=(const GLENV&) = delete;
	static BITMAPINFOHEADER _create_BMIheader(int32_t bmp_size_x, int32_t bmp_size_y) {
		BITMAPINFOHEADER BMIheader = { 0 };
    	BMIheader.biSize = sizeof(BITMAPINFOHEADER);
    	BMIheader.biWidth  = bmp_size_x;
    	BMIheader.biHeight = bmp_size_y;
    	BMIheader.biPlanes = 1;
    	BMIheader.biBitCount = 24;
    	BMIheader.biCompression = BI_RGB;
    	return BMIheader;
	}
	static constexpr PIXELFORMATDESCRIPTOR _create_PFD() {
		PIXELFORMATDESCRIPTOR PFD = { 0 };
    	PFD.nSize = sizeof(PFD);
    	PFD.nVersion = 1;
    	PFD.dwFlags = PFD_DRAW_TO_BITMAP | PFD_SUPPORT_OPENGL | PFD_SUPPORT_GDI;
    	PFD.iPixelType = PFD_TYPE_RGBA;
    	PFD.cColorBits = 24;
    	PFD.cDepthBits = 16;
    	PFD.iLayerType = PFD_MAIN_PLANE;
    	return PFD;
	}
	GLENV(int32_t _size_x, int32_t _size_y): size_x(_size_x), size_y(_size_y), hmemDC(CreateCompatibleDC(NULL)) {
		BITMAPINFOHEADER BMIheader = _create_BMIheader(_size_x, _size_y);
    	hBMP = CreateDIBSection(hmemDC, (BITMAPINFO*)&BMIheader, DIB_RGB_COLORS, (void**)&DIBbits, NULL, 0);
    	holdBMP = (HBITMAP)SelectObject(hmemDC, hBMP);
    	PIXELFORMATDESCRIPTOR PFD = _create_PFD();
		int PF = ChoosePixelFormat(hmemDC, &PFD);
		SetPixelFormat(hmemDC, PF, &PFD);
		hRC = wglCreateContext(hmemDC);
		wglMakeCurrent(hmemDC, hRC);
	}
	~GLENV() {
		wglMakeCurrent(NULL, NULL);
    	wglDeleteContext(hRC);
    	SelectObject(hmemDC, holdBMP);
    	DeleteObject(hBMP);
    	DeleteDC(hmemDC);
	}
	void resize(int32_t new_size_x, int32_t new_size_y) {
		wglMakeCurrent(NULL, NULL);
    	wglDeleteContext(hRC);
    	SelectObject(hmemDC, holdBMP);
    	DeleteObject(hBMP);
    	BITMAPINFOHEADER BMIheader = _create_BMIheader(new_size_x, new_size_y);
    	hBMP = CreateDIBSection(hmemDC, (BITMAPINFO*)&BMIheader, DIB_RGB_COLORS, (void**)&DIBbits, NULL, 0);
    	holdBMP = (HBITMAP)SelectObject(hmemDC, hBMP);
    	hRC = wglCreateContext(hmemDC);
    	wglMakeCurrent(hmemDC, hRC);
    	size_x = new_size_x; size_y = new_size_y;
	}
};

