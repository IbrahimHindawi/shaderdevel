// main.c — Win32 + WGL + OpenGL core shader playground with live reload (quad)
// Build (MSVC):
//   cl /Zi /W3 /DUNICODE /D_UNICODE main.c user32.lib gdi32.lib opengl32.lib
//
// Files watched (same dir as EXE, override via command line):
//   shader.vert
//   shader.frag
//
// Hotkeys: F5 = recompile, ESC = quit, Space = pause time
// Uniforms: uTime (float), uResolution (vec2), uMouse (vec2, pixels)

#define UNICODE
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define STRICT
#include <windows.h>
#include <wingdi.h>
#include <gl/GL.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <math.h>

#pragma comment(lib, "opengl32.lib")

// ============================= Config ==============================
static const bool  g_vsync_enabled = true;

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

// ======= Minimal modern GL function pointers (no loader libs) ======
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

typedef char GLchar;
typedef void   (APIENTRY *PFNGLGENVERTEXARRAYSPROC)(GLsizei, GLuint*);
typedef void   (APIENTRY *PFNGLBINDVERTEXARRAYPROC)(GLuint);
typedef void   (APIENTRY *PFNGLDELETEVERTEXARRAYSPROC)(GLsizei,const GLuint*);
typedef void   (APIENTRY *PFNGLGENBUFFERSPROC)(GLsizei, GLuint*);
typedef void   (APIENTRY *PFNGLBINDBUFFERPROC)(GLenum, GLuint);
typedef void   (APIENTRY *PFNGLBUFFERDATAPROC)(GLenum, ptrdiff_t, const void*, GLenum);
typedef void   (APIENTRY *PFNGLDELETEBUFFERSPROC)(GLsizei,const GLuint*);
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
typedef GLint  (APIENTRY *PFNGLGETUNIFORMLOCATIONPROC)(GLuint, const GLchar*);
typedef void   (APIENTRY *PFNGLUNIFORM1FPROC)(GLint, float);
typedef void   (APIENTRY *PFNGLUNIFORM2FPROC)(GLint, float, float);

static PFNGLGENVERTEXARRAYSPROC         glGenVertexArrays     = NULL;
static PFNGLBINDVERTEXARRAYPROC         glBindVertexArray     = NULL;
static PFNGLDELETEVERTEXARRAYSPROC      glDeleteVertexArrays  = NULL;
static PFNGLGENBUFFERSPROC              glGenBuffers          = NULL;
static PFNGLBINDBUFFERPROC              glBindBuffer          = NULL;
static PFNGLBUFFERDATAPROC              glBufferData          = NULL;
static PFNGLDELETEBUFFERSPROC           glDeleteBuffers       = NULL;
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
static PFNGLGETUNIFORMLOCATIONPROC      glGetUniformLocation  = NULL;
static PFNGLUNIFORM1FPROC               glUniform1f           = NULL;
static PFNGLUNIFORM2FPROC               glUniform2f           = NULL;
// glViewport is core 1.1 in opengl32.dll — don’t load via wglGetProcAddress.

// ============================ App State ============================
typedef struct {
    HINSTANCE hinst;
    HWND      hwnd;
    HDC       hdc;
    HGLRC     glrc;
    int       width, height;
    bool      running;
    bool      key_down[256];
    bool      paused;

    GLuint    program;
    GLuint    vao, vbo;

    // uniforms
    GLint     uTime, uResolution, uMouse;

    // shader files + mtimes
    WCHAR     vert_path[MAX_PATH];
    WCHAR     frag_path[MAX_PATH];
    FILETIME  vert_time;
    FILETIME  frag_time;

    POINT     mouse; // in window client coords
    LARGE_INTEGER qpf;
    double    start_seconds;
    double    paused_offset;
} App;

static App g_app = {0};

// Forward decls
static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

