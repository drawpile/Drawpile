/*
 * Copyright (c) 2022 askmeaboutloom
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "gl.h"
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/file.h>
#include <SDL.h>

#define DP_GL_DEFINE_FALLBACKS
#include <gles2_inc.h>


#define DP_GL_PROC_X(TYPE, NAME) TYPE NAME = DP_##NAME##_fallback;
DP_GL_PROC_LIST
#undef DP_GL_PROC_X

// Converting a void * to a function pointer isn't ISO C, so it triggers a
// pedantic warning. This here is a standards-compliant workaround apparently,
// according to my dlsym docs. It doesn't trigger the warning anyway.
#define DP_DL_ASSIGN(FUNC_PTR, VOID_PTR) *(void **)(&FUNC_PTR) = VOID_PTR

#define GET_GL_PROC(TYPE, NAME)                                        \
    do {                                                               \
        void *_proc = SDL_GL_GetProcAddress(#NAME);                    \
        if (_proc) {                                                   \
            DP_DL_ASSIGN(NAME, _proc);                                 \
        }                                                              \
        else {                                                         \
            DP_warn("did not find proc address for " #TYPE " " #NAME); \
        }                                                              \
    } while (0)

static void load_gl_procs(void)
{
    DP_debug("populating gl proc function pointers");
#define DP_GL_PROC_X(TYPE, NAME) GET_GL_PROC(TYPE, NAME);
    DP_GL_PROC_LIST
#undef DP_GL_PROC_X
    DP_GL_CLEAR_ERROR();
}

#ifdef NDEBUG
#    define DUMP_GL_STRING(NAME) /* nothing */
#else
#    define DUMP_GL_STRING(NAME)                                              \
        do {                                                                  \
            const unsigned char *_value = glGetString(NAME);                  \
            DP_GL_CLEAR_ERROR();                                              \
            DP_debug(#NAME ": %s", _value ? (const char *)_value : "(NULL)"); \
        } while (0)
#endif

static void dump_gl_info(void)
{
    DUMP_GL_STRING(GL_VENDOR);
    DUMP_GL_STRING(GL_RENDERER);
    DUMP_GL_STRING(GL_VERSION);
    DUMP_GL_STRING(GL_SHADING_LANGUAGE_VERSION);
}

bool DP_gl_init(void)
{
    load_gl_procs();
    dump_gl_info();
    return true;
}


void DP_gl_clear(float r, float g, float b, float a, float depth, int stencil)
{
    DP_GL_CLEAR_ERROR();
    DP_GL(glClearColor, r, g, b, a);
    DP_GL(glClearDepthf, depth);
    DP_GL(glClearStencil, stencil);
    DP_GL(glClear,
          GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}


static unsigned int parse_shader(unsigned int type, const char *source)
{
    unsigned int shader = glCreateShader(type);
    DP_GL_CLEAR_ERROR();
    if (shader == 0) {
        const char *type_name = type == GL_VERTEX_SHADER   ? "vertex"
                              : type == GL_FRAGMENT_SHADER ? "fragment"
                                                           : "unknown";
        DP_error_set("Can't create %s shader", type_name);
        return 0;
    }

    char *buffer = source[0] == '<' ? DP_file_slurp(source + 1, NULL) : NULL;
    const char *code = buffer ? buffer : source;
    DP_GL(glShaderSource, shader, 1, &code, NULL);
    DP_GL(glCompileShader, shader);
    DP_free(buffer);
    return shader;
}

static void dump_info_log(void GL_APIENTRY (*get_info_log)(unsigned int, int,
                                                           int *, char *),
                          unsigned int handle, int log_length)
{
    char *buffer = DP_malloc(DP_int_to_size(log_length));
    int buflen;
    DP_GL(get_info_log, handle, log_length, &buflen, buffer);
    if (buflen > 0) {
        DP_warn("%.*s", buflen, buffer);
    }
    DP_free(buffer);
}

static void dump_shader_info_log(unsigned int shader)
{
    int log_length;
    DP_GL(glGetShaderiv, shader, GL_INFO_LOG_LENGTH, &log_length);
    if (log_length > 0) {
        dump_info_log(glGetShaderInfoLog, shader, log_length);
    }
}

static int get_shader_compile_status(unsigned int shader)
{
    int compile_status;
    DP_GL(glGetShaderiv, shader, GL_COMPILE_STATUS, &compile_status);
    return compile_status;
}

unsigned int DP_gl_shader_new(unsigned int type, const char *source)
{
    DP_GL_CLEAR_ERROR();
    unsigned int shader = parse_shader(type, source);
    if (shader == 0) {
        return 0;
    }

    dump_shader_info_log(shader);
    if (get_shader_compile_status(shader)) {
        return shader;
    }
    else {
        DP_error_set("Can't compile shader: %s", source);
        DP_GL(glDeleteShader, shader);
        return 0;
    }
}


static void dump_program_info_log(unsigned int program)
{
    int log_length;
    DP_GL(glGetProgramiv, program, GL_INFO_LOG_LENGTH, &log_length);
    if (log_length > 0) {
        dump_info_log(glGetProgramInfoLog, program, log_length);
    }
}

static int get_program_link_status(unsigned int program)
{
    int link_status;
    DP_GL(glGetProgramiv, program, GL_LINK_STATUS, &link_status);
    return link_status;
}

unsigned int DP_gl_program_new(const char *vert, const char *frag)
{
    DP_GL_CLEAR_ERROR();
    unsigned int vshader = DP_gl_shader_new(GL_VERTEX_SHADER, vert);
    if (vshader == 0) {
        return 0;
    }

    unsigned int fshader = DP_gl_shader_new(GL_FRAGMENT_SHADER, frag);
    if (fshader == 0) {
        DP_GL(glDeleteShader, vshader);
        return 0;
    }

    unsigned int program = glCreateProgram();
    DP_GL_CLEAR_ERROR();
    if (program == 0) {
        DP_error_set("Can't create shader program");
        DP_GL(glDeleteShader, fshader);
        DP_GL(glDeleteShader, vshader);
        return 0;
    }

    DP_GL(glAttachShader, program, vshader);
    DP_GL(glAttachShader, program, fshader);
    DP_GL(glLinkProgram, program);

    dump_program_info_log(program);

    DP_GL(glDetachShader, program, vshader);
    DP_GL(glDetachShader, program, fshader);
    DP_GL(glDeleteShader, vshader);
    DP_GL(glDeleteShader, fshader);

    if (get_program_link_status(program)) {
        return program;
    }
    else {
        DP_error_set("Can't link program with '%s' and '%s'", vert, frag);
        DP_GL(glDeleteProgram, program);
        return 0;
    }
}


unsigned int DP_gl_get_error(void)
{
    return glGetError();
}

const char *DP_gl_strerror(unsigned int err)
{
    switch (err) {
    case GL_NO_ERROR:
        return "GL_NO_ERROR";
    case GL_INVALID_ENUM:
        return "GL_INVALID_ENUM";
    case GL_INVALID_VALUE:
        return "GL_INVALID_VALUE";
    case GL_INVALID_OPERATION:
        return "GL_INVALID_OPERATION";
    case GL_INVALID_FRAMEBUFFER_OPERATION:
        return "GL_INVALID_FRAMEBUFFER_OPERATION";
    case GL_OUT_OF_MEMORY:
        return "GL_OUT_OF_MEMORY";
    default:
        return "unknown GL error";
    }
}

void DP_gl_warn(unsigned int err, const char *where)
{
    DP_warn("OpenGL error %d (%s) in %s", err, DP_gl_strerror(err),
            where ? where : "");
}

void DP_gl_panic(unsigned int err, const char *where)
{
    DP_panic("OpenGL error %d (%s) in %s", err, DP_gl_strerror(err),
             where ? where : "");
}
