// Run beside the packaged ANGLE DLLs. WARP makes this independent of CI GPUs.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <EGL/eglext_angle.h>
#include <GLES3/gl3.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <vector>

static void require(bool condition, const char* operation) {
    if (!condition) throw std::runtime_error(operation);
}

static void eglRequire(bool condition, const char* operation) {
    if (!condition) {
        std::fprintf(stderr, "%s: EGL error=0x%x\n", operation, eglGetError());
        throw std::runtime_error(operation);
    }
}

struct EGLState {
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLContext context = EGL_NO_CONTEXT;
    EGLSurface pbuffer = EGL_NO_SURFACE;
    EGLSurface windowSurface = EGL_NO_SURFACE;
    HWND window = nullptr;

    bool close() {
        bool ok = true;
        auto checked = [&ok](EGLBoolean result, const char* operation) {
            if (result != EGL_TRUE) {
                std::fprintf(stderr, "%s: EGL error=0x%x\n", operation, eglGetError());
                ok = false;
            }
        };
        if (display != EGL_NO_DISPLAY) {
            checked(eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT), "unbind");
            if (windowSurface != EGL_NO_SURFACE) checked(eglDestroySurface(display, windowSurface), "destroy window surface");
            if (pbuffer != EGL_NO_SURFACE) checked(eglDestroySurface(display, pbuffer), "destroy pbuffer");
            if (context != EGL_NO_CONTEXT) checked(eglDestroyContext(display, context), "destroy context");
            checked(eglTerminate(display), "terminate");
            display = EGL_NO_DISPLAY;
        }
        if (window) {
            if (!DestroyWindow(window)) {
                std::fprintf(stderr, "DestroyWindow: Win32 error=%lu\n", GetLastError());
                ok = false;
            }
            window = nullptr;
        }
        checked(eglReleaseThread(), "release thread");
        return ok;
    }
    ~EGLState() { close(); }
};

static GLuint compile(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    require(shader != 0, "glCreateShader");
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled != GL_TRUE) {
        GLint length = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        std::vector<char> log(static_cast<size_t>(length) + 1);
        glGetShaderInfoLog(shader, length, nullptr, log.data());
        std::fprintf(stderr, "Shader compilation failed: %s\n", log.data());
        glDeleteShader(shader);
        throw std::runtime_error("glCompileShader");
    }
    return shader;
}