// ==================== Small helpers ================================
static void WinMsgBoxUTF8(const char* title, const char* msg) {
    int wlen = MultiByteToWideChar(CP_UTF8, 0, msg, -1, NULL, 0);
    int tlen = MultiByteToWideChar(CP_UTF8, 0, title, -1, NULL, 0);
    WCHAR* wmsg = (WCHAR*)malloc(wlen * sizeof(WCHAR));
    WCHAR* wttl = (WCHAR*)malloc(tlen * sizeof(WCHAR));
    MultiByteToWideChar(CP_UTF8, 0, msg, -1, wmsg, wlen);
    MultiByteToWideChar(CP_UTF8, 0, title, -1, wttl, tlen);
    MessageBoxW(NULL, wmsg, wttl, MB_OK | MB_ICONERROR);
    free(wmsg); free(wttl);
}
static bool ReadFileUTF8(const WCHAR* path, char** out_data, size_t* out_size) {
    *out_data = NULL; *out_size = 0;
    HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size;
    if (!GetFileSizeEx(f, &size)) { CloseHandle(f); return false; }
    if (size.QuadPart <= 0 || size.QuadPart > 10485760) { CloseHandle(f); return false; } // <=10MB
    char* buf = (char*)malloc((size_t)size.QuadPart + 1);
    DWORD read = 0; BOOL ok = ReadFile(f, buf, (DWORD)size.QuadPart, &read, NULL);
    CloseHandle(f);
    if (!ok) { free(buf); return false; }
    buf[read] = 0;
    *out_data = buf; *out_size = read;
    return true;
}
static bool GetFileWriteTime(const WCHAR* path, FILETIME* ft) {
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExW(path, GetFileExInfoStandard, &fad)) return false;
    *ft = fad.ftLastWriteTime;
    return true;
}
static double NowSeconds(void) {
    LARGE_INTEGER qpc; QueryPerformanceCounter(&qpc);
    return (double)qpc.QuadPart / (double)g_app.qpf.QuadPart;
}

// ==================== Temp context to load extensions ==============
static bool CreateHiddenTempWindowAndLoadExtensions(void) {
    WNDCLASSW wc = {0};
    wc.style         = CS_OWNDC;
    wc.lpfnWndProc   = DefWindowProcW;
    wc.hInstance     = g_app.hinst;
    wc.lpszClassName = L"TmpWGL";
    if(!RegisterClassW(&wc)) return false;

    HWND temp = CreateWindowW(wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 64, 64, NULL, NULL, g_app.hinst, NULL);
    if(!temp) { UnregisterClassW(wc.lpszClassName, g_app.hinst); return false; }

    HDC tdc = GetDC(temp);

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

    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(trc);
    ReleaseDC(temp, tdc);
    DestroyWindow(temp);
    UnregisterClassW(wc.lpszClassName, g_app.hinst);

    return (p_wglChoosePixelFormatARB && p_wglCreateContextAttribsARB);
}

