#include "render.h"

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct {
    float m[16];
} mat4;

static mat4 mat4_identity(void) {
    mat4 out = { {1, 0, 0, 0,
                 0, 1, 0, 0,
                 0, 0, 1, 0,
                 0, 0, 0, 1} };
    return out;
}

static mat4 mat4_mul(mat4 a, mat4 b) {
    mat4 out = { {0} };
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            out.m[row * 4 + col] =
                a.m[row * 4 + 0] * b.m[0 * 4 + col] +
                a.m[row * 4 + 1] * b.m[1 * 4 + col] +
                a.m[row * 4 + 2] * b.m[2 * 4 + col] +
                a.m[row * 4 + 3] * b.m[3 * 4 + col];
        }
    }
    return out;
}

static mat4 mat4_translate(float x, float y, float z) {
    mat4 out = mat4_identity();
    out.m[12] = x;
    out.m[13] = y;
    out.m[14] = z;
    return out;
}

static mat4 mat4_rotate_y(float r) {
    mat4 out = mat4_identity();
    float c = cosf(r);
    float s = sinf(r);
    out.m[0] = c;
    out.m[2] = s;
    out.m[8] = -s;
    out.m[10] = c;
    return out;
}

static mat4 mat4_rotate_x(float r) {
    mat4 out = mat4_identity();
    float c = cosf(r);
    float s = sinf(r);
    out.m[5] = c;
    out.m[6] = -s;
    out.m[9] = s;
    out.m[10] = c;
    return out;
}

static mat4 mat4_perspective(float fovy_rad, float aspect, float z_near, float z_far) {
    float f = 1.0f / tanf(fovy_rad * 0.5f);
    mat4 out = { {0} };
    out.m[0] = f / aspect;
    out.m[5] = f;
    out.m[10] = (z_far + z_near) / (z_near - z_far);
    out.m[11] = -1.0f;
    out.m[14] = (2.0f * z_far * z_near) / (z_near - z_far);
    return out;
}

typedef struct {
    float pos[3];
    float color[3];
} vertex;

static const vertex cube_vertices[] = {
    // Front (red)
    {{-1, -1,  1}, {1, 0, 0}},
    {{ 1, -1,  1}, {1, 0, 0}},
    {{ 1,  1,  1}, {1, 0, 0}},
    {{-1, -1,  1}, {1, 0, 0}},
    {{ 1,  1,  1}, {1, 0, 0}},
    {{-1,  1,  1}, {1, 0, 0}},
    // Back (green)
    {{-1, -1, -1}, {0, 1, 0}},
    {{-1,  1, -1}, {0, 1, 0}},
    {{ 1,  1, -1}, {0, 1, 0}},
    {{-1, -1, -1}, {0, 1, 0}},
    {{ 1,  1, -1}, {0, 1, 0}},
    {{ 1, -1, -1}, {0, 1, 0}},
    // Left (blue)
    {{-1, -1, -1}, {0, 0, 1}},
    {{-1, -1,  1}, {0, 0, 1}},
    {{-1,  1,  1}, {0, 0, 1}},
    {{-1, -1, -1}, {0, 0, 1}},
    {{-1,  1,  1}, {0, 0, 1}},
    {{-1,  1, -1}, {0, 0, 1}},
    // Right (yellow)
    {{ 1, -1, -1}, {1, 1, 0}},
    {{ 1,  1, -1}, {1, 1, 0}},
    {{ 1,  1,  1}, {1, 1, 0}},
    {{ 1, -1, -1}, {1, 1, 0}},
    {{ 1,  1,  1}, {1, 1, 0}},
    {{ 1, -1,  1}, {1, 1, 0}},
    // Top (cyan)
    {{-1,  1, -1}, {0, 1, 1}},
    {{-1,  1,  1}, {0, 1, 1}},
    {{ 1,  1,  1}, {0, 1, 1}},
    {{-1,  1, -1}, {0, 1, 1}},
    {{ 1,  1,  1}, {0, 1, 1}},
    {{ 1,  1, -1}, {0, 1, 1}},
    // Bottom (magenta)
    {{-1, -1, -1}, {1, 0, 1}},
    {{ 1, -1, -1}, {1, 0, 1}},
    {{ 1, -1,  1}, {1, 0, 1}},
    {{-1, -1, -1}, {1, 0, 1}},
    {{ 1, -1,  1}, {1, 0, 1}},
    {{-1, -1,  1}, {1, 0, 1}},
};

struct cs_renderer {
    int width;
    int height;
    float fps;
    float angle;
    EGLDisplay display;
    EGLContext context;
    EGLSurface surface;
    GLuint program;
    GLuint vbo;
    GLint loc_pos;
    GLint loc_color;
    GLint loc_mvp;
};

static GLuint compile_shader(GLenum type, const char *source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    return shader;
}

static GLuint link_program(const char *vs_src, const char *fs_src) {
    GLuint vs = compile_shader(GL_VERTEX_SHADER, vs_src);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fs_src);
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

