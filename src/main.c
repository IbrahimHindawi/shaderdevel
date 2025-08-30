// main.c — single-file Win32 + WGL + modern OpenGL triangle (core profile)
// Build (MSVC):
//   cl /Zi /W3 /DUNICODE /D_UNICODE main.c user32.lib gdi32.lib opengl32.lib
// Build (MinGW):
//   gcc -O2 -municode main.c -lgdi32 -lopengl32 -o app.exe

#define UNICODE
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define STRICT
#include <windows.h>
#include <wingdi.h>
#include <gl/GL.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>   // ptrdiff_t
#include <stdio.h>
#include <stdlib.h>

#pragma comment(lib, "opengl32.lib")

// ============================= Config ==============================
static const bool  g_fullscreen    = false;
static const bool  g_vsync_enabled = true;
static const float g_screen_near   = 0.1f;
static const float g_screen_far    = 1000.0f;

// =================== Minimal WGL extension defs ====================
#define WGL_DRAW_TO_WINDOW_ARB           0x2001
#define WGL_ACCELERATION_ARB             0x2003
#define WGL_SWAP_METHOD_ARB              0x2007
#define WGL_SUPPORT_OPENGL_ARB           0x2010
#define WGL_DOUBLE_BUFFER_ARB            0x2011
#define WGL_PIXEL_TYPE_ARB               0x2013
#define WGL_COLOR_BITS_ARB               0x2014
#define WGL_DEPTH_BITS_ARB               0x2022
#define WGL_STENCIL_BITS_ARB             0x2023
#define WGL_FULL_ACCELERATION_ARB        0x2027
#define WGL_SWAP_EXCHANGE_ARB            0x2028
#define WGL_TYPE_RGBA_ARB                0x202B
#define WGL_CONTEXT_MAJOR_VERSION_ARB    0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB    0x2092
#define WGL_CONTEXT_PROFILE_MASK_ARB     0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001

typedef BOOL  (WINAPI *PFNWGLCHOOSEPIXELFORMATARBPROC)(HDC,const int*,const FLOAT*,UINT,int*,UINT*);
typedef HGLRC (WINAPI *PFNWGLCREATECONTEXTATTRIBSARBPROC)(HDC,HGLRC,const int*);
typedef BOOL  (WINAPI *PFNWGLSWAPINTERVALEXTPROC)(int);

static PFNWGLCHOOSEPIXELFORMATARBPROC    p_wglChoosePixelFormatARB    = NULL;
static PFNWGLCREATECONTEXTATTRIBSARBPROC p_wglCreateContextAttribsARB = NULL;
static PFNWGLSWAPINTERVALEXTPROC         p_wglSwapIntervalEXT         = NULL;

// ============ Modern GL enums missing from legacy <GL/gl.h> ========
#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER                0x8892
#endif
#ifndef GL_STATIC_DRAW
#define GL_STATIC_DRAW                 0x88E4
#endif
#ifndef GL_VERTEX_SHADER
#define GL_VERTEX_SHADER               0x8B31
#endif
#ifndef GL_FRAGMENT_SHADER
#define GL_FRAGMENT_SHADER             0x8B30
#endif
#ifndef GL_COMPILE_STATUS
#define GL_COMPILE_STATUS              0x8B81
#endif
#ifndef GL_LINK_STATUS
#define GL_LINK_STATUS                 0x8B82
#endif
#ifndef GL_INFO_LOG_LENGTH
#define GL_INFO_LOG_LENGTH             0x8B84
#endif

// ======= Minimal modern GL function pointers (no loader libs) ======
typedef char GLchar;

