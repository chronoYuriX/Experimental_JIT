#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <gl/gl.h>
#include <stdio.h>

#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "winmm.lib")

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CLOSE: DestroyWindow(hwnd); break;
        case WM_DESTROY: PostQuitMessage(0); break;
        default: return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int main() {
    // 1. 创建窗口
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = L"MiniGL";
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowW(L"MiniGL", L"2D Texture Test",
                              WS_OVERLAPPEDWINDOW,
                              100, 100, 512, 512,
                              NULL, NULL, wc.hInstance, NULL);

    HDC hdc = GetDC(hwnd);
    ShowWindow(hwnd, SW_SHOW);

    // 2. 设置 OpenGL
    PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR), 1,
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        PFD_TYPE_RGBA, 24, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        24, 0, 0, PFD_MAIN_PLANE, 0, 0, 0, 0
    };
    SetPixelFormat(hdc, ChoosePixelFormat(hdc, &pfd), &pfd);
    HGLRC hrc = wglCreateContext(hdc);
    wglMakeCurrent(hdc, hrc);

    glViewport(0, 0, 512, 512);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, 512, 0, 512, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // 3. 创建纹理数据（64x64 的 RGBA 渐变图案）
    const int TEX_W = 64, TEX_H = 64;
    unsigned char pixels[TEX_H][TEX_W][4];

    for (int y = 0; y < TEX_H; y++) {
        for (int x = 0; x < TEX_W; x++) {
            pixels[y][x][0] = (unsigned char)(x * 4);       // R: 水平渐变
            pixels[y][x][1] = (unsigned char)(y * 4);       // G: 垂直渐变
            pixels[y][x][2] = 128;                          // B: 固定值
            pixels[y][x][3] = 255;                          // A: 不透明
        }
    }

    // 4. 创建并绑定纹理
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    // 重要：设置纹理参数
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    #define GL_CLAMP_TO_EDGE 0x812F
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // 上传纹理（这里是关键！）
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, TEX_W, TEX_H, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    // 检查错误
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        printf("OpenGL Error after glTexImage2D: 0x%x\n", err);
    }

    // 5. 主循环：绘制带纹理的四边形
    MSG msg;
    while (1) {
        if (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) break;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        } else {
            glClear(GL_COLOR_BUFFER_BIT);

            // 启用纹理
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, textureID);

            // 绘制带纹理的四边形（占据窗口中央区域）
            glBegin(GL_QUADS);
                glTexCoord2f(0.0f, 0.0f); glVertex2i(100, 100);
                glTexCoord2f(1.0f, 0.0f); glVertex2i(412, 100);
                glTexCoord2f(1.0f, 1.0f); glVertex2i(412, 412);
                glTexCoord2f(0.0f, 1.0f); glVertex2i(100, 412);
            glEnd();

            SwapBuffers(hdc);
            Sleep(16);
        }
    }

    // 清理
    glDeleteTextures(1, &textureID);
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(hrc);
    ReleaseDC(hwnd, hdc);
    DestroyWindow(hwnd);
    return 0;
}
