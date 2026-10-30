// Link against an ANGLE slice and target OS 27.0 to exercise the new Metal
// address-space rules. Rendering into a pbuffer needs no window or app bundle.
#define EGL_EGL_PROTOTYPES 1
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <EGL/eglext_angle.h>
#include <GLES3/gl3.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <vector>

static GLuint compile(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint status = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (!status) {
        GLint length = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        std::vector<char> log(length + 1);
        glGetShaderInfoLog(shader, length, nullptr, log.data());
        std::fprintf(stderr, "GLSL compile failed: %s\n", log.data());
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static bool run(const char* label, const char* fragment, std::array<int, 4> expected) {
    const char* vertex = R"(#version 300 es
    void main() {
        vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
        gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
    })";
    GLuint vs = compile(GL_VERTEX_SHADER, vertex);
    GLuint fs = compile(GL_FRAGMENT_SHADER, fragment);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return false;
    }
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    GLint linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLint length = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        std::vector<char> log(length + 1);
        glGetProgramInfoLog(program, length, nullptr, log.data());
        std::fprintf(stderr, "FAIL %s: %s\n", label, log.data());
        glDeleteProgram(program);
        glDeleteShader(vs);
        glDeleteShader(fs);
        while (glGetError()) {}
        return false;
    }
    glUseProgram(program);
    glUniform4f(glGetUniformLocation(program, "uValue"), 0.125f, 0.25f, 0.375f, 0.5f);
    glViewport(0, 0, 1, 1);
    glDisable(GL_DITHER);
    GLuint vao = 0;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    unsigned char pixel[4] = {};
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    GLenum error = glGetError();
    bool ok = error == GL_NO_ERROR;
    for (int i = 0; i < 4; ++i) ok &= std::abs(int(pixel[i]) - expected[i]) <= 1;
    std::printf("%s %s: pixel=(%u,%u,%u,%u), GL error=0x%x\n", ok ? "PASS" : "FAIL", label,
        unsigned(pixel[0]), unsigned(pixel[1]), unsigned(pixel[2]), unsigned(pixel[3]), error);
    glBindVertexArray(0);
    glDeleteVertexArrays(1, &vao);
    glUseProgram(0);
    glDeleteProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return ok;
}

int main() {
    const EGLint displayAttributes[] = {
        EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_METAL_ANGLE, EGL_NONE
    };
    auto getDisplay = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
    if (!getDisplay) {
        std::fprintf(stderr, "eglGetPlatformDisplayEXT is unavailable\n");
        return 2;
    }
    EGLDisplay display = getDisplay(EGL_PLATFORM_ANGLE_ANGLE, nullptr, displayAttributes);
    if (display == EGL_NO_DISPLAY || !eglInitialize(display, nullptr, nullptr)) {
        std::fprintf(stderr, "EGL init failed: 0x%x\n", eglGetError()); return 2;
    }
    if (!eglBindAPI(EGL_OPENGL_ES_API)) {
        std::fprintf(stderr, "EGL bind API failed: 0x%x\n", eglGetError());
        eglTerminate(display);
        return 2;
    }
    const EGLint configAttributes[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_NONE
    };
    EGLConfig config = nullptr;
    EGLint count = 0;
    if (!eglChooseConfig(display, configAttributes, &config, 1, &count) || !count) {
        std::fprintf(stderr, "EGL config failed: 0x%x\n", eglGetError());
        eglTerminate(display);
        return 2;
    }
    const EGLint surfaceAttributes[] = {EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE};
    const EGLint contextAttributes[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    EGLSurface surface = eglCreatePbufferSurface(display, config, surfaceAttributes);
    EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttributes);
    if (surface == EGL_NO_SURFACE || context == EGL_NO_CONTEXT || !eglMakeCurrent(display, surface, surface, context)) {
        std::fprintf(stderr, "EGL makeCurrent failed: 0x%x\n", eglGetError());
        if (context != EGL_NO_CONTEXT) eglDestroyContext(display, context);
        if (surface != EGL_NO_SURFACE) eglDestroySurface(display, surface);
        eglTerminate(display);
        return 2;
    }
    std::printf("GL_VERSION=%s\nGL_RENDERER=%s\n", glGetString(GL_VERSION), glGetString(GL_RENDERER));
    const char* structFragment = R"(#version 300 es
    precision highp float;
    uniform vec4 uValue;
    out vec4 outColor;
    struct Data { vec3 value; float weight; };
    void produce(out vec3 value, out float weight) { value = uValue.xyz; weight = uValue.w; }
    void adjust(inout vec3 value, inout float weight) { value += vec3(0.125); weight += 0.125; }
    void modify(inout Data d) {
        produce(d.value, d.weight);
        adjust(d.value, d.weight);
    }
    void main() {
        Data d;
        modify(d);
        outColor = vec4(d.value, d.weight);
    })";
    const char* swizzleFragment = R"(#version 300 es
    precision highp float;
    uniform vec4 uValue;
    out vec4 outColor;
    void adjust(inout vec2 value) { value += vec2(0.125); }
    void scalar(inout float value) { value += 0.125; }
    void main() {
        vec4 value = uValue;
        adjust(value.zy);
        scalar(value[int(gl_FragCoord.x) % 4]);
        outColor = value;
    })";
    bool structs = run("struct out/inout float/vec3", structFragment, {64, 96, 128, 159});
    bool swizzles = run("vector index/swizzle inout", swizzleFragment, {64, 96, 128, 128});
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(display, context);
    eglDestroySurface(display, surface);
    eglTerminate(display);
    return structs && swizzles ? 0 : 1;
}
