#define UNICODE
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define WIN_32_EXTRA_LEAN
#include "glad/glad.h"
#include "glad/glad_wgl.h"
#include <windows.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WGL_CONTEXT_PROFILE_MASK_ARB 0x9126

#pragma comment(lib, "opengl32.lib")

// ============================== App State ==============================
typedef struct {
    int cols, rows;
    bool paused;
    int frame;
    double startSec;
    double pausedAccum;
    double lastReloadCheck;
    int seed;
    int winW, winH;
    float mouseX, mouseY;
    int mouseDown;
} AppState;

typedef struct {
    HINSTANCE hInst;
    HWND      hwnd;
    HDC       hdc;
    HGLRC     hglrc;
    LARGE_INTEGER qpf;
    AppState  st;
    wchar_t   shaderPathW[MAX_PATH];
    uint64_t  shaderStamp;
} App;

// ============================== Utils ==============================
static double now_sec(App* A){
    LARGE_INTEGER qpc; QueryPerformanceCounter(&qpc);
    return (double)qpc.QuadPart / (double)A->qpf.QuadPart;
}
static uint64_t file_mtime_u64W(const wchar_t* path){
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExW(path, GetFileExInfoStandard, &fad)) return 0ULL;
    ULARGE_INTEGER u; u.LowPart = fad.ftLastWriteTime.dwLowDateTime; u.HighPart = fad.ftLastWriteTime.dwHighDateTime; 
    return u.QuadPart;
}
static char* read_text_fileW(const wchar_t* path){
    FILE* f = _wfopen(path, L"rb"); if(!f) return NULL;
    fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET);
    char* s=(char*)malloc(n+1); if(!s){ fclose(f); return NULL; }
    fread(s,1,n,f); s[n]=0; fclose(f); return s;
}

// ============================== GL Helpers ==============================
static GLuint compile_shader(GLenum type, const char* src, const char* dbg){
    GLuint s = glCreateShader(type);
    glShaderSource(s,1,&src,NULL);
    glCompileShader(s);
    GLint ok=0; glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
    if(!ok){ GLint L=0; glGetShaderiv(s,GL_INFO_LOG_LENGTH,&L); char* log=(char*)malloc(L);
        glGetShaderInfoLog(s,L,NULL,log); fprintf(stderr,"[Shader %s]\n%s\n", dbg, log); free(log); glDeleteShader(s); return 0; }
    return s;
}
static GLuint link_prog(GLuint vs, GLuint fs){
    GLuint p=glCreateProgram(); glAttachShader(p,vs); glAttachShader(p,fs); glLinkProgram(p);
    GLint ok=0; glGetProgramiv(p,GL_LINK_STATUS,&ok);
    if(!ok){ GLint L=0; glGetProgramiv(p,GL_INFO_LOG_LENGTH,&L); char* log=(char*)malloc(L);
        glGetProgramInfoLog(p,L,NULL,log); fprintf(stderr,"[Link]\n%s\n",log); free(log); glDeleteProgram(p); return 0; }
    return p;
}

// Fullscreen quad
static GLuint gVAO=0, gVBO=0;
static void make_quad(){
    float v[] = { // pos, uv
        -1,-1, 0,0,   1,-1, 1,0,   1, 1, 1,1,
        -1,-1, 0,0,   1, 1, 1,1,  -1, 1, 0,1
    };
    glGenVertexArrays(1, &gVAO);
    glGenBuffers(1, &gVBO);
    glBindVertexArray(gVAO);
    glBindBuffer(GL_ARRAY_BUFFER, gVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,4*sizeof(float),(void*)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,4*sizeof(float),(void*)(2*sizeof(float)));
    glBindVertexArray(0);
}

static const char* VERT_SRC =
"#version 330 core\n"
"layout(location=0) in vec2 aPos;\n"
"layout(location=1) in vec2 aUV;\n"
"out vec2 vUV;\n"
"void main(){ vUV=aUV; gl_Position=vec4(aPos,0.0,1.0); }\n";

