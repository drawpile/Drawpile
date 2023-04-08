/*
 * Copyright (c) 2019, 2020, 2021 askmeaboutloom
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

/*
 * Emscripten links to all OpenGL functions statically. While loading the
 * functions dynamically still works due to emulation thereof, they don't
 * recommend it for performance reasons, function pointers require additional
 * validation in WebAssembly. So we'll be special-casing this a bunch.
 */
#ifdef __EMSCRIPTEN__
#    define GL_GLES_PROTOTYPES 1
#else
#    define GL_GLES_PROTOTYPES 0
#endif

#include "gl2.h"
#include "gl2ext.h"

/*
 * These extension functions are always available in WebGL and Dear Imgui
 * makes use of them under Emscripten, so we include their prototypes here.
 */
#ifdef __EMSCRIPTEN__
#    ifdef __cplusplus
extern "C" {
#    endif

GL_APICALL void GL_APIENTRY glBindVertexArrayOES(GLuint array);
GL_APICALL void GL_APIENTRY glDeleteVertexArraysOES(GLsizei n,
                                                    const GLuint *arrays);
GL_APICALL void GL_APIENTRY glGenVertexArraysOES(GLsizei n, GLuint *arrays);
GL_APICALL GLboolean GL_APIENTRY glIsVertexArrayOES(GLuint array);

#    ifdef __cplusplus
}
#    endif
#endif

