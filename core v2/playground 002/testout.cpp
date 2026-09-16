#include <stdio.h>

#include "cyxlib/io.cpp"
// #include "cyxlib/non-crt-math.cpp"
#include "cyxlib/GLwrapper.cpp"


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

int main() {
	DEBUG_OUTPUT io(1024);
	using namespace NCM;
	io.print(L"sin30 = %\nsin45 = %\nsin60 = %\nsin90 = %\nsin120 = %\nsin210 (-30) = %\nsin(-30) = %\n",
		sin(PI / 6), sin(PI / 4), sin(PI / 3), sin(PI / 2), sin(PI * 2 / 3), sin(PI * 7 / 6), sin(-PI / 6));
	io.print(L"cos30 = %f\ncos45 = %f\ncos60 = %f\ncos90 = %f\ncos120 = %f\ncos210 = %f\ncos(-30) = %f\n",
		cos(PI / 6), cos(PI / 4), cos(PI / 3), cos(PI / 2), cos(PI * 2 / 3), cos(PI * 7 / 6), cos(-PI / 6));
	io.print(L"log(2, 32) = %f\npow(2, 5) = %f\npow(2, 1.5) = %f\n",
		log(2.f, 32.f), pow(2.f, 5.f), pow(2.f, 1.5f));
		
		
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