static const char* FRAG_PROLOG =
"#version 330 core\n"
"out vec4 FragColor;\n"
"in vec2 vUV;\n"
"uniform float iTime;\n"
"uniform vec2  iResolution;\n"
"uniform vec2  iMouse;\n"
"uniform ivec2 iGrid;\n"
"uniform bool  iPaused;\n"
"uniform int   iFrame;\n"
"uniform int   iSeed;\n"
"void mainImage(out vec4 fragColor, in vec2 fragCoord, in ivec2 cell, in vec2 cellUV);\n";

static const char* FRAG_DEFAULT_USER =
"float hash(vec2 p){ return fract(sin(dot(p,vec2(41.0,289.0)))*43758.5453); }\n"
"void mainImage(out vec4 fragColor, in vec2 fragCoord, in ivec2 cell, in vec2 cellUV){\n"
"  vec2 uv = cellUV;\n"
"  float t = iTime*0.5 + float(iSeed)*0.1 + hash(vec2(cell));\n"
"  float v = 0.5+0.5*cos(6.2831*(uv.x+uv.y+t));\n"
"  vec3 col = mix(vec3(0.05,0.1,0.2), vec3(0.9,0.8,0.2), v);\n"
"  col *= 0.7 + 0.3*hash(vec2(cell)+0.123);\n"
"  fragColor = vec4(col,1.0);\n"
"}\n";

static const char* FRAG_EPILOG =
"void main(){\n"
"  vec2 fragCoord = vUV * iResolution;\n"
"  vec2 gridSize   = vec2(max(1,iGrid.x), max(1,iGrid.y));\n"
"  vec2 cellPix    = iResolution / gridSize;\n"
"  ivec2 cell      = ivec2(clamp(floor(fragCoord / cellPix), vec2(0.0), gridSize-1.0));\n"
"  vec2 cellUV     = fract(fragCoord / cellPix);\n"
"  vec4 c; mainImage(c, fragCoord, cell, cellUV);\n"
"  FragColor = c;\n"
"}\n";

typedef struct {
    GLuint prog;
    GLint u_iTime, u_iRes, u_iMouse, u_iGrid, u_iPaused, u_iFrame, u_iSeed;
} GLProg;

static GLuint build_program_from_user(const char* user){
    size_t N = strlen(FRAG_PROLOG)+strlen(user)+strlen(FRAG_EPILOG)+8;
    char* fs = (char*)malloc(N);
    strcpy(fs, FRAG_PROLOG); strcat(fs, user); strcat(fs, FRAG_EPILOG);
    GLuint vs = compile_shader(GL_VERTEX_SHADER, VERT_SRC, "VS");
    if(!vs){ free(fs); return 0; }
    GLuint fsid = compile_shader(GL_FRAGMENT_SHADER, fs, "FS(user+wrap)");
    free(fs);
    if(!fsid){ glDeleteShader(vs); return 0; }
    GLuint p = link_prog(vs, fsid);
    glDeleteShader(vs); glDeleteShader(fsid);
    return p;
}
static void bind_uniforms(GLProg* gp){
    gp->u_iTime  = glGetUniformLocation(gp->prog, "iTime");
    gp->u_iRes   = glGetUniformLocation(gp->prog, "iResolution");
    gp->u_iMouse = glGetUniformLocation(gp->prog, "iMouse");
    gp->u_iGrid  = glGetUniformLocation(gp->prog, "iGrid");
    gp->u_iPaused= glGetUniformLocation(gp->prog, "iPaused");
    gp->u_iFrame = glGetUniformLocation(gp->prog, "iFrame");
    gp->u_iSeed  = glGetUniformLocation(gp->prog, "iSeed");
}