/* Don't load standard GL functions dynamically in Emscripten. */
#ifdef __EMSCRIPTEN__
#    define DP_GL_PROC_LIST
#else
#    define DP_GL_PROC_LIST                                                   \
        DP_GL_PROC_X(PFNGLACTIVETEXTUREPROC, glActiveTexture)                 \
        DP_GL_PROC_X(PFNGLATTACHSHADERPROC, glAttachShader)                   \
        DP_GL_PROC_X(PFNGLBINDATTRIBLOCATIONPROC, glBindAttribLocation)       \
        DP_GL_PROC_X(PFNGLBINDBUFFERPROC, glBindBuffer)                       \
        DP_GL_PROC_X(PFNGLBINDFRAMEBUFFERPROC, glBindFramebuffer)             \
        DP_GL_PROC_X(PFNGLBINDRENDERBUFFERPROC, glBindRenderbuffer)           \
        DP_GL_PROC_X(PFNGLBINDTEXTUREPROC, glBindTexture)                     \
        DP_GL_PROC_X(PFNGLBLENDCOLORPROC, glBlendColor)                       \
        DP_GL_PROC_X(PFNGLBLENDEQUATIONPROC, glBlendEquation)                 \
        DP_GL_PROC_X(PFNGLBLENDEQUATIONSEPARATEPROC, glBlendEquationSeparate) \
        DP_GL_PROC_X(PFNGLBLENDFUNCPROC, glBlendFunc)                         \
        DP_GL_PROC_X(PFNGLBLENDFUNCSEPARATEPROC, glBlendFuncSeparate)         \
        DP_GL_PROC_X(PFNGLBUFFERDATAPROC, glBufferData)                       \
        DP_GL_PROC_X(PFNGLBUFFERSUBDATAPROC, glBufferSubData)                 \
        DP_GL_PROC_X(PFNGLCHECKFRAMEBUFFERSTATUSPROC,                         \
                     glCheckFramebufferStatus)                                \
        DP_GL_PROC_X(PFNGLCLEARPROC, glClear)                                 \
        DP_GL_PROC_X(PFNGLCLEARCOLORPROC, glClearColor)                       \
        DP_GL_PROC_X(PFNGLCLEARDEPTHFPROC, glClearDepthf)                     \
        DP_GL_PROC_X(PFNGLCLEARSTENCILPROC, glClearStencil)                   \
        DP_GL_PROC_X(PFNGLCOLORMASKPROC, glColorMask)                         \
        DP_GL_PROC_X(PFNGLCOMPILESHADERPROC, glCompileShader)                 \
        DP_GL_PROC_X(PFNGLCOMPRESSEDTEXIMAGE2DPROC, glCompressedTexImage2D)   \
        DP_GL_PROC_X(PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC,                        \
                     glCompressedTexSubImage2D)                               \
        DP_GL_PROC_X(PFNGLCOPYTEXIMAGE2DPROC, glCopyTexImage2D)               \
        DP_GL_PROC_X(PFNGLCOPYTEXSUBIMAGE2DPROC, glCopyTexSubImage2D)         \
        DP_GL_PROC_X(PFNGLCREATEPROGRAMPROC, glCreateProgram)                 \
        DP_GL_PROC_X(PFNGLCREATESHADERPROC, glCreateShader)                   \
        DP_GL_PROC_X(PFNGLCULLFACEPROC, glCullFace)                           \
        DP_GL_PROC_X(PFNGLDELETEBUFFERSPROC, glDeleteBuffers)                 \
        DP_GL_PROC_X(PFNGLDELETEFRAMEBUFFERSPROC, glDeleteFramebuffers)       \
        DP_GL_PROC_X(PFNGLDELETEPROGRAMPROC, glDeleteProgram)                 \
        DP_GL_PROC_X(PFNGLDELETERENDERBUFFERSPROC, glDeleteRenderbuffers)     \
        DP_GL_PROC_X(PFNGLDELETESHADERPROC, glDeleteShader)                   \
        DP_GL_PROC_X(PFNGLDELETETEXTURESPROC, glDeleteTextures)               \
        DP_GL_PROC_X(PFNGLDEPTHFUNCPROC, glDepthFunc)                         \
        DP_GL_PROC_X(PFNGLDEPTHMASKPROC, glDepthMask)                         \
        DP_GL_PROC_X(PFNGLDEPTHRANGEFPROC, glDepthRangef)                     \
        DP_GL_PROC_X(PFNGLDETACHSHADERPROC, glDetachShader)                   \
        DP_GL_PROC_X(PFNGLDISABLEPROC, glDisable)                             \
        DP_GL_PROC_X(PFNGLDISABLEVERTEXATTRIBARRAYPROC,                       \
                     glDisableVertexAttribArray)                              \
        DP_GL_PROC_X(PFNGLDRAWARRAYSPROC, glDrawArrays)                       \
        DP_GL_PROC_X(PFNGLDRAWELEMENTSPROC, glDrawElements)                   \
        DP_GL_PROC_X(PFNGLENABLEPROC, glEnable)                               \
        DP_GL_PROC_X(PFNGLENABLEVERTEXATTRIBARRAYPROC,                        \
                     glEnableVertexAttribArray)                               \
        DP_GL_PROC_X(PFNGLFINISHPROC, glFinish)                               \
        DP_GL_PROC_X(PFNGLFLUSHPROC, glFlush)                                 \
        DP_GL_PROC_X(PFNGLFRAMEBUFFERRENDERBUFFERPROC,                        \
                     glFramebufferRenderbuffer)                               \
        DP_GL_PROC_X(PFNGLFRAMEBUFFERTEXTURE2DPROC, glFramebufferTexture2D)   \
        DP_GL_PROC_X(PFNGLFRONTFACEPROC, glFrontFace)                         \
        DP_GL_PROC_X(PFNGLGENBUFFERSPROC, glGenBuffers)                       \
        DP_GL_PROC_X(PFNGLGENERATEMIPMAPPROC, glGenerateMipmap)               \
        DP_GL_PROC_X(PFNGLGENFRAMEBUFFERSPROC, glGenFramebuffers)             \
        DP_GL_PROC_X(PFNGLGENRENDERBUFFERSPROC, glGenRenderbuffers)           \
        DP_GL_PROC_X(PFNGLGENTEXTURESPROC, glGenTextures)                     \
        DP_GL_PROC_X(PFNGLGETACTIVEATTRIBPROC, glGetActiveAttrib)             \
        DP_GL_PROC_X(PFNGLGETACTIVEUNIFORMPROC, glGetActiveUniform)           \
        DP_GL_PROC_X(PFNGLGETATTACHEDSHADERSPROC, glGetAttachedShaders)       \
        DP_GL_PROC_X(PFNGLGETATTRIBLOCATIONPROC, glGetAttribLocation)         \
        DP_GL_PROC_X(PFNGLGETBOOLEANVPROC, glGetBooleanv)                     \
        DP_GL_PROC_X(PFNGLGETBUFFERPARAMETERIVPROC, glGetBufferParameteriv)   \
        DP_GL_PROC_X(PFNGLGETERRORPROC, glGetError)                           \
        DP_GL_PROC_X(PFNGLGETFLOATVPROC, glGetFloatv)                         \
        DP_GL_PROC_X(PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC,            \
                     glGetFramebufferAttachmentParameteriv)                   \
        DP_GL_PROC_X(PFNGLGETINTEGERVPROC, glGetIntegerv)                     \
        DP_GL_PROC_X(PFNGLGETPROGRAMIVPROC, glGetProgramiv)                   \
        DP_GL_PROC_X(PFNGLGETPROGRAMINFOLOGPROC, glGetProgramInfoLog)         \
        DP_GL_PROC_X(PFNGLGETRENDERBUFFERPARAMETERIVPROC,                     \
                     glGetRenderbufferParameteriv)                            \
        DP_GL_PROC_X(PFNGLGETSHADERIVPROC, glGetShaderiv)                     \
        DP_GL_PROC_X(PFNGLGETSHADERINFOLOGPROC, glGetShaderInfoLog)           \
        DP_GL_PROC_X(PFNGLGETSHADERPRECISIONFORMATPROC,                       \
                     glGetShaderPrecisionFormat)                              \
        DP_GL_PROC_X(PFNGLGETSHADERSOURCEPROC, glGetShaderSource)             \
        DP_GL_PROC_X(PFNGLGETSTRINGPROC, glGetString)                         \
        DP_GL_PROC_X(PFNGLGETTEXPARAMETERFVPROC, glGetTexParameterfv)         \
        DP_GL_PROC_X(PFNGLGETTEXPARAMETERIVPROC, glGetTexParameteriv)         \
        DP_GL_PROC_X(PFNGLGETUNIFORMFVPROC, glGetUniformfv)                   \
        DP_GL_PROC_X(PFNGLGETUNIFORMIVPROC, glGetUniformiv)                   \
        DP_GL_PROC_X(PFNGLGETUNIFORMLOCATIONPROC, glGetUniformLocation)       \
        DP_GL_PROC_X(PFNGLGETVERTEXATTRIBFVPROC, glGetVertexAttribfv)         \
        DP_GL_PROC_X(PFNGLGETVERTEXATTRIBIVPROC, glGetVertexAttribiv)         \
        DP_GL_PROC_X(PFNGLGETVERTEXATTRIBPOINTERVPROC,                        \
                     glGetVertexAttribPointerv)                               \
        DP_GL_PROC_X(PFNGLHINTPROC, glHint)                                   \
        DP_GL_PROC_X(PFNGLISBUFFERPROC, glIsBuffer)                           \
        DP_GL_PROC_X(PFNGLISENABLEDPROC, glIsEnabled)                         \
        DP_GL_PROC_X(PFNGLISFRAMEBUFFERPROC, glIsFramebuffer)                 \
        DP_GL_PROC_X(PFNGLISPROGRAMPROC, glIsProgram)                         \
        DP_GL_PROC_X(PFNGLISRENDERBUFFERPROC, glIsRenderbuffer)               \
        DP_GL_PROC_X(PFNGLISSHADERPROC, glIsShader)                           \
        DP_GL_PROC_X(PFNGLISTEXTUREPROC, glIsTexture)                         \
        DP_GL_PROC_X(PFNGLLINEWIDTHPROC, glLineWidth)                         \
        DP_GL_PROC_X(PFNGLLINKPROGRAMPROC, glLinkProgram)                     \
        DP_GL_PROC_X(PFNGLPIXELSTOREIPROC, glPixelStorei)                     \
        DP_GL_PROC_X(PFNGLPOLYGONOFFSETPROC, glPolygonOffset)                 \
        DP_GL_PROC_X(PFNGLREADPIXELSPROC, glReadPixels)                       \
        DP_GL_PROC_X(PFNGLRELEASESHADERCOMPILERPROC, glReleaseShaderCompiler) \
        DP_GL_PROC_X(PFNGLRENDERBUFFERSTORAGEPROC, glRenderbufferStorage)     \
        DP_GL_PROC_X(PFNGLSAMPLECOVERAGEPROC, glSampleCoverage)               \
        DP_GL_PROC_X(PFNGLSCISSORPROC, glScissor)                             \
        DP_GL_PROC_X(PFNGLSHADERBINARYPROC, glShaderBinary)                   \
        DP_GL_PROC_X(PFNGLSHADERSOURCEPROC, glShaderSource)                   \
        DP_GL_PROC_X(PFNGLSTENCILFUNCPROC, glStencilFunc)                     \
        DP_GL_PROC_X(PFNGLSTENCILFUNCSEPARATEPROC, glStencilFuncSeparate)     \
        DP_GL_PROC_X(PFNGLSTENCILMASKPROC, glStencilMask)                     \
        DP_GL_PROC_X(PFNGLSTENCILMASKSEPARATEPROC, glStencilMaskSeparate)     \
        DP_GL_PROC_X(PFNGLSTENCILOPPROC, glStencilOp)                         \
        DP_GL_PROC_X(PFNGLSTENCILOPSEPARATEPROC, glStencilOpSeparate)         \
        DP_GL_PROC_X(PFNGLTEXIMAGE2DPROC, glTexImage2D)                       \
        DP_GL_PROC_X(PFNGLTEXPARAMETERFPROC, glTexParameterf)                 \
        DP_GL_PROC_X(PFNGLTEXPARAMETERFVPROC, glTexParameterfv)               \
        DP_GL_PROC_X(PFNGLTEXPARAMETERIPROC, glTexParameteri)                 \
        DP_GL_PROC_X(PFNGLTEXPARAMETERIVPROC, glTexParameteriv)               \
        DP_GL_PROC_X(PFNGLTEXSUBIMAGE2DPROC, glTexSubImage2D)                 \
        DP_GL_PROC_X(PFNGLUNIFORM1FPROC, glUniform1f)                         \
        DP_GL_PROC_X(PFNGLUNIFORM1FVPROC, glUniform1fv)                       \
        DP_GL_PROC_X(PFNGLUNIFORM1IPROC, glUniform1i)                         \
        DP_GL_PROC_X(PFNGLUNIFORM1IVPROC, glUniform1iv)                       \
        DP_GL_PROC_X(PFNGLUNIFORM2FPROC, glUniform2f)                         \
        DP_GL_PROC_X(PFNGLUNIFORM2FVPROC, glUniform2fv)                       \
        DP_GL_PROC_X(PFNGLUNIFORM2IPROC, glUniform2i)                         \
        DP_GL_PROC_X(PFNGLUNIFORM2IVPROC, glUniform2iv)                       \
        DP_GL_PROC_X(PFNGLUNIFORM3FPROC, glUniform3f)                         \
        DP_GL_PROC_X(PFNGLUNIFORM3FVPROC, glUniform3fv)                       \
        DP_GL_PROC_X(PFNGLUNIFORM3IPROC, glUniform3i)                         \
        DP_GL_PROC_X(PFNGLUNIFORM3IVPROC, glUniform3iv)                       \
        DP_GL_PROC_X(PFNGLUNIFORM4FPROC, glUniform4f)                         \
        DP_GL_PROC_X(PFNGLUNIFORM4FVPROC, glUniform4fv)                       \
        DP_GL_PROC_X(PFNGLUNIFORM4IPROC, glUniform4i)                         \
        DP_GL_PROC_X(PFNGLUNIFORM4IVPROC, glUniform4iv)                       \
        DP_GL_PROC_X(PFNGLUNIFORMMATRIX2FVPROC, glUniformMatrix2fv)           \
        DP_GL_PROC_X(PFNGLUNIFORMMATRIX3FVPROC, glUniformMatrix3fv)           \
        DP_GL_PROC_X(PFNGLUNIFORMMATRIX4FVPROC, glUniformMatrix4fv)           \
        DP_GL_PROC_X(PFNGLUSEPROGRAMPROC, glUseProgram)                       \
        DP_GL_PROC_X(PFNGLVALIDATEPROGRAMPROC, glValidateProgram)             \
        DP_GL_PROC_X(PFNGLVERTEXATTRIB1FPROC, glVertexAttrib1f)               \
        DP_GL_PROC_X(PFNGLVERTEXATTRIB1FVPROC, glVertexAttrib1fv)             \
        DP_GL_PROC_X(PFNGLVERTEXATTRIB2FPROC, glVertexAttrib2f)               \
        DP_GL_PROC_X(PFNGLVERTEXATTRIB2FVPROC, glVertexAttrib2fv)             \
        DP_GL_PROC_X(PFNGLVERTEXATTRIB3FPROC, glVertexAttrib3f)               \
        DP_GL_PROC_X(PFNGLVERTEXATTRIB3FVPROC, glVertexAttrib3fv)             \
        DP_GL_PROC_X(PFNGLVERTEXATTRIB4FPROC, glVertexAttrib4f)               \
        DP_GL_PROC_X(PFNGLVERTEXATTRIB4FVPROC, glVertexAttrib4fv)             \
        DP_GL_PROC_X(PFNGLVERTEXATTRIBPOINTERPROC, glVertexAttribPointer)     \
        DP_GL_PROC_X(PFNGLVIEWPORTPROC, glViewport)