cs_renderer *cs_render_create(const cs_render_config *config) {
    if (!config) {
        return NULL;
    }

    cs_renderer *renderer = (cs_renderer *)calloc(1, sizeof(cs_renderer));
    if (!renderer) {
        return NULL;
    }

    renderer->width = config->width;
    renderer->height = config->height;
    renderer->fps = config->fps;

    renderer->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (renderer->display == EGL_NO_DISPLAY) {
        free(renderer);
        return NULL;
    }

    if (!eglInitialize(renderer->display, NULL, NULL)) {
        free(renderer);
        return NULL;
    }

    EGLint attribs[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_NONE
    };

    EGLConfig cfg;
    EGLint num_configs = 0;
    if (!eglChooseConfig(renderer->display, attribs, &cfg, 1, &num_configs) || num_configs == 0) {
        eglTerminate(renderer->display);
        free(renderer);
        return NULL;
    }

    EGLint pbuffer_attribs[] = {
        EGL_WIDTH, renderer->width,
        EGL_HEIGHT, renderer->height,
        EGL_NONE
    };

    renderer->surface = eglCreatePbufferSurface(renderer->display, cfg, pbuffer_attribs);
    if (renderer->surface == EGL_NO_SURFACE) {
        eglTerminate(renderer->display);
        free(renderer);
        return NULL;
    }

    EGLint ctx_attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    renderer->context = eglCreateContext(renderer->display, cfg, EGL_NO_CONTEXT, ctx_attribs);
    if (renderer->context == EGL_NO_CONTEXT) {
        eglDestroySurface(renderer->display, renderer->surface);
        eglTerminate(renderer->display);
        free(renderer);
        return NULL;
    }

    if (!eglMakeCurrent(renderer->display, renderer->surface, renderer->surface, renderer->context)) {
        eglDestroyContext(renderer->display, renderer->context);
        eglDestroySurface(renderer->display, renderer->surface);
        eglTerminate(renderer->display);
        free(renderer);
        return NULL;
    }

    const char *vs_src =
        "attribute vec3 a_pos;\n"
        "attribute vec3 a_color;\n"
        "uniform mat4 u_mvp;\n"
        "varying vec3 v_color;\n"
        "void main() { v_color = a_color; gl_Position = u_mvp * vec4(a_pos, 1.0); }\n";

    const char *fs_src =
        "precision mediump float;\n"
        "varying vec3 v_color;\n"
        "void main() { gl_FragColor = vec4(v_color, 1.0); }\n";

    renderer->program = link_program(vs_src, fs_src);
    renderer->loc_pos = glGetAttribLocation(renderer->program, "a_pos");
    renderer->loc_color = glGetAttribLocation(renderer->program, "a_color");
    renderer->loc_mvp = glGetUniformLocation(renderer->program, "u_mvp");

    glGenBuffers(1, &renderer->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cube_vertices), cube_vertices, GL_STATIC_DRAW);

    glEnable(GL_DEPTH_TEST);
    glViewport(0, 0, renderer->width, renderer->height);

    return renderer;
}

void cs_render_destroy(cs_renderer *renderer) {
    if (!renderer) {
        return;
    }

    glDeleteBuffers(1, &renderer->vbo);
    glDeleteProgram(renderer->program);

    if (renderer->display != EGL_NO_DISPLAY) {
        eglMakeCurrent(renderer->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (renderer->context != EGL_NO_CONTEXT) {
            eglDestroyContext(renderer->display, renderer->context);
        }
        if (renderer->surface != EGL_NO_SURFACE) {
            eglDestroySurface(renderer->display, renderer->surface);
        }
        eglTerminate(renderer->display);
    }

    free(renderer);
}

int cs_render_frame(cs_renderer *renderer, uint8_t *rgba_out, size_t rgba_len) {
    if (!renderer || !rgba_out) {
        return -1;
    }

    size_t expected = (size_t)renderer->width * (size_t)renderer->height * 4;
    if (rgba_len < expected) {
        return -1;
    }

    renderer->angle += (float)(2.0 * M_PI) / (renderer->fps * 4.0f);

    glClearColor(0.05f, 0.07f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    mat4 proj = mat4_perspective(60.0f * (float)M_PI / 180.0f,
                                (float)renderer->width / (float)renderer->height,
                                0.1f, 100.0f);
    mat4 view = mat4_translate(0.0f, 0.0f, -5.0f);
    mat4 rot_y = mat4_rotate_y(renderer->angle);
    mat4 rot_x = mat4_rotate_x(renderer->angle * 0.7f);
    mat4 model = mat4_mul(rot_y, rot_x);
    mat4 mvp = mat4_mul(proj, mat4_mul(view, model));

    glUseProgram(renderer->program);
    glUniformMatrix4fv(renderer->loc_mvp, 1, GL_FALSE, mvp.m);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->vbo);

    glEnableVertexAttribArray((GLuint)renderer->loc_pos);
    glEnableVertexAttribArray((GLuint)renderer->loc_color);
    glVertexAttribPointer((GLuint)renderer->loc_pos, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *)0);
    glVertexAttribPointer((GLuint)renderer->loc_color, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *)(sizeof(float) * 3));

    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(sizeof(cube_vertices) / sizeof(vertex)));

    glDisableVertexAttribArray((GLuint)renderer->loc_pos);
    glDisableVertexAttribArray((GLuint)renderer->loc_color);

    glReadPixels(0, 0, renderer->width, renderer->height, GL_RGBA, GL_UNSIGNED_BYTE, rgba_out);

    return 0;
}
