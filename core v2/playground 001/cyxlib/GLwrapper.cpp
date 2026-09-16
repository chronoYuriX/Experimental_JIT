#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <gl/gl.h>
#include <stdio.h>
#include <stdint.h>


int SaveBMP(const char* filename, HBITMAP hbm)
{
    BITMAP bm;
    if (!GetObject(hbm, sizeof(bm), &bm)) return -1;
    BITMAPFILEHEADER bf = {0};
    BITMAPINFOHEADER bi = {0};
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = bm.bmWidth;
    bi.biHeight = bm.bmHeight;      // 正高度 => 自下而上 DIB
    bi.biPlanes = 1;
    bi.biBitCount = bm.bmBitsPixel;
    bi.biCompression = BI_RGB;

    int stride = ((bm.bmWidth * bm.bmBitsPixel + 31) / 32) * 4;
    int dataSize = stride * bm.bmHeight;

    bf.bfType = 0x4D42; // 'BM'
    bf.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + dataSize;
    bf.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

    FILE* fp = fopen(filename, "wb");
    if (!fp) return -2;

    fwrite(&bf, sizeof(bf), 1, fp);
    fwrite(&bi, sizeof(bi), 1, fp);

    // 读取 DIB 像素数据
    BYTE* bits = (BYTE*)malloc(dataSize);
    if (bits) {
        HDC dc = CreateCompatibleDC(0);
        SelectObject(dc, hbm);
        GetDIBits(dc, hbm, 0, bm.bmHeight, bits, (BITMAPINFO*)&bi, DIB_RGB_COLORS);
        DeleteDC(dc);
        fwrite(bits, 1, dataSize, fp);
        free(bits);
    }

    fclose(fp);
    return 0;
}

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

int main()
{
	GLENV GLenv(400, 300);
	
    glViewport(0, 0, GLenv.size_x, GLenv.size_y);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0,  GLenv.size_x, 0,  GLenv.size_y, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glClearColor(0.2f, 0.3f, 0.4f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glBegin(GL_TRIANGLES);
        glColor3f(1.0f, 0.0f, 0.0f);
        glVertex2i(10, 10);
        glColor3f(0.0f, 1.0f, 0.0f);
        glVertex2i(GLenv.size_x - 10, 20);
        glColor3f(0.0f, 0.0f, 1.0f);
        glVertex2i(GLenv.size_x / 2,  GLenv.size_y - 10);
    glEnd();
    glFlush();
    if (SaveBMP("output.bmp", GLenv.hBMP) == 0) system("output.bmp");
    else printf("Failed!\n");
    
    GLenv.resize(200, 150);
    
    glViewport(0, 0, GLenv.size_x, GLenv.size_y);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0,  GLenv.size_x, 0,  GLenv.size_y, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glClearColor(0.2f, 0.3f, 0.4f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    glBegin(GL_TRIANGLES);
        glColor3f(1.0f, 0.0f, 0.0f);
        glVertex2i(10, 10);
        glColor3f(0.0f, 1.0f, 0.0f);
        glVertex2i(GLenv.size_x - 10, 20);
        glColor3f(0.0f, 0.0f, 1.0f);
        glVertex2i(GLenv.size_x / 2,  GLenv.size_y - 10);
    glEnd();
    glFlush();
    if (SaveBMP("output_small.bmp", GLenv.hBMP) == 0) system("output_small.bmp");
    else printf("Falied\n");
    
    return 0;
}