// ============================== WGL Setup ==============================
static bool wgl_make_context_33(HWND hwnd, HDC* outDC, HGLRC* outRC){
    PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR), 1,
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        PFD_TYPE_RGBA, 32,
        8,0, 8,0, 8,0, 8,0, 24, 8, 0,
        PFD_MAIN_PLANE, 0, 0,0,0
    };
    HDC hdc = GetDC(hwnd);
    int pf = ChoosePixelFormat(hdc, &pfd);
    if(!pf || !SetPixelFormat(hdc, pf, &pfd)) return false;

    // temp legacy context
    HGLRC temp = wglCreateContext(hdc);
    if(!temp) return false;
    wglMakeCurrent(hdc, temp);

    // load WGL extensions
    if (!gladLoadWGL(hdc)) return false;

    // ask for 3.3 core context
    int attribs[] = {
        WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
        WGL_CONTEXT_MINOR_VERSION_ARB, 3,
        WGL_CONTEXT_PROFILE_MASK_ARB,  WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
#ifdef _DEBUG
        WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_DEBUG_BIT_ARB,
#endif
        0
    };
    HGLRC rc = NULL;
    if (wglCreateContextAttribsARB)
        rc = wglCreateContextAttribsARB(hdc, 0, attribs);
    if (!rc) rc = temp; // fallback to legacy (not ideal)

    if (rc != temp) {
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(temp);
        wglMakeCurrent(hdc, rc);
    }

    if (!gladLoadGL((GLADloadfunc)wglGetProcAddress)) return false;
    *outDC = hdc; *outRC = rc;
    return true;
}

// ============================== Win32 ==============================
static App* GApp = NULL;

static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l){
    App* A = GApp;
    switch(m){
        case WM_SIZE:
            if (A){ A->st.winW = LOWORD(l); A->st.winH = HIWORD(l); glViewport(0,0,A->st.winW,A->st.winH); }
            return 0;
        case WM_LBUTTONDOWN: SetCapture(h); if(A){ A->st.mouseDown=1; } return 0;
        case WM_LBUTTONUP:   ReleaseCapture(); if(A){ A->st.mouseDown=0; } return 0;
        case WM_MOUSEMOVE:
            if (A){ A->st.mouseX = (float)GET_X_LPARAM(l); A->st.mouseY = (float)(A->st.winH - GET_Y_LPARAM(l)); }
            return 0;
        case WM_KEYDOWN:
            if (!A) break;
            if (w==VK_ESCAPE) PostQuitMessage(0);
            if (w=='P') A->st.paused = !A->st.paused;
            if (w=='R') A->st.lastReloadCheck = -1.0;
            if (w==VK_OEM_4 /*[*/) A->st.cols = (A->st.cols>1)? A->st.cols-1 : 1;
            if (w==VK_OEM_6 /*]*/) A->st.cols = A->st.cols+1;
            // Shift+[ and Shift+] emulate { and }
            if ((GetKeyState(VK_SHIFT)&0x8000) && w==VK_OEM_4) A->st.rows = (A->st.rows>1)? A->st.rows-1 : 1;
            if ((GetKeyState(VK_SHIFT)&0x8000) && w==VK_OEM_6) A->st.rows = A->st.rows+1;
            return 0;
        case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h,m,w,l);
}