typedef void   (APIENTRY *PFNGLGENVERTEXARRAYSPROC)(GLsizei, GLuint*);
typedef void   (APIENTRY *PFNGLBINDVERTEXARRAYPROC)(GLuint);
typedef void   (APIENTRY *PFNGLGENBUFFERSPROC)(GLsizei, GLuint*);
typedef void   (APIENTRY *PFNGLBINDBUFFERPROC)(GLenum, GLuint);
typedef void   (APIENTRY *PFNGLBUFFERDATAPROC)(GLenum, ptrdiff_t, const void*, GLenum);
typedef GLuint (APIENTRY *PFNGLCREATESHADERPROC)(GLenum);
typedef void   (APIENTRY *PFNGLSHADERSOURCEPROC)(GLuint, GLsizei, const GLchar* const*, const GLint*);
typedef void   (APIENTRY *PFNGLCOMPILESHADERPROC)(GLuint);
typedef void   (APIENTRY *PFNGLGETSHADERIVPROC)(GLuint, GLenum, GLint*);
typedef void   (APIENTRY *PFNGLGETSHADERINFOLOGPROC)(GLuint, GLsizei, GLsizei*, GLchar*);
typedef GLuint (APIENTRY *PFNGLCREATEPROGRAMPROC)(void);
typedef void   (APIENTRY *PFNGLATTACHSHADERPROC)(GLuint, GLuint);
typedef void   (APIENTRY *PFNGLLINKPROGRAMPROC)(GLuint);
typedef void   (APIENTRY *PFNGLGETPROGRAMIVPROC)(GLuint, GLenum, GLint*);
typedef void   (APIENTRY *PFNGLGETPROGRAMINFOLOGPROC)(GLuint, GLsizei, GLsizei*, GLchar*);
typedef void   (APIENTRY *PFNGLDELETEPROGRAMPROC)(GLuint);
typedef void   (APIENTRY *PFNGLDELETESHADERPROC)(GLuint);
typedef void   (APIENTRY *PFNGLUSEPROGRAMPROC)(GLuint);
typedef void   (APIENTRY *PFNGLVERTEXATTRIBPOINTERPROC)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
typedef void   (APIENTRY *PFNGLENABLEVERTEXATTRIBARRAYPROC)(GLuint);
typedef GLint  (APIENTRY *PFNGLGETATTRIBLOCATIONPROC)(GLuint, const GLchar*);

static PFNGLGENVERTEXARRAYSPROC         glGenVertexArrays     = NULL;
static PFNGLBINDVERTEXARRAYPROC         glBindVertexArray     = NULL;
static PFNGLGENBUFFERSPROC              glGenBuffers          = NULL;
static PFNGLBINDBUFFERPROC              glBindBuffer          = NULL;
static PFNGLBUFFERDATAPROC              glBufferData          = NULL;
static PFNGLCREATESHADERPROC            glCreateShader        = NULL;
static PFNGLSHADERSOURCEPROC            glShaderSource        = NULL;
static PFNGLCOMPILESHADERPROC           glCompileShader       = NULL;
static PFNGLGETSHADERIVPROC             glGetShaderiv         = NULL;
static PFNGLGETSHADERINFOLOGPROC        glGetShaderInfoLog    = NULL;
static PFNGLCREATEPROGRAMPROC           glCreateProgram       = NULL;
static PFNGLATTACHSHADERPROC            glAttachShader        = NULL;
static PFNGLLINKPROGRAMPROC             glLinkProgram         = NULL;
static PFNGLGETPROGRAMIVPROC            glGetProgramiv        = NULL;
static PFNGLGETPROGRAMINFOLOGPROC       glGetProgramInfoLog   = NULL;
static PFNGLDELETEPROGRAMPROC           glDeleteProgram       = NULL;
static PFNGLDELETESHADERPROC            glDeleteShader        = NULL;
static PFNGLUSEPROGRAMPROC              glUseProgram          = NULL;
static PFNGLVERTEXATTRIBPOINTERPROC     glVertexAttribPointer = NULL;
static PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray = NULL;
static PFNGLGETATTRIBLOCATIONPROC       glGetAttribLocation   = NULL;
// NOTE: glViewport is a core 1.1 function already in opengl32.lib — do NOT load it via wglGetProcAddress.

// ============================ App State ============================
typedef struct App {
    HINSTANCE hinst;
    HWND      hwnd;
    HDC       hdc;
    HGLRC     glrc;

    int       width, height;
    bool      keys[256];
    bool      running;

    GLuint    program;
    GLuint    vao, vbo;
} App;

static App g_app = {0};

// Forward decls
static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