#endif

#define DP_GL_PROC_X(TYPE, NAME) extern TYPE NAME;
DP_GL_PROC_LIST
#undef DP_GL_PROC_X

#ifdef DP_GL_DEFINE_FALLBACKS
#    define DP_GL_DEFINE_FALLBACK(TYPE, NAME, ARGS, RETVAL)       \
        static TYPE GL_APIENTRY DP_##NAME##_fallback ARGS         \
        {                                                         \
            DP_warn("OpenGL procedure " #NAME " was not loaded"); \
            return RETVAL;                                        \
        }

#    ifdef __EMSCRIPTEN__
#        define DP_GL_FALLBACK(...) /* nothing, these are linked statically */
#    else
#        define DP_GL_FALLBACK(...) DP_GL_DEFINE_FALLBACK(__VA_ARGS__)
#    endif

DP_GL_FALLBACK(void, glActiveTexture, (DP_UNUSED GLenum texture), )
DP_GL_FALLBACK(void, glAttachShader,
               (DP_UNUSED GLuint program, DP_UNUSED GLuint shader), )
DP_GL_FALLBACK(void, glBindAttribLocation,
               (DP_UNUSED GLuint program, DP_UNUSED GLuint index,
                DP_UNUSED const GLchar *name), )
DP_GL_FALLBACK(void, glBindBuffer,
               (DP_UNUSED GLenum target, DP_UNUSED GLuint buffer), )
DP_GL_FALLBACK(void, glBindFramebuffer,
               (DP_UNUSED GLenum target, DP_UNUSED GLuint framebuffer), )
DP_GL_FALLBACK(void, glBindRenderbuffer,
               (DP_UNUSED GLenum target, DP_UNUSED GLuint renderbuffer), )
DP_GL_FALLBACK(void, glBindTexture,
               (DP_UNUSED GLenum target, DP_UNUSED GLuint texture), )
DP_GL_FALLBACK(void, glBlendColor,
               (DP_UNUSED GLfloat red, DP_UNUSED GLfloat green,
                DP_UNUSED GLfloat blue, DP_UNUSED GLfloat alpha), )
DP_GL_FALLBACK(void, glBlendEquation, (DP_UNUSED GLenum mode), )
DP_GL_FALLBACK(void, glBlendEquationSeparate,
               (DP_UNUSED GLenum modeRGB, DP_UNUSED GLenum modeAlpha), )
DP_GL_FALLBACK(void, glBlendFunc,
               (DP_UNUSED GLenum sfactor, DP_UNUSED GLenum dfactor), )
DP_GL_FALLBACK(void, glBlendFuncSeparate,
               (DP_UNUSED GLenum sfactorRGB, DP_UNUSED GLenum dfactorRGB,
                DP_UNUSED GLenum sfactorAlpha, DP_UNUSED GLenum dfactorAlpha), )
DP_GL_FALLBACK(void, glBufferData,
               (DP_UNUSED GLenum target, DP_UNUSED GLsizeiptr size,
                DP_UNUSED const void *data, DP_UNUSED GLenum usage), )
DP_GL_FALLBACK(void, glBufferSubData,
               (DP_UNUSED GLenum target, DP_UNUSED GLintptr offset,
                DP_UNUSED GLsizeiptr size, DP_UNUSED const void *data), )
DP_GL_FALLBACK(GLenum, glCheckFramebufferStatus, (DP_UNUSED GLenum target), 0)
DP_GL_FALLBACK(void, glClear, (DP_UNUSED GLbitfield mask), )
DP_GL_FALLBACK(void, glClearColor,
               (DP_UNUSED GLfloat red, DP_UNUSED GLfloat green,
                DP_UNUSED GLfloat blue, DP_UNUSED GLfloat alpha), )
DP_GL_FALLBACK(void, glClearDepthf, (DP_UNUSED GLfloat d), )
DP_GL_FALLBACK(void, glClearStencil, (DP_UNUSED GLint s), )
DP_GL_FALLBACK(void, glColorMask,
               (DP_UNUSED GLboolean red, DP_UNUSED GLboolean green,
                DP_UNUSED GLboolean blue, DP_UNUSED GLboolean alpha), )
DP_GL_FALLBACK(void, glCompileShader, (DP_UNUSED GLuint shader), )
DP_GL_FALLBACK(void, glCompressedTexImage2D,
               (DP_UNUSED GLenum target, DP_UNUSED GLint level,
                DP_UNUSED GLenum internalformat, DP_UNUSED GLsizei width,
                DP_UNUSED GLsizei height, DP_UNUSED GLint border,
                DP_UNUSED GLsizei imageSize, DP_UNUSED const void *data), )
DP_GL_FALLBACK(void, glCompressedTexSubImage2D,
               (DP_UNUSED GLenum target, DP_UNUSED GLint level,
                DP_UNUSED GLint xoffset, DP_UNUSED GLint yoffset,
                DP_UNUSED GLsizei width, DP_UNUSED GLsizei height,
                DP_UNUSED GLenum format, DP_UNUSED GLsizei imageSize,
                DP_UNUSED const void *data), )
DP_GL_FALLBACK(void, glCopyTexImage2D,
               (DP_UNUSED GLenum target, DP_UNUSED GLint level,
                DP_UNUSED GLenum internalformat, DP_UNUSED GLint x,
                DP_UNUSED GLint y, DP_UNUSED GLsizei width,
                DP_UNUSED GLsizei height, DP_UNUSED GLint border), )
DP_GL_FALLBACK(void, glCopyTexSubImage2D,
               (DP_UNUSED GLenum target, DP_UNUSED GLint level,
                DP_UNUSED GLint xoffset, DP_UNUSED GLint yoffset,
                DP_UNUSED GLint x, DP_UNUSED GLint y, DP_UNUSED GLsizei width,
                DP_UNUSED GLsizei height), )
DP_GL_FALLBACK(GLuint, glCreateProgram, (void), 0)
DP_GL_FALLBACK(GLuint, glCreateShader, (DP_UNUSED GLenum type), 0)
DP_GL_FALLBACK(void, glCullFace, (DP_UNUSED GLenum mode), )
DP_GL_FALLBACK(void, glDeleteBuffers,
               (DP_UNUSED GLsizei n, DP_UNUSED const GLuint *buffers), )
DP_GL_FALLBACK(void, glDeleteFramebuffers,
               (DP_UNUSED GLsizei n, DP_UNUSED const GLuint *framebuffers), )
DP_GL_FALLBACK(void, glDeleteProgram, (DP_UNUSED GLuint program), )
DP_GL_FALLBACK(void, glDeleteRenderbuffers,
               (DP_UNUSED GLsizei n, DP_UNUSED const GLuint *renderbuffers), )
DP_GL_FALLBACK(void, glDeleteShader, (DP_UNUSED GLuint shader), )
DP_GL_FALLBACK(void, glDeleteTextures,
               (DP_UNUSED GLsizei n, DP_UNUSED const GLuint *textures), )
DP_GL_FALLBACK(void, glDepthFunc, (DP_UNUSED GLenum func), )
DP_GL_FALLBACK(void, glDepthMask, (DP_UNUSED GLboolean flag), )
DP_GL_FALLBACK(void, glDepthRangef,
               (DP_UNUSED GLfloat n, DP_UNUSED GLfloat f), )
DP_GL_FALLBACK(void, glDetachShader,
               (DP_UNUSED GLuint program, DP_UNUSED GLuint shader), )
DP_GL_FALLBACK(void, glDisable, (DP_UNUSED GLenum cap), )
DP_GL_FALLBACK(void, glDisableVertexAttribArray, (DP_UNUSED GLuint index), )
DP_GL_FALLBACK(void, glDrawArrays,
               (DP_UNUSED GLenum mode, DP_UNUSED GLint first,
                DP_UNUSED GLsizei count), )
DP_GL_FALLBACK(void, glDrawElements,
               (DP_UNUSED GLenum mode, DP_UNUSED GLsizei count,
                DP_UNUSED GLenum type, DP_UNUSED const void *indices), )
DP_GL_FALLBACK(void, glEnable, (DP_UNUSED GLenum cap), )
DP_GL_FALLBACK(void, glEnableVertexAttribArray, (DP_UNUSED GLuint index), )
DP_GL_FALLBACK(void, glFinish, (void), )
DP_GL_FALLBACK(void, glFlush, (void), )
DP_GL_FALLBACK(void, glFramebufferRenderbuffer,
               (DP_UNUSED GLenum target, DP_UNUSED GLenum attachment,
                DP_UNUSED GLenum renderbuffertarget,
                DP_UNUSED GLuint renderbuffer), )
DP_GL_FALLBACK(void, glFramebufferTexture2D,
               (DP_UNUSED GLenum target, DP_UNUSED GLenum attachment,
                DP_UNUSED GLenum textarget, DP_UNUSED GLuint texture,
                DP_UNUSED GLint level), )
DP_GL_FALLBACK(void, glFrontFace, (DP_UNUSED GLenum mode), )
DP_GL_FALLBACK(void, glGenBuffers,
               (DP_UNUSED GLsizei n, DP_UNUSED GLuint *buffers), )
DP_GL_FALLBACK(void, glGenerateMipmap, (DP_UNUSED GLenum target), )
DP_GL_FALLBACK(void, glGenFramebuffers,
               (DP_UNUSED GLsizei n, DP_UNUSED GLuint *framebuffers), )
DP_GL_FALLBACK(void, glGenRenderbuffers,
               (DP_UNUSED GLsizei n, DP_UNUSED GLuint *renderbuffers), )
DP_GL_FALLBACK(void, glGenTextures,
               (DP_UNUSED GLsizei n, DP_UNUSED GLuint *textures), )
DP_GL_FALLBACK(void, glGetActiveAttrib,
               (DP_UNUSED GLuint program, DP_UNUSED GLuint index,
                DP_UNUSED GLsizei bufSize, DP_UNUSED GLsizei *length,
                DP_UNUSED GLint *size, DP_UNUSED GLenum *type,
                DP_UNUSED GLchar *name), )
DP_GL_FALLBACK(void, glGetActiveUniform,
               (DP_UNUSED GLuint program, DP_UNUSED GLuint index,
                DP_UNUSED GLsizei bufSize, DP_UNUSED GLsizei *length,
                DP_UNUSED GLint *size, DP_UNUSED GLenum *type,
                DP_UNUSED GLchar *name), )
DP_GL_FALLBACK(void, glGetAttachedShaders,
               (DP_UNUSED GLuint program, DP_UNUSED GLsizei maxCount,
                DP_UNUSED GLsizei *count, DP_UNUSED GLuint *shaders), )
DP_GL_FALLBACK(GLint, glGetAttribLocation,
               (DP_UNUSED GLuint program, DP_UNUSED const GLchar *name), -1)
DP_GL_FALLBACK(void, glGetBooleanv,
               (DP_UNUSED GLenum pname, DP_UNUSED GLboolean *data), )
DP_GL_FALLBACK(void, glGetBufferParameteriv,
               (DP_UNUSED GLenum target, DP_UNUSED GLenum pname,
                DP_UNUSED GLint *params), )
DP_GL_FALLBACK(GLenum, glGetError, (void), GL_NO_ERROR)
DP_GL_FALLBACK(void, glGetFloatv,
               (DP_UNUSED GLenum pname, DP_UNUSED GLfloat *data), )
DP_GL_FALLBACK(void, glGetFramebufferAttachmentParameteriv,
               (DP_UNUSED GLenum target, DP_UNUSED GLenum attachment,
                DP_UNUSED GLenum pname, DP_UNUSED GLint *params), )
DP_GL_FALLBACK(void, glGetIntegerv,
               (DP_UNUSED GLenum pname, DP_UNUSED GLint *data), )
DP_GL_FALLBACK(void, glGetProgramiv,
               (DP_UNUSED GLuint program, DP_UNUSED GLenum pname,
                DP_UNUSED GLint *params), )
DP_GL_FALLBACK(void, glGetProgramInfoLog,
               (DP_UNUSED GLuint program, DP_UNUSED GLsizei bufSize,
                DP_UNUSED GLsizei *length, DP_UNUSED GLchar *infoLog), )
DP_GL_FALLBACK(void, glGetRenderbufferParameteriv,
               (DP_UNUSED GLenum target, DP_UNUSED GLenum pname,
                DP_UNUSED GLint *params), )
DP_GL_FALLBACK(void, glGetShaderiv,
               (DP_UNUSED GLuint shader, DP_UNUSED GLenum pname,
                DP_UNUSED GLint *params), )
DP_GL_FALLBACK(void, glGetShaderInfoLog,
               (DP_UNUSED GLuint shader, DP_UNUSED GLsizei bufSize,
                DP_UNUSED GLsizei *length, DP_UNUSED GLchar *infoLog), )
DP_GL_FALLBACK(void, glGetShaderPrecisionFormat,
               (DP_UNUSED GLenum shadertype, DP_UNUSED GLenum precisiontype,
                DP_UNUSED GLint *range, DP_UNUSED GLint *precision), )
DP_GL_FALLBACK(void, glGetShaderSource,
               (DP_UNUSED GLuint shader, DP_UNUSED GLsizei bufSize,
                DP_UNUSED GLsizei *length, DP_UNUSED GLchar *source), )
DP_GL_FALLBACK(const GLubyte *GL_APIENTRY, glGetString, (DP_UNUSED GLenum name),
               NULL)
DP_GL_FALLBACK(void, glGetTexParameterfv,
               (DP_UNUSED GLenum target, DP_UNUSED GLenum pname,
                DP_UNUSED GLfloat *params), )
DP_GL_FALLBACK(void, glGetTexParameteriv,
               (DP_UNUSED GLenum target, DP_UNUSED GLenum pname,
                DP_UNUSED GLint *params), )
DP_GL_FALLBACK(void, glGetUniformfv,
               (DP_UNUSED GLuint program, DP_UNUSED GLint location,
                DP_UNUSED GLfloat *params), )
DP_GL_FALLBACK(void, glGetUniformiv,
               (DP_UNUSED GLuint program, DP_UNUSED GLint location,
                DP_UNUSED GLint *params), )
DP_GL_FALLBACK(GLint, glGetUniformLocation,
               (DP_UNUSED GLuint program, DP_UNUSED const GLchar *name), -1)
DP_GL_FALLBACK(void, glGetVertexAttribfv,
               (DP_UNUSED GLuint index, DP_UNUSED GLenum pname,
                DP_UNUSED GLfloat *params), )
DP_GL_FALLBACK(void, glGetVertexAttribiv,
               (DP_UNUSED GLuint index, DP_UNUSED GLenum pname,
                DP_UNUSED GLint *params), )
DP_GL_FALLBACK(void, glGetVertexAttribPointerv,
               (DP_UNUSED GLuint index, DP_UNUSED GLenum pname,
                DP_UNUSED void **pointer), )
DP_GL_FALLBACK(void, glHint, (DP_UNUSED GLenum target, DP_UNUSED GLenum mode), )
DP_GL_FALLBACK(GLboolean, glIsBuffer, (DP_UNUSED GLuint buffer), GL_FALSE)
DP_GL_FALLBACK(GLboolean, glIsEnabled, (DP_UNUSED GLenum cap), GL_FALSE)
DP_GL_FALLBACK(GLboolean, glIsFramebuffer, (DP_UNUSED GLuint framebuffer),
               GL_FALSE)
DP_GL_FALLBACK(GLboolean, glIsProgram, (DP_UNUSED GLuint program), GL_FALSE)
DP_GL_FALLBACK(GLboolean, glIsRenderbuffer, (DP_UNUSED GLuint renderbuffer),
               GL_FALSE)
DP_GL_FALLBACK(GLboolean, glIsShader, (DP_UNUSED GLuint shader), GL_FALSE)
DP_GL_FALLBACK(GLboolean, glIsTexture, (DP_UNUSED GLuint texture), GL_FALSE)
DP_GL_FALLBACK(void, glLineWidth, (DP_UNUSED GLfloat width), )
DP_GL_FALLBACK(void, glLinkProgram, (DP_UNUSED GLuint program), )
DP_GL_FALLBACK(void, glPixelStorei,
               (DP_UNUSED GLenum pname, DP_UNUSED GLint param), )
DP_GL_FALLBACK(void, glPolygonOffset,
               (DP_UNUSED GLfloat factor, DP_UNUSED GLfloat units), )
DP_GL_FALLBACK(void, glReadPixels,
               (DP_UNUSED GLint x, DP_UNUSED GLint y, DP_UNUSED GLsizei width,
                DP_UNUSED GLsizei height, DP_UNUSED GLenum format,
                DP_UNUSED GLenum type, DP_UNUSED void *pixels), )
DP_GL_FALLBACK(void, glReleaseShaderCompiler, (void), )
DP_GL_FALLBACK(void, glRenderbufferStorage,
               (DP_UNUSED GLenum target, DP_UNUSED GLenum internalformat,
                DP_UNUSED GLsizei width, DP_UNUSED GLsizei height), )
DP_GL_FALLBACK(void, glSampleCoverage,
               (DP_UNUSED GLfloat value, DP_UNUSED GLboolean invert), )
DP_GL_FALLBACK(void, glScissor,
               (DP_UNUSED GLint x, DP_UNUSED GLint y, DP_UNUSED GLsizei width,
                DP_UNUSED GLsizei height), )
DP_GL_FALLBACK(void, glShaderBinary,
               (DP_UNUSED GLsizei count, DP_UNUSED const GLuint *shaders,
                DP_UNUSED GLenum binaryformat, DP_UNUSED const void *binary,
                DP_UNUSED GLsizei length), )
DP_GL_FALLBACK(void, glShaderSource,
               (DP_UNUSED GLuint shader, DP_UNUSED GLsizei count,
                DP_UNUSED const GLchar *const *string,
                DP_UNUSED const GLint *length), )
DP_GL_FALLBACK(void, glStencilFunc,
               (DP_UNUSED GLenum func, DP_UNUSED GLint ref,
                DP_UNUSED GLuint mask), )
DP_GL_FALLBACK(void, glStencilFuncSeparate,
               (DP_UNUSED GLenum face, DP_UNUSED GLenum func,
                DP_UNUSED GLint ref, DP_UNUSED GLuint mask), )
DP_GL_FALLBACK(void, glStencilMask, (DP_UNUSED GLuint mask), )
DP_GL_FALLBACK(void, glStencilMaskSeparate,
               (DP_UNUSED GLenum face, DP_UNUSED GLuint mask), )
DP_GL_FALLBACK(void, glStencilOp,
               (DP_UNUSED GLenum fail, DP_UNUSED GLenum zfail,
                DP_UNUSED GLenum zpass), )
DP_GL_FALLBACK(void, glStencilOpSeparate,
               (DP_UNUSED GLenum face, DP_UNUSED GLenum sfail,
                DP_UNUSED GLenum dpfail, DP_UNUSED GLenum dppass), )
DP_GL_FALLBACK(void, glTexImage2D,
               (DP_UNUSED GLenum target, DP_UNUSED GLint level,
                DP_UNUSED GLint internalformat, DP_UNUSED GLsizei width,
                DP_UNUSED GLsizei height, DP_UNUSED GLint border,
                DP_UNUSED GLenum format, DP_UNUSED GLenum type,
                DP_UNUSED const void *pixels), )
DP_GL_FALLBACK(void, glTexParameterf,
               (DP_UNUSED GLenum target, DP_UNUSED GLenum pname,
                DP_UNUSED GLfloat param), )
DP_GL_FALLBACK(void, glTexParameterfv,
               (DP_UNUSED GLenum target, DP_UNUSED GLenum pname,
                DP_UNUSED const GLfloat *params), )
DP_GL_FALLBACK(void, glTexParameteri,
               (DP_UNUSED GLenum target, DP_UNUSED GLenum pname,
                DP_UNUSED GLint param), )
DP_GL_FALLBACK(void, glTexParameteriv,
               (DP_UNUSED GLenum target, DP_UNUSED GLenum pname,
                DP_UNUSED const GLint *params), )
DP_GL_FALLBACK(void, glTexSubImage2D,
               (DP_UNUSED GLenum target, DP_UNUSED GLint level,
                DP_UNUSED GLint xoffset, DP_UNUSED GLint yoffset,
                DP_UNUSED GLsizei width, DP_UNUSED GLsizei height,
                DP_UNUSED GLenum format, DP_UNUSED GLenum type,
                DP_UNUSED const void *pixels), )
DP_GL_FALLBACK(void, glUniform1f,
               (DP_UNUSED GLint location, DP_UNUSED GLfloat v0), )
DP_GL_FALLBACK(void, glUniform1fv,
               (DP_UNUSED GLint location, DP_UNUSED GLsizei count,
                DP_UNUSED const GLfloat *value), )
DP_GL_FALLBACK(void, glUniform1i,
               (DP_UNUSED GLint location, DP_UNUSED GLint v0), )
DP_GL_FALLBACK(void, glUniform1iv,
               (DP_UNUSED GLint location, DP_UNUSED GLsizei count,
                DP_UNUSED const GLint *value), )
DP_GL_FALLBACK(void, glUniform2f,
               (DP_UNUSED GLint location, DP_UNUSED GLfloat v0,
                DP_UNUSED GLfloat v1), )
DP_GL_FALLBACK(void, glUniform2fv,
               (DP_UNUSED GLint location, DP_UNUSED GLsizei count,
                DP_UNUSED const GLfloat *value), )
DP_GL_FALLBACK(void, glUniform2i,
               (DP_UNUSED GLint location, DP_UNUSED GLint v0,
                DP_UNUSED GLint v1), )
DP_GL_FALLBACK(void, glUniform2iv,
               (DP_UNUSED GLint location, DP_UNUSED GLsizei count,
                DP_UNUSED const GLint *value), )
DP_GL_FALLBACK(void, glUniform3f,
               (DP_UNUSED GLint location, DP_UNUSED GLfloat v0,
                DP_UNUSED GLfloat v1, DP_UNUSED GLfloat v2), )
DP_GL_FALLBACK(void, glUniform3fv,
               (DP_UNUSED GLint location, DP_UNUSED GLsizei count,
                DP_UNUSED const GLfloat *value), )
DP_GL_FALLBACK(void, glUniform3i,
               (DP_UNUSED GLint location, DP_UNUSED GLint v0,
                DP_UNUSED GLint v1, DP_UNUSED GLint v2), )
DP_GL_FALLBACK(void, glUniform3iv,
               (DP_UNUSED GLint location, DP_UNUSED GLsizei count,
                DP_UNUSED const GLint *value), )
DP_GL_FALLBACK(void, glUniform4f,
               (DP_UNUSED GLint location, DP_UNUSED GLfloat v0,
                DP_UNUSED GLfloat v1, DP_UNUSED GLfloat v2,
                DP_UNUSED GLfloat v3), )
DP_GL_FALLBACK(void, glUniform4fv,
               (DP_UNUSED GLint location, DP_UNUSED GLsizei count,
                DP_UNUSED const GLfloat *value), )
DP_GL_FALLBACK(void, glUniform4i,
               (DP_UNUSED GLint location, DP_UNUSED GLint v0,
                DP_UNUSED GLint v1, DP_UNUSED GLint v2, DP_UNUSED GLint v3), )
DP_GL_FALLBACK(void, glUniform4iv,
               (DP_UNUSED GLint location, DP_UNUSED GLsizei count,
                DP_UNUSED const GLint *value), )
DP_GL_FALLBACK(void, glUniformMatrix2fv,
               (DP_UNUSED GLint location, DP_UNUSED GLsizei count,
                DP_UNUSED GLboolean transpose,
                DP_UNUSED const GLfloat *value), )
DP_GL_FALLBACK(void, glUniformMatrix3fv,
               (DP_UNUSED GLint location, DP_UNUSED GLsizei count,
                DP_UNUSED GLboolean transpose,
                DP_UNUSED const GLfloat *value), )
DP_GL_FALLBACK(void, glUniformMatrix4fv,
               (DP_UNUSED GLint location, DP_UNUSED GLsizei count,
                DP_UNUSED GLboolean transpose,
                DP_UNUSED const GLfloat *value), )
DP_GL_FALLBACK(void, glUseProgram, (DP_UNUSED GLuint program), )
DP_GL_FALLBACK(void, glValidateProgram, (DP_UNUSED GLuint program), )
DP_GL_FALLBACK(void, glVertexAttrib1f,
               (DP_UNUSED GLuint index, DP_UNUSED GLfloat x), )
DP_GL_FALLBACK(void, glVertexAttrib1fv,
               (DP_UNUSED GLuint index, DP_UNUSED const GLfloat *v), )
DP_GL_FALLBACK(void, glVertexAttrib2f,
               (DP_UNUSED GLuint index, DP_UNUSED GLfloat x,
                DP_UNUSED GLfloat y), )
DP_GL_FALLBACK(void, glVertexAttrib2fv,
               (DP_UNUSED GLuint index, DP_UNUSED const GLfloat *v), )
DP_GL_FALLBACK(void, glVertexAttrib3f,
               (DP_UNUSED GLuint index, DP_UNUSED GLfloat x,
                DP_UNUSED GLfloat y, DP_UNUSED GLfloat z), )
DP_GL_FALLBACK(void, glVertexAttrib3fv,
               (DP_UNUSED GLuint index, DP_UNUSED const GLfloat *v), )
DP_GL_FALLBACK(void, glVertexAttrib4f,
               (DP_UNUSED GLuint index, DP_UNUSED GLfloat x,
                DP_UNUSED GLfloat y, DP_UNUSED GLfloat z,
                DP_UNUSED GLfloat w), )
DP_GL_FALLBACK(void, glVertexAttrib4fv,
               (DP_UNUSED GLuint index, DP_UNUSED const GLfloat *v), )
DP_GL_FALLBACK(void, glVertexAttribPointer,
               (DP_UNUSED GLuint index, DP_UNUSED GLint size,
                DP_UNUSED GLenum type, DP_UNUSED GLboolean normalized,
                DP_UNUSED GLsizei stride, DP_UNUSED const void *pointer), )
DP_GL_FALLBACK(void, glViewport,
               (DP_UNUSED GLint x, DP_UNUSED GLint y, DP_UNUSED GLsizei width,
                DP_UNUSED GLsizei height), )
#    undef DP_GL_DEFINE_FALLBACK
#endif