// ============ Load modern GL funcs after a real context is current =
static bool LoadModernGLFunctions(void) {
    // Load via wglGetProcAddress
    glGenVertexArrays         = (PFNGLGENVERTEXARRAYSPROC)         wglGetProcAddress("glGenVertexArrays");
    glBindVertexArray         = (PFNGLBINDVERTEXARRAYPROC)         wglGetProcAddress("glBindVertexArray");
    glDeleteVertexArrays      = (PFNGLDELETEVERTEXARRAYSPROC)      wglGetProcAddress("glDeleteVertexArrays");
    glGenBuffers              = (PFNGLGENBUFFERSPROC)              wglGetProcAddress("glGenBuffers");
    glBindBuffer              = (PFNGLBINDBUFFERPROC)              wglGetProcAddress("glBindBuffer");
    glBufferData              = (PFNGLBUFFERDATAPROC)              wglGetProcAddress("glBufferData");
    glDeleteBuffers           = (PFNGLDELETEBUFFERSPROC)           wglGetProcAddress("glDeleteBuffers");
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
    glGetUniformLocation      = (PFNGLGETUNIFORMLOCATIONPROC)      wglGetProcAddress("glGetUniformLocation");
    glUniform1f               = (PFNGLUNIFORM1FPROC)               wglGetProcAddress("glUniform1f");
    glUniform2f               = (PFNGLUNIFORM2FPROC)               wglGetProcAddress("glUniform2f");

    if(!glGenVertexArrays || !glBindVertexArray || !glDeleteVertexArrays ||
       !glGenBuffers || !glBindBuffer || !glBufferData || !glDeleteBuffers ||
       !glCreateShader || !glShaderSource || !glCompileShader ||
       !glGetShaderiv || !glGetShaderInfoLog || !glCreateProgram || !glAttachShader ||
       !glLinkProgram || !glGetProgramiv || !glGetProgramInfoLog ||
       !glDeleteProgram || !glDeleteShader || !glUseProgram ||
       !glVertexAttribPointer || !glEnableVertexAttribArray || !glGetAttribLocation ||
       !glGetUniformLocation || !glUniform1f || !glUniform2f) {
        WinMsgBoxUTF8("OpenGL function load failed",
                      "Some required modern OpenGL functions are missing.");
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

    int pixelFormat = 0; UINT count = 0;
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

    glDisable(GL_DEPTH_TEST);
    glViewport(0, 0, w, h);
    if(p_wglSwapIntervalEXT) p_wglSwapIntervalEXT(vsync ? 1 : 0);
    return true;
}

static void ShutdownOpenGL(void) {
    if(g_app.program) { glDeleteProgram(g_app.program); g_app.program = 0; }
    if(g_app.vbo) { glDeleteBuffers(1, &g_app.vbo); g_app.vbo = 0; }
    if(g_app.vao) { glDeleteVertexArrays(1, &g_app.vao); g_app.vao = 0; }
    if(g_app.glrc) { wglMakeCurrent(NULL, NULL); wglDeleteContext(g_app.glrc); g_app.glrc = NULL; }
    if(g_app.hdc) { ReleaseDC(g_app.hwnd, g_app.hdc); g_app.hdc = NULL; }
}

// =================== Shader compile/link + program swap ============
static GLuint CompileShader(GLenum type, const char* src, char* logbuf, int logbufsz) {
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, NULL);
    glCompileShader(sh);
    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if(!ok) {
        GLsizei got=0;
        glGetShaderInfoLog(sh, logbufsz, &got, logbuf);
        glDeleteShader(sh);
        return 0;
    }
    return sh;
}
static GLuint LinkProgram(GLuint vs, GLuint fs, char* logbuf, int logbufsz) {
    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if(!ok) {
        GLsizei got=0;
        glGetProgramInfoLog(p, logbufsz, &got, logbuf);
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

static bool LoadAndBuildProgramFromFiles(const WCHAR* vpath, const WCHAR* fpath, GLuint* outProg, char* outLog, int outLogSz) {
    *outProg = 0; outLog[0] = 0;

    char *vsrc=NULL,*fsrc=NULL; size_t vsz=0, fsz=0;
    if(!ReadFileUTF8(vpath, &vsrc, &vsz)) { snprintf(outLog,outLogSz,"Failed to read vertex shader file."); return false; }
    if(!ReadFileUTF8(fpath, &fsrc, &fsz)) { snprintf(outLog,outLogSz,"Failed to read fragment shader file."); free(vsrc); return false; }

    GLuint vs = CompileShader(GL_VERTEX_SHADER,   vsrc, outLog, outLogSz);
    if(!vs) { free(vsrc); free(fsrc); return false; }
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fsrc, outLog, outLogSz);
    if(!fs) { glDeleteShader(vs); free(vsrc); free(fsrc); return false; }

    GLuint prog = LinkProgram(vs, fs, outLog, outLogSz);
    glDeleteShader(vs); glDeleteShader(fs);
    free(vsrc); free(fsrc);
    if(!prog) return false;

    *outProg = prog;
    return true;
}

static void ApplyProgram(GLuint prog) {
    if (g_app.program) glDeleteProgram(g_app.program);
    g_app.program = prog;
    glUseProgram(g_app.program);
    g_app.uTime       = glGetUniformLocation(g_app.program, "uTime");
    g_app.uResolution = glGetUniformLocation(g_app.program, "uResolution");
    g_app.uMouse      = glGetUniformLocation(g_app.program, "uMouse");
}

// ============== Quad (pos,uv) =====================================
static void CreateFullscreenQuad(void) {
    // Triangle strip: (-1, -1) .. (1, 1)
    // pos.xy, uv.xy
    const float verts[] = {
        -1.f, -1.f,   0.f, 0.f,
         1.f, -1.f,   1.f, 0.f,
        -1.f,  1.f,   0.f, 1.f,
         1.f,  1.f,   1.f, 1.f,
    };
    glGenVertexArrays(1, &g_app.vao);
    glBindVertexArray(g_app.vao);
    glGenBuffers(1, &g_app.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, g_app.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

    GLint locPos = glGetAttribLocation(g_app.program, "aPos");
    GLint locUV  = glGetAttribLocation(g_app.program, "aUV");

    // In case shader uses layout(location=...), these are typically 0 and 1,
    // but we query to be robust.
    if (locPos < 0) locPos = 0;
    if (locUV  < 0) locUV  = 1;

    glVertexAttribPointer((GLuint)locPos, 2, GL_FLOAT, GL_FALSE, 4*(GLsizei)sizeof(float), (const void*)(0));
    glEnableVertexAttribArray((GLuint)locPos);
    glVertexAttribPointer((GLuint)locUV,  2, GL_FLOAT, GL_FALSE, 4*(GLsizei)sizeof(float), (const void*)(2*sizeof(float)));
    glEnableVertexAttribArray((GLuint)locUV);
    glBindVertexArray(0);
}

// ============== File watching (mtime polling) ======================
static bool TimesEqual(const FILETIME* a, const FILETIME* b) {
    return (a->dwLowDateTime==b->dwLowDateTime && a->dwHighDateTime==b->dwHighDateTime);
}
static bool CheckAndHotReload(void) {
    FILETIME vt={0}, ft={0};
    bool v_ok = GetFileWriteTime(g_app.vert_path, &vt);
    bool f_ok = GetFileWriteTime(g_app.frag_path, &ft);
    if(!v_ok || !f_ok) return false;

    bool changed = (!TimesEqual(&vt, &g_app.vert_time) || !TimesEqual(&ft,&g_app.frag_time));
    if (!changed) return false;

    char logbuf[4096];
    GLuint newProg=0;
    if (LoadAndBuildProgramFromFiles(g_app.vert_path, g_app.frag_path, &newProg, logbuf, sizeof(logbuf))) {
        g_app.vert_time = vt; g_app.frag_time = ft;
        ApplyProgram(newProg);
        // Rebind VAO because attrib locations could have changed
        if (g_app.vao) { glDeleteVertexArrays(1, &g_app.vao); g_app.vao = 0; }
        if (g_app.vbo) { glDeleteBuffers(1, &g_app.vbo); g_app.vbo = 0; }
        CreateFullscreenQuad();

        WCHAR title[512];
        swprintf(title, 512, L"Shader Playground — OK");
        SetWindowTextW(g_app.hwnd, title);
    } else {
        WCHAR title[512];
        swprintf(title, 512, L"Shader Playground — COMPILE ERROR");
        SetWindowTextW(g_app.hwnd, title);
        WinMsgBoxUTF8("Shader Error", logbuf[0]?logbuf:"Compile/link failed.");
    }
    return true;
}

// ============================ Drawing ==============================
static void Render(float timeSec) {
    glViewport(0,0,g_app.width,g_app.height);
    glUseProgram(g_app.program);
    if (g_app.uTime >= 0)       glUniform1f(g_app.uTime, g_app.paused ? (float)g_app.paused_offset : timeSec);
    if (g_app.uResolution >= 0) glUniform2f(g_app.uResolution, (float)g_app.width, (float)g_app.height);
    if (g_app.uMouse >= 0)      glUniform2f(g_app.uMouse, (float)g_app.mouse.x, (float)g_app.mouse.y);

    glBindVertexArray(g_app.vao);
    // using triangle strip; 4 verts
    glDrawArrays(0x0005 /*GL_TRIANGLE_STRIP*/, 0, 4);
    SwapBuffers(g_app.hdc);
}

// ============================ Windowing ============================
static void UpdateMouse(void) {
    POINT p; GetCursorPos(&p);
    ScreenToClient(g_app.hwnd, &p);
    g_app.mouse = p;
}
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch(msg) {
    case WM_SIZE: {
        g_app.width  = LOWORD(lparam);
        g_app.height = HIWORD(lparam);
        if(g_app.width>0 && g_app.height>0 && g_app.hdc) glViewport(0,0,g_app.width,g_app.height);
    } return 0;
    case WM_KEYDOWN:
        if (wparam < 256) g_app.key_down[wparam] = true;
        if (wparam == VK_F5) CheckAndHotReload();
        if (wparam == VK_SPACE) {
            g_app.paused = !g_app.paused;
            if (g_app.paused) {
                double now = NowSeconds() - g_app.start_seconds;
                g_app.paused_offset = now;
            } else {
                // shift start so time continues from paused_offset
                g_app.start_seconds = NowSeconds() - g_app.paused_offset;
            }
        }
        return 0;
    case WM_KEYUP:
        if (wparam < 256) g_app.key_down[wparam] = false;
        return 0;
    case WM_CLOSE:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wparam, lparam);
    }
}