// ============================== Entry ==============================
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int){
    App A = {0}; GApp=&A; A.hInst=hInst; A.st.cols=4; A.st.rows=3; A.st.seed=1337;
    QueryPerformanceFrequency(&A.qpf);

    // Paths
    GetModuleFileNameW(NULL, A.shaderPathW, MAX_PATH);
    wchar_t* slash = wcsrchr(A.shaderPathW, L'\\'); if (slash) *(slash+1)=0;
    wcscat(A.shaderPathW, L"src/user_shader.glsl");
    A.shaderStamp = file_mtime_u64W(A.shaderPathW);

    // Window
    WNDCLASSW wc = {0};
    wc.style = CS_OWNDC; wc.lpfnWndProc = WndProc; wc.hInstance=hInst;
    wc.lpszClassName = L"ShaderGridWin32"; wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassW(&wc);
    RECT r = {0,0,1280,720}; AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hwnd = CreateWindowW(wc.lpszClassName, L"Shader Grid Lab (Win32+WGL)",
        WS_OVERLAPPEDWINDOW|WS_VISIBLE, CW_USEDEFAULT,CW_USEDEFAULT,
        r.right-r.left, r.bottom-r.top, NULL,NULL,hInst,NULL);
    A.hwnd = hwnd;

    // GL context
    if (!wgl_make_context_33(hwnd, &A.hdc, &A.hglrc)){
        MessageBoxW(hwnd, L"OpenGL init failed", L"Error", MB_ICONERROR|MB_OK);
        return 1;
    }

    // GL resources
    make_quad();

    // Build initial program
    GLProg gp={0};
    char* user = read_text_fileW(A.shaderPathW);
    if(!user) user = _strdup(FRAG_DEFAULT_USER);
    gp.prog = build_program_from_user(user);
    if(!gp.prog){
        fprintf(stderr,"Initial shader failed. Falling back.\n");
        free(user); user = _strdup(FRAG_DEFAULT_USER);
        gp.prog = build_program_from_user(user);
        if(!gp.prog){ MessageBoxW(hwnd,L"Shader build failed",L"Error",MB_ICONERROR|MB_OK); return 1; }
    }
    bind_uniforms(&gp);
    free(user);

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    A.st.startSec = now_sec(&A);
    double prev = A.st.startSec;

    // Main loop
    MSG msg;
    while (1){
        while (PeekMessageW(&msg,NULL,0,0,PM_REMOVE)){
            if (msg.message==WM_QUIT) goto end;
            TranslateMessage(&msg); DispatchMessageW(&msg);
        }

        // hot-reload 10x/sec
        double tnow = now_sec(&A);
        if (A.st.lastReloadCheck + 0.1 < tnow){
            A.st.lastReloadCheck = tnow;
            uint64_t ts = file_mtime_u64W(A.shaderPathW);
            if (ts && ts != A.shaderStamp){
                A.shaderStamp = ts;
                char* s = read_text_fileW(A.shaderPathW);
                if(!s) s = _strdup(FRAG_DEFAULT_USER);
                GLuint np = build_program_from_user(s);
                if (np){
                    glDeleteProgram(gp.prog);
                    gp.prog = np; bind_uniforms(&gp);
                    A.st.frame = 0;
                    OutputDebugStringW(L"[Reloaded shader]\n");
                } else {
                    OutputDebugStringW(L"[Reload failed, keeping previous]\n");
                }
                free(s);
            }
        }

        // time
        double dt = tnow - prev; prev = tnow;
        if (A.st.paused) A.st.pausedAccum += dt;
        float iTime = (float)(tnow - A.st.startSec - A.st.pausedAccum);

        // draw
        glDisable(GL_DEPTH_TEST);
        glClearColor(0.05f,0.06f,0.08f,1.f);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(gp.prog);
        glUniform1f(gp.u_iTime, iTime);
        glUniform2f(gp.u_iRes, (float)A.st.winW, (float)A.st.winH);
        glUniform2f(gp.u_iMouse, A.st.mouseX, A.st.mouseY);
        glUniform2i(gp.u_iGrid, A.st.cols, A.st.rows);
        glUniform1i(gp.u_iPaused, A.st.paused?1:0);
        glUniform1i(gp.u_iFrame, A.st.frame);
        glUniform1i(gp.u_iSeed, A.st.seed);

        glBindVertexArray(gVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        SwapBuffers(A.hdc);
        A.st.frame++;
    }

end:
    if (gp.prog) glDeleteProgram(gp.prog);
    if (gVBO) glDeleteBuffers(1,&gVBO);
    if (gVAO) glDeleteVertexArrays(1,&gVAO);
    if (A.hglrc){ wglMakeCurrent(NULL,NULL); wglDeleteContext(A.hglrc); }
    if (A.hdc) ReleaseDC(A.hwnd, A.hdc);
    return 0;
}