// ==================== Helpers =====================================
static void WinMsgBoxA(const char* title, const char* msg) {
    int wlen = MultiByteToWideChar(CP_UTF8, 0, msg, -1, NULL, 0);
    int tlen = MultiByteToWideChar(CP_UTF8, 0, title, -1, NULL, 0);
    WCHAR* wmsg = (WCHAR*)malloc(wlen * sizeof(WCHAR));
    WCHAR* wttl = (WCHAR*)malloc(tlen * sizeof(WCHAR));
    MultiByteToWideChar(CP_UTF8, 0, msg, -1, wmsg, wlen);
    MultiByteToWideChar(CP_UTF8, 0, title, -1, wttl, tlen);
    MessageBoxW(NULL, wmsg, wttl, MB_OK | MB_ICONERROR);
    free(wmsg); free(wttl);
}

static void BuildIdentity(float m[16]) {
    memset(m, 0, sizeof(float)*16);
    m[0]=1; m[5]=1; m[10]=1; m[15]=1;
}
static void BuildPerspectiveFovLH(float m[16], float fov, float aspect, float zn, float zf) {
    float yScale = 1.0f / (float)tan(fov * 0.5f);
    float xScale = yScale / aspect;
    memset(m, 0, sizeof(float)*16);
    m[0] = xScale;
    m[5] = yScale;
    m[10] = zf/(zf-zn);
    m[11] = 1.0f;
    m[14] = (-zn*zf)/(zf-zn);
}

// ==================== Temp context to load extensions ==============
static bool CreateHiddenTempWindowAndLoadExtensions(void) {
    WNDCLASSW wc = {0};
    wc.style         = CS_OWNDC;
    wc.lpfnWndProc   = DefWindowProcW;
    wc.hInstance     = g_app.hinst;
    wc.lpszClassName = L"TmpWGL";
    if(!RegisterClassW(&wc)) return false;

    HWND temp = CreateWindowW(wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW,
                              CW_USEDEFAULT, CW_USEDEFAULT, 64, 64,
                              NULL, NULL, g_app.hinst, NULL);
    if(!temp) { UnregisterClassW(wc.lpszClassName, g_app.hinst); return false; }

    HDC tdc = GetDC(temp);
    if(!tdc) { DestroyWindow(temp); UnregisterClassW(wc.lpszClassName, g_app.hinst); return false; }

    PIXELFORMATDESCRIPTOR pfd = {0};
    pfd.nSize      = sizeof(pfd);
    pfd.nVersion   = 1;
    pfd.dwFlags    = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 24;
    pfd.cDepthBits = 24;
    pfd.cStencilBits=8;

    int pf = ChoosePixelFormat(tdc, &pfd);
    if(pf == 0 || !SetPixelFormat(tdc, pf, &pfd)) {
        ReleaseDC(temp, tdc); DestroyWindow(temp); UnregisterClassW(wc.lpszClassName, g_app.hinst);
        return false;
    }

    HGLRC trc = wglCreateContext(tdc);
    if(!trc) {
        ReleaseDC(temp, tdc); DestroyWindow(temp); UnregisterClassW(wc.lpszClassName, g_app.hinst);
        return false;
    }
    if(!wglMakeCurrent(tdc, trc)) {
        wglDeleteContext(trc);
        ReleaseDC(temp, tdc); DestroyWindow(temp); UnregisterClassW(wc.lpszClassName, g_app.hinst);
        return false;
    }

    p_wglChoosePixelFormatARB    = (PFNWGLCHOOSEPIXELFORMATARBPROC)   wglGetProcAddress("wglChoosePixelFormatARB");
    p_wglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");
    p_wglSwapIntervalEXT         = (PFNWGLSWAPINTERVALEXTPROC)        wglGetProcAddress("wglSwapIntervalEXT");

    bool ok = (p_wglChoosePixelFormatARB && p_wglCreateContextAttribsARB);
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(trc);
    ReleaseDC(temp, tdc);
    DestroyWindow(temp);
    UnregisterClassW(wc.lpszClassName, g_app.hinst);

    return ok;
}