// ================ App bootstrap & loop =============================
static bool CreateMainWindowAndInitGL(int w, int h) {
    WNDCLASSEXW wc = {0};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = g_app.hinst;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = L"ShaderPlayground";
    if(!RegisterClassExW(&wc)) return false;

    if(!CreateHiddenTempWindowAndLoadExtensions()) {
        MessageBoxW(NULL, L"Could not initialize WGL extensions.", L"Error", MB_OK | MB_ICONERROR);
        return false;
    }

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    int x = (sw - w)/2, y = (sh - h)/2;

    DWORD exStyle = WS_EX_APPWINDOW;
    // DWORD style   = WS_POPUP;
    DWORD style   = WS_OVERLAPPEDWINDOW;
    g_app.hwnd = CreateWindowExW(exStyle, wc.lpszClassName, L"Shader Playground", style, x, y, w, h, NULL, NULL, g_app.hinst, NULL);
    if(!g_app.hwnd) return false;

    if(!InitOpenGLRealContext(g_app.hwnd, w, h, g_vsync_enabled)) {
        MessageBoxW(g_app.hwnd, L"Could not initialize OpenGL (core context).", L"Error", MB_OK | MB_ICONERROR);
        return false;
    }

    ShowWindow(g_app.hwnd, SW_SHOW);
    SetForegroundWindow(g_app.hwnd);
    SetFocus(g_app.hwnd);
    ShowCursor(TRUE); // leave cursor visible for playground

    return true;
}