static void drawAndRead() {
    struct GLObjects {
        GLuint vertex = 0, fragment = 0, program = 0, vao = 0;
        ~GLObjects() {
            glUseProgram(0);
            glBindVertexArray(0);
            if (vao) glDeleteVertexArrays(1, &vao);
            if (program) glDeleteProgram(program);
            if (fragment) glDeleteShader(fragment);
            if (vertex) glDeleteShader(vertex);
        }
    } objects;
    objects.vertex = compile(GL_VERTEX_SHADER, R"(#version 300 es
void main() {
    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
})");
    objects.fragment = compile(GL_FRAGMENT_SHADER, R"(#version 300 es
precision highp float;
out vec4 color;
void main() { color = vec4(0.25, 0.5, 0.75, 1.0); }
)");
    objects.program = glCreateProgram();
    require(objects.program != 0, "glCreateProgram");
    glAttachShader(objects.program, objects.vertex);
    glAttachShader(objects.program, objects.fragment);
    glLinkProgram(objects.program);
    GLint linked = GL_FALSE;
    glGetProgramiv(objects.program, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        GLint length = 0;
        glGetProgramiv(objects.program, GL_INFO_LOG_LENGTH, &length);
        std::vector<char> log(static_cast<size_t>(length) + 1);
        glGetProgramInfoLog(objects.program, length, nullptr, log.data());
        std::fprintf(stderr, "Program link failed: %s\n", log.data());
        throw std::runtime_error("glLinkProgram");
    }
    glViewport(0, 0, 4, 4);
    glDisable(GL_DITHER);
    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glUseProgram(objects.program);
    glGenVertexArrays(1, &objects.vao);
    glBindVertexArray(objects.vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    std::array<unsigned char, 4> pixel{};
    glReadPixels(2, 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
    const GLenum error = glGetError();
    std::printf("Pixel=(%u,%u,%u,%u), GL error=0x%x\n", unsigned(pixel[0]), unsigned(pixel[1]), unsigned(pixel[2]), unsigned(pixel[3]), error);
    require(error == GL_NO_ERROR, "draw/read GL error");
    constexpr std::array<int, 4> expected{64, 128, 191, 255};
    for (size_t i = 0; i < pixel.size(); ++i)
        require(std::abs(int(pixel[i]) - expected[i]) <= 1, "unexpected rendered pixel");
}

int main() {
    try {
        EGLState state;
        auto getDisplay = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
        require(getDisplay != nullptr, "eglGetPlatformDisplayEXT unavailable");
        const EGLint displayAttributes[] = {
            EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,
            EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_DEVICE_TYPE_D3D_WARP_ANGLE, EGL_NONE};
        state.display = getDisplay(EGL_PLATFORM_ANGLE_ANGLE, EGL_DEFAULT_DISPLAY, displayAttributes);
        eglRequire(state.display != EGL_NO_DISPLAY, "get D3D11 WARP display");
        eglRequire(eglInitialize(state.display, nullptr, nullptr) == EGL_TRUE, "eglInitialize");
        eglRequire(eglBindAPI(EGL_OPENGL_ES_API) == EGL_TRUE, "eglBindAPI");
        const EGLint configAttributes[] = {
            EGL_SURFACE_TYPE, EGL_PBUFFER_BIT | EGL_WINDOW_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
            EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
            EGL_DEPTH_SIZE, 24, EGL_STENCIL_SIZE, 8, EGL_NONE};
        EGLConfig config = nullptr;
        EGLint count = 0;
        eglRequire(eglChooseConfig(state.display, configAttributes, &config, 1, &count) == EGL_TRUE && count == 1, "choose ES3/depth24/stencil8 config");
        const EGLint contextAttributes[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
        state.context = eglCreateContext(state.display, config, EGL_NO_CONTEXT, contextAttributes);
        eglRequire(state.context != EGL_NO_CONTEXT, "create ES3 context");
        const EGLint pbufferAttributes[] = {EGL_WIDTH, 4, EGL_HEIGHT, 4, EGL_NONE};
        state.pbuffer = eglCreatePbufferSurface(state.display, config, pbufferAttributes);
        eglRequire(state.pbuffer != EGL_NO_SURFACE, "create pbuffer");
        eglRequire(eglMakeCurrent(state.display, state.pbuffer, state.pbuffer, state.context) == EGL_TRUE, "make pbuffer current");
        const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
        const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
        GLint major = 0;
        glGetIntegerv(GL_MAJOR_VERSION, &major);
        require(version && renderer && major >= 3, "GLES3 information unavailable");
        std::printf("GL_VERSION=%s\nGL_RENDERER=%s\n", version, renderer);
        require(std::strstr(renderer, "ANGLE") && std::strstr(renderer, "Direct3D11"), "unexpected renderer");
        drawAndRead();
        require(glGetError() == GL_NO_ERROR, "GL resource cleanup error");
        state.window = CreateWindowExW(0, L"STATIC", L"ANGLE smoke", WS_OVERLAPPED, 0, 0, 64, 64, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        require(state.window != nullptr, "create hidden HWND");
        state.windowSurface = eglCreateWindowSurface(state.display, config, reinterpret_cast<EGLNativeWindowType>(state.window), nullptr);
        eglRequire(state.windowSurface != EGL_NO_SURFACE, "create HWND surface");
        eglRequire(eglMakeCurrent(state.display, state.windowSurface, state.windowSurface, state.context) == EGL_TRUE, "make HWND current");
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        require(glGetError() == GL_NO_ERROR, "HWND GL error");
        eglRequire(eglSwapBuffers(state.display, state.windowSurface) == EGL_TRUE, "HWND swap");
        require(state.close(), "EGL/Win32 cleanup failed");
        std::puts("PASS: ANGLE D3D11 WARP GLES3 shaders, pixels, HWND swap and cleanup");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