// ============ Load modern GL funcs after a real context is current =
static bool LoadModernGLFunctions(void) {
    glGenVertexArrays         = (PFNGLGENVERTEXARRAYSPROC)         wglGetProcAddress("glGenVertexArrays");
    glBindVertexArray         = (PFNGLBINDVERTEXARRAYPROC)         wglGetProcAddress("glBindVertexArray");
    glGenBuffers              = (PFNGLGENBUFFERSPROC)              wglGetProcAddress("glGenBuffers");
    glBindBuffer              = (PFNGLBINDBUFFERPROC)              wglGetProcAddress("glBindBuffer");
    glBufferData              = (PFNGLBUFFERDATAPROC)              wglGetProcAddress("glBufferData");
    glCreateShader            = (PFNGLCREATESHADERPROC)            wglGetProcAddress("glCreateShader");
    glShaderSource            = (PFNGLSHADERSOURCEPROC)            wglGetProcAddress("glShaderSource");
    glCompileShader           = (PFNGLCOMPILESHADERPROC)           wglGetProcAddress("glCompileShader");
    glGetShaderiv             = (PFNGLGETSHADERIVPROC)             wglGetProcAddress("glGetShaderiv");
    glGetShaderInfoLog        = (PFNGLGETSHADERINFOLOGPROC)        wglGetProcAddress("glGetShaderInfoLog");
    glCreateProgram           = (PFNGLCREATEPROGRAMPROC)           wglGetProcAddress("glCreateProgram");
    glAttachShader            = (PFNGLATTACHSHADERPROC)            wglGetProcAddress("glAttachShader");
    glLinkProgram             = (PFNGLLINKPROGRAMPROC)             wglGetProcAddress("glLinkProgram");
    glGetProgramiv            = (PFNGLGETPROGRAMIVPROC)            wglGetProcAddress("glGetProgramiv");
    glGetProgramInfoLog       = (PFNGLGETPROGRAMINFOLOGPROC)       wglGetProcAddress("glGetProgramInfoLog");
    glDeleteProgram           = (PFNGLDELETEPROGRAMPROC)           wglGetProcAddress("glDeleteProgram");
    glDeleteShader            = (PFNGLDELETESHADERPROC)            wglGetProcAddress("glDeleteShader");
    glUseProgram              = (PFNGLUSEPROGRAMPROC)              wglGetProcAddress("glUseProgram");
    glVertexAttribPointer     = (PFNGLVERTEXATTRIBPOINTERPROC)     wglGetProcAddress("glVertexAttribPointer");
    glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC) wglGetProcAddress("glEnableVertexAttribArray");
    glGetAttribLocation       = (PFNGLGETATTRIBLOCATIONPROC)       wglGetProcAddress("glGetAttribLocation");
    // DO NOT load glViewport — it's already provided by opengl32.lib

    if(!glGenVertexArrays || !glBindVertexArray || !glGenBuffers || !glBindBuffer ||
       !glBufferData || !glCreateShader || !glShaderSource || !glCompileShader ||
       !glGetShaderiv || !glGetShaderInfoLog || !glCreateProgram || !glAttachShader ||
       !glLinkProgram || !glGetProgramiv || !glGetProgramInfoLog || !glDeleteProgram ||
       !glDeleteShader || !glUseProgram || !glVertexAttribPointer || !glEnableVertexAttribArray ||
       !glGetAttribLocation) {
        WinMsgBoxA("OpenGL function load failed",
                   "One or more modern OpenGL functions could not be loaded.");
        return false;
    }
    return true;
}