static void ResolveDefaultPathsFromExe(void) {
    WCHAR exe[MAX_PATH];
    GetModuleFileNameW(NULL, exe, MAX_PATH);
    // strip filename
    WCHAR dir[MAX_PATH];
    wcscpy(dir, exe);
    for (int i=(int)wcslen(dir)-1; i>=0; --i) { if (dir[i]==L'\\' || dir[i]==L'/') { dir[i]=0; break; } }

    swprintf(g_app.vert_path, MAX_PATH, L"src\\shader.vert", dir);
    swprintf(g_app.frag_path, MAX_PATH, L"src\\shader.frag", dir);
}

static void SeedTimes(void) {
    FILETIME t;
    if (GetFileWriteTime(g_app.vert_path, &t)) g_app.vert_time = t;
    if (GetFileWriteTime(g_app.frag_path, &t)) g_app.frag_time = t;
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrevInstance, PWSTR cmd, int nCmdShow) {
    (void)hPrevInstance; (void)cmd; (void)nCmdShow;
    g_app.hinst = hInst;
    QueryPerformanceFrequency(&g_app.qpf);

    // default shader paths (override via command line: first token vert, second frag)
    ResolveDefaultPathsFromExe();
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv && argc >= 2) wcsncpy(g_app.vert_path, argv[1], MAX_PATH);
    if (argv && argc >= 3) wcsncpy(g_app.frag_path, argv[2], MAX_PATH);
    if (argv) LocalFree(argv);

    // Create window + GL
    g_app.width=1280; g_app.height=720;
    if(!CreateMainWindowAndInitGL(g_app.width, g_app.height)) return 0;

    // First compile/link
    char logbuf[4096];
    GLuint prog=0;
    if (!LoadAndBuildProgramFromFiles(g_app.vert_path, g_app.frag_path, &prog, logbuf, sizeof(logbuf))) {
        WinMsgBoxUTF8("Initial compile failed", logbuf[0]?logbuf:"Could not build shaders.");
        ShutdownOpenGL();
        DestroyWindow(g_app.hwnd);
        return 0;
    }
    ApplyProgram(prog);
    CreateFullscreenQuad();
    SeedTimes();

    g_app.running = true;
    g_app.paused = false;
    g_app.start_seconds = NowSeconds();

    MSG msg; ZeroMemory(&msg, sizeof(msg));
    while(g_app.running) {
        while(PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if(msg.message == WM_QUIT) g_app.running = false;
        }
        if(!g_app.running) break;
        if (g_app.key_down[VK_ESCAPE]) g_app.running = false;

        // Poll mouse & hot-reload files (if modified)
        UpdateMouse();
        CheckAndHotReload();

        // Time
        double t = NowSeconds() - g_app.start_seconds;
        Render((float)t);
    }

    ShutdownOpenGL();
    if (g_app.hwnd) DestroyWindow(g_app.hwnd);
    UnregisterClassW(L"ShaderPlayground", g_app.hinst);
    return 0;
}