// ======================= Real GL context setup =====================
static bool InitOpenGLRealContext(HWND hwnd, int w, int h, bool vsync) {
    g_app.hdc = GetDC(hwnd);
    if(!g_app.hdc) return false;

    int attribs[] = {
        WGL_SUPPORT_OPENGL_ARB, TRUE,
        WGL_DRAW_TO_WINDOW_ARB, TRUE,
        WGL_ACCELERATION_ARB,   WGL_FULL_ACCELERATION_ARB,
        WGL_COLOR_BITS_ARB,     24,
        WGL_DEPTH_BITS_ARB,     24,
        WGL_STENCIL_BITS_ARB,   8,
        WGL_DOUBLE_BUFFER_ARB,  TRUE,
        WGL_SWAP_METHOD_ARB,    WGL_SWAP_EXCHANGE_ARB,
        WGL_PIXEL_TYPE_ARB,     WGL_TYPE_RGBA_ARB,
        0
    };

    int pixelFormat = 0;
    UINT count = 0;
    if(!p_wglChoosePixelFormatARB(g_app.hdc, attribs, NULL, 1, &pixelFormat, &count) || count == 0) {
        return false;
    }

    PIXELFORMATDESCRIPTOR desc;
    if(!DescribePixelFormat(g_app.hdc, pixelFormat, sizeof(desc), &desc)) return false;
    if(!SetPixelFormat(g_app.hdc, pixelFormat, &desc)) return false;

    int ctxAttribs[] = {
        WGL_CONTEXT_MAJOR_VERSION_ARB, 4,
        WGL_CONTEXT_MINOR_VERSION_ARB, 0,
        WGL_CONTEXT_PROFILE_MASK_ARB,  WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
        0
    };

    g_app.glrc = p_wglCreateContextAttribsARB(g_app.hdc, 0, ctxAttribs);
    if(!g_app.glrc) return false;

    if(!wglMakeCurrent(g_app.hdc, g_app.glrc)) return false;

    if(!LoadModernGLFunctions()) return false;

    glClearDepth(1.0f);
    glEnable(GL_DEPTH_TEST);
    glFrontFace(GL_CW);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    glViewport(0, 0, w, h);

    if(p_wglSwapIntervalEXT) p_wglSwapIntervalEXT(vsync ? 1 : 0);
    return true;
}

static void ShutdownOpenGL(void) {
    if(g_app.program) { glDeleteProgram(g_app.program); g_app.program = 0; }
    if(g_app.glrc) {
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(g_app.glrc);
        g_app.glrc = NULL;
    }
    if(g_app.hdc) {
        ReleaseDC(g_app.hwnd, g_app.hdc);
        g_app.hdc = NULL;
    }
}

// =================== Shader helpers & triangle setup ===============
static GLuint CompileShader(GLenum type, const char* src) {
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, NULL);
    glCompileShader(sh);
    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if(!ok) {
        GLint len = 0; glGetShaderiv(sh, GL_INFO_LOG_LENGTH, &len);
        char* log = (len > 1) ? (char*)malloc(len) : NULL;
        if(log) { GLsizei got=0; glGetShaderInfoLog(sh, len, &got, log); WinMsgBoxA("Shader compile error", log); free(log); }
        glDeleteShader(sh);
        return 0;
    }
    return sh;
}

static GLuint LinkProgram(GLuint vs, GLuint fs) {
    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if(!ok) {
        GLint len = 0; glGetProgramiv(p, GL_INFO_LOG_LENGTH, &len);
        char* log = (len > 1) ? (char*)malloc(len) : NULL;
        if(log) { GLsizei got=0; glGetProgramInfoLog(p, len, &got, log); WinMsgBoxA("Program link error", log); free(log); }
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

static bool CreateTrianglePipeline(void) {
    const char* vs_src =
        "#version 330 core\n"
        "layout(location=0) in vec2 aPos;\n"
        "layout(location=1) in vec3 aCol;\n"
        "out vec3 vCol;\n"
        "void main(){\n"
        "  vCol = aCol;\n"
        "  gl_Position = vec4(aPos, 0.0, 1.0);\n"
        "}\n";

    const char* fs_src =
        "#version 330 core\n"
        "in vec3 vCol;\n"
        "out vec4 FragColor;\n"
        "void main(){\n"
        "  FragColor = vec4(vCol, 1.0);\n"
        "}\n";

    GLuint vs = CompileShader(GL_VERTEX_SHADER, vs_src);
    if(!vs) return false;
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fs_src);
    if(!fs) { glDeleteShader(vs); return false; }

    g_app.program = LinkProgram(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if(!g_app.program) return false;

    const float verts[] = {
        //   x,     y,     r,    g,    b  (CW winding to match GL_CW front-face)
        -0.6f, -0.5f,   1.0f, 0.0f, 0.0f,
         0.0f,  0.6f,   0.0f, 0.0f, 1.0f,
         0.6f, -0.5f,   0.0f, 1.0f, 0.0f,
    };

    glGenVertexArrays(1, &g_app.vao);
    glBindVertexArray(g_app.vao);

    glGenBuffers(1, &g_app.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, g_app.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * (GLsizei)sizeof(float), (const void*)(0));
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * (GLsizei)sizeof(float), (const void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
    return true;
}

// ============================== Draw ===============================
static void BeginScene(float r, float g, float b, float a) {
    glClearColor(r,g,b,a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}
static void EndScene(void) {
    SwapBuffers(g_app.hdc);
}
static void DrawTriangle(void) {
    glUseProgram(g_app.program);
    glBindVertexArray(g_app.vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
    glUseProgram(0);
}

// ============================ Windowing ============================
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

static bool CreateMainWindowAndInitGL(int* outW, int* outH) {
    WNDCLASSEXW wc = {0};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = g_app.hinst;
    wc.hIcon         = LoadIcon(NULL, IDI_WINLOGO);
    wc.hIconSm       = wc.hIcon;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = L"Engine";
    if(!RegisterClassExW(&wc)) return false;

    if(!CreateHiddenTempWindowAndLoadExtensions()) {
        MessageBoxW(NULL, L"Could not initialize the OpenGL WGL extensions.", L"Error", MB_OK);
        return false;
    }

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);

    int w = 800, h = 600;
    int x = (sw - w)/2;
    int y = (sh - h)/2;

    DWORD exStyle = WS_EX_APPWINDOW;
    DWORD style   = WS_POPUP;

    g_app.hwnd = CreateWindowExW(exStyle, wc.lpszClassName, wc.lpszClassName, style,
                                 x, y, w, h, NULL, NULL, g_app.hinst, NULL);
    if(!g_app.hwnd) return false;

    if(!InitOpenGLRealContext(g_app.hwnd, w, h, g_vsync_enabled)) {
        MessageBoxW(g_app.hwnd, L"Could not initialize OpenGL (core context).", L"Error", MB_OK);
        return false;
    }

    if(!CreateTrianglePipeline()) return false;

    ShowWindow(g_app.hwnd, SW_SHOW);
    SetForegroundWindow(g_app.hwnd);
    SetFocus(g_app.hwnd);
    ShowCursor(FALSE);

    *outW = w; *outH = h;
    return true;
}

static void DestroyMainWindow(void) {
    ShowCursor(TRUE);
    if(g_fullscreen) ChangeDisplaySettings(NULL, 0);
    if(g_app.hwnd) {
        DestroyWindow(g_app.hwnd);
        g_app.hwnd = NULL;
    }
    UnregisterClassW(L"Engine", g_app.hinst);
}

// ========================= Message handling ========================
static void OnKeyDown(WPARAM wparam) {
    if(wparam < 256) g_app.keys[wparam] = true;
}
static void OnKeyUp(WPARAM wparam) {
    if(wparam < 256) g_app.keys[wparam] = false;
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch(msg) {
    case WM_KEYDOWN: OnKeyDown(wparam); return 0;
    case WM_KEYUP:   OnKeyUp(wparam);   return 0;
    case WM_SIZE: {
        int ww = LOWORD(lparam), hh = HIWORD(lparam);
        if(ww > 0 && hh > 0 && g_app.hdc) glViewport(0, 0, ww, hh);
    } return 0;
    case WM_CLOSE:   PostQuitMessage(0); return 0;
    default:         return DefWindowProcW(hwnd, msg, wparam, lparam);
    }
}

// =============================== WinMain ===========================
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    g_app.hinst = hInst;
    memset(g_app.keys, 0, sizeof(g_app.keys));

    int w=0, h=0;
    if(!CreateMainWindowAndInitGL(&w, &h)) return 0;

    g_app.running = true;
    MSG msg; ZeroMemory(&msg, sizeof(msg));

    while(g_app.running) {
        while(PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if(msg.message == WM_QUIT) g_app.running = false;
        }

        if(g_app.keys[VK_ESCAPE]) g_app.running = false;

        BeginScene(0.1f, 0.1f, 0.12f, 1.0f);
        DrawTriangle();
        EndScene();
    }

    ShutdownOpenGL();
    DestroyMainWindow();
    return 0;
}
