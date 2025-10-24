// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/view/glcanvas.h"
#include "desktop/main.h"
#include "desktop/settings.h"
#include "desktop/view/canvascontroller.h"
#include "desktop/view/canvasscene.h"
#include "libclient/canvas/tilecache.h"
#include "libclient/drawdance/perf.h"
#include <QMetaEnum>
#include <QOpenGLExtraFunctions>
#include <QOpenGLFunctions>
#include <QPaintEvent>
#include <QPainter>

#define DP_PERF_CONTEXT "glcanvas"

Q_LOGGING_CATEGORY(lcDpGlCanvas, "net.drawpile.view.canvas.gl", QtWarningMsg)

namespace view {

QStringList GlCanvas::systemInfo;

struct GlCanvas::Private {
	static constexpr int BUFFER_COUNT = 2;
	static constexpr int CANVAS_UV_BUFFER_INDEX = 0;
	static constexpr int OUTLINE_VERTEX_BUFFER_INDEX = 1;
	static constexpr int VERTEX_COUNT = 8;
	static constexpr GLint FALLBACK_TEXTURE_SIZE = 1024;

	struct Dirty {
		bool texture = true;
		bool textureFilter = true;
		bool checker = true;
		bool transform = true;
		int lastOutlineSize = -1;
	};

	struct CanvasShader {
		GLuint program = 0;
		GLuint vao = 0;
		GLint rectLocation = 0;
		GLint viewLocation = 0;
		GLint translationLocation = 0;
		GLint transformLocation = 0;
		GLint canvasLocation = 0;
		GLint checkerLocation = 0;
		GLint smoothLocation = 0;
		GLint gridScaleLocation = 0;
	};

	struct OutlineShader {
		GLuint program = 0;
		GLuint vao = 0;
		GLint viewLocation = 0;
		GLint posLocation = 0;
		GLint translationLocation = 0;
		GLint transformLocation = 0;
		GLint sizeLocation = 0;
		GLint widthLocation = 0;
		GLint shapeLocation = 0;
	};

	void dispose(QOpenGLFunctions *f, QOpenGLExtraFunctions *e)
	{
		Q_ASSERT(initialized);
		initialized = false;

		qCDebug(lcDpGlCanvas, "Delete buffers");
		f->glDeleteBuffers(BUFFER_COUNT, buffers);
		for(int i = 0; i < BUFFER_COUNT; ++i) {
			buffers[i] = 0;
		}

		if(!canvasTextures.isEmpty()) {
			qCDebug(lcDpGlCanvas, "Delete canvas textures");
			f->glDeleteTextures(
				canvasTextures.size(), canvasTextures.constData());
		}
		canvasTextures.clear();
		canvasRects.clear();
		canvasFilters.clear();

		qCDebug(lcDpGlCanvas, "Delete checker texture");
		f->glDeleteTextures(1, &checkerTexture);

		qCDebug(lcDpGlCanvas, "Delete canvas program");
		f->glDeleteProgram(canvasShader.program);

		qCDebug(lcDpGlCanvas, "Delete outline program");
		f->glDeleteProgram(outlineShader.program);

		if(!haveGles2) {
			qCDebug(lcDpGlCanvas, "Delete canvas vertex array object");
			e->glDeleteVertexArrays(1, &canvasShader.vao);

			qCDebug(lcDpGlCanvas, "Delete outline vertex array object");
			e->glDeleteVertexArrays(1, &outlineShader.vao);
		}

		canvasShader = CanvasShader();
	}

	static QString getGlString(QOpenGLFunctions *f, GLenum name)
	{
		const GLubyte *value = f->glGetString(name);
		return value ? QString::fromUtf8(reinterpret_cast<const char *>(value))
					 : QString();
	}

	static bool supportsFragmentHighp(QOpenGLFunctions *f)
	{
		GLint range[2] = {0, 0};
		GLint precision = 0;
		f->glGetShaderPrecisionFormat(
			GL_FRAGMENT_SHADER, GL_HIGH_FLOAT, range, &precision);
		return precision != 0;
	}

	static QString getGlShaderPrecision(
		QOpenGLContext *context, QOpenGLFunctions *f, GLenum shadertype,
		GLenum precisiontype)
	{
		if(context->isOpenGLES() || context->format().majorVersion() >= 4) {
			GLint range[2] = {0, 0};
			GLint precision = 0;
			f->glGetShaderPrecisionFormat(
				shadertype, precisiontype, range, &precision);
			return QStringLiteral("range (%1, %2) precision %3")
				.arg(range[0])
				.arg(range[1])
				.arg(precision);
		} else {
			return QStringLiteral("range unknown precision unknown");
		}
	}

	static GLuint parseShader(
		QOpenGLFunctions *f, GLenum type, const QVector<const char *> &code)
	{
		GLuint shader = f->glCreateShader(type);
		f->glShaderSource(
			shader, GLsizei(code.size()),
			const_cast<const char **>(code.constData()), nullptr);
		f->glCompileShader(shader);
		return shader;
	}

	static void dumpShaderInfoLog(QOpenGLFunctions *f, GLuint shader)
	{
		GLint logLength;
		f->glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
		if(logLength > 0) {
			QByteArray buffer(logLength, '\0');
			GLsizei bufferLength;
			f->glGetShaderInfoLog(
				shader, logLength, &bufferLength, buffer.data());
			qCWarning(
				lcDpGlCanvas, "Shader info log: %.*s", int(bufferLength),
				buffer.constData());
		}
	}

	static GLint getShaderCompileStatus(QOpenGLFunctions *f, GLuint shader)
	{
		GLint compileStatus;
		f->glGetShaderiv(shader, GL_COMPILE_STATUS, &compileStatus);
		return compileStatus;
	}

	static GLuint compileShader(
		QOpenGLFunctions *f, GLenum type, const QVector<const char *> &code)
	{
		if(lcDpGlCanvas().isDebugEnabled()) {
			QString joined;
			for(const char *c : code) {
				joined.append(QString::fromUtf8(c));
			}
			qCDebug(
				lcDpGlCanvas, "Compile shader:\n%s", qUtf8Printable(joined));
		}
		GLuint shader = parseShader(f, type, code);
		dumpShaderInfoLog(f, shader);
		if(!getShaderCompileStatus(f, shader)) {
			const char *title = type == GL_VERTEX_SHADER	 ? "vertex"
								: type == GL_FRAGMENT_SHADER ? "fragment"
															 : "unknown";
			qCCritical(lcDpGlCanvas, "Can't compile %s shader", title);
		}
		return shader;
	}

	static void dumpProgramInfoLog(QOpenGLFunctions *f, GLuint program)
	{
		GLint logLength;
		f->glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
		if(logLength > 1) {
			QByteArray buffer(logLength + 1, '\0');
			GLsizei bufferLength;
			f->glGetProgramInfoLog(
				program, logLength, &bufferLength, buffer.data());
			if(bufferLength > 0) {
				qCWarning(
					lcDpGlCanvas, "Program info log: %.*s", int(bufferLength),
					buffer.constData());
			}
		}
	}

	static GLint getProgramLinkStatus(QOpenGLFunctions *f, GLuint program)
	{
		GLint linkStatus;
		f->glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
		return linkStatus;
	}

	static const char *getShaderVersionCode(bool haveGles2)
	{
		if(haveGles2) {
			return "#version 100\n"
				   "#define DP_VERTEX_IN attribute\n"
				   "#define DP_VERTEX_OUT varying\n"
				   "#define DP_FRAGMENT_IN varying\n"
				   "#define DP_FRAG_COLOR gl_FragColor\n"
				   "#define DP_TEXTURE2D texture2D\n";
		} else {
			return "#version 150\n"
				   "#define DP_VERTEX_IN in\n"
				   "#define DP_VERTEX_OUT out\n"
				   "#define DP_FRAGMENT_IN in\n"
				   "#define DP_FRAGMENT_OUT out\n"
				   "#define DP_FRAG_COLOR o_frag_color\n"
				   "#define DP_TEXTURE2D texture\n";
		}
	}

	static const char *getShaderHighpCode(bool highp)
	{
		return highp ? "#define DP_HAVE_HIGHP 1\n" : "";
	}

	static CanvasShader initCanvasShader(
		QOpenGLFunctions *f, QOpenGLExtraFunctions *e, bool haveGles2,
		bool highp)
	{
		static constexpr char canvasVertexCode[] = R"""(
uniform vec4 u_rect;
uniform vec2 u_view;
uniform vec2 u_translation;
uniform mat3 u_transform;
DP_VERTEX_IN mediump vec2 v_uv;
#ifdef DP_HAVE_HIGHP
DP_VERTEX_OUT highp vec2 f_pos;
#endif
DP_VERTEX_OUT vec2 f_uv;
DP_VERTEX_OUT vec2 f_uvChecker;

void main()
{
	vec2 vp = vec2(u_rect.x + v_uv.x * u_rect.z, u_rect.y + v_uv.y * u_rect.w);
	vec2 pos = floor((u_transform * vec3(vp, 0.0)).xy + u_translation + 0.5);
	float x = 2.0 * pos.x / u_view.x;
	float y = -2.0 * pos.y / u_view.y;
	gl_Position = vec4(x, y, 0.0, 1.0);
#ifdef DP_HAVE_HIGHP
	f_pos = vp;
#endif
	f_uv = v_uv;
	f_uvChecker = pos / 64.0;
}
	)""";

		static constexpr char canvasFragmentCode[] = R"""(
precision mediump float;

uniform sampler2D u_canvas;
uniform sampler2D u_checker;
#ifdef DP_HAVE_HIGHP
uniform highp vec4 u_rect;
uniform float u_smooth;
uniform highp float u_gridScale;
DP_FRAGMENT_IN highp vec2 f_pos;
#endif
DP_FRAGMENT_IN vec2 f_uv;
DP_FRAGMENT_IN vec2 f_uvChecker;
#ifdef DP_FRAGMENT_OUT
DP_FRAGMENT_OUT vec4 DP_FRAG_COLOR;
#endif

void main()
{
	float lod_bias = -0.5;
	vec4 canvasColor;
#ifdef DP_HAVE_HIGHP
	if(u_smooth < 0.5) {
		highp vec2 p = floor(f_pos) + vec2(0.5, 0.5);
		canvasColor = DP_TEXTURE2D(
			u_canvas,
			vec2((p.x - u_rect.x) / u_rect.z, (p.y - u_rect.y) / u_rect.w),
			lod_bias);
	} else {
#endif
		canvasColor = DP_TEXTURE2D(u_canvas, f_uv, lod_bias);
#ifdef DP_HAVE_HIGHP
	}
#endif

	vec3 c;
	if(canvasColor.a >= 1.0) {
		c = canvasColor.bgr;
	} else {
		c = DP_TEXTURE2D(u_checker, f_uvChecker).rgb * (1.0 - canvasColor.a)
			+ canvasColor.bgr;
	}

#ifdef DP_HAVE_HIGHP
	if(u_gridScale > 0.0) {
		vec2 gridPos = mod(abs(f_pos), 1.0);
		if(min(gridPos.x, gridPos.y) < u_gridScale) {
			lowp float lum = 0.3 * c.r + 0.59 * c.g + 0.11 * c.b;
			c = mix(c, vec3(lum < 0.2 ? 0.3 - lum : 0.0), 0.3);
		}
	}
#endif

	DP_FRAG_COLOR = vec4(c, 1.0);
}
	)""";

		const char *versionCode = getShaderVersionCode(haveGles2);
		const char *highpCode = getShaderHighpCode(highp);

		qCDebug(lcDpGlCanvas, "Create canvas shader program");
		CanvasShader canvasShader;
		canvasShader.program = f->glCreateProgram();

		if(!haveGles2) {
			qCDebug(lcDpGlCanvas, "Create canvas shader vertex array object");
			e->glGenVertexArrays(1, &canvasShader.vao);
		}

		qCDebug(lcDpGlCanvas, "Compile canvas vertex shader");
		GLuint canvasVertexShader = Private::compileShader(
			f, GL_VERTEX_SHADER, {versionCode, highpCode, canvasVertexCode});

		qCDebug(lcDpGlCanvas, "Compile canvas fragment shader");
		GLuint canvasFragmentShader = Private::compileShader(
			f, GL_FRAGMENT_SHADER,
			{versionCode, highpCode, canvasFragmentCode});

		qCDebug(lcDpGlCanvas, "Attach canvas vertex shader");
		f->glAttachShader(canvasShader.program, canvasVertexShader);

		qCDebug(lcDpGlCanvas, "Attach canvas fragment shader");
		f->glAttachShader(canvasShader.program, canvasFragmentShader);

		qCDebug(lcDpGlCanvas, "Link canvas shader program");
		f->glLinkProgram(canvasShader.program);

		dumpProgramInfoLog(f, canvasShader.program);
		if(getProgramLinkStatus(f, canvasShader.program)) {
			qCDebug(lcDpGlCanvas, "Get canvas rect uniform location");
			canvasShader.rectLocation =
				f->glGetUniformLocation(canvasShader.program, "u_rect");
			qCDebug(lcDpGlCanvas, "Get canvas view uniform location");
			canvasShader.viewLocation =
				f->glGetUniformLocation(canvasShader.program, "u_view");
			qCDebug(lcDpGlCanvas, "Get canvas translation uniform location");
			canvasShader.translationLocation =
				f->glGetUniformLocation(canvasShader.program, "u_translation");
			qCDebug(lcDpGlCanvas, "Get canvas transform uniform location");
			canvasShader.transformLocation =
				f->glGetUniformLocation(canvasShader.program, "u_transform");
			qCDebug(lcDpGlCanvas, "Get canvas sampler uniform location");
			canvasShader.canvasLocation =
				f->glGetUniformLocation(canvasShader.program, "u_canvas");
			qCDebug(
				lcDpGlCanvas, "Get canvas checker sampler uniform location");
			canvasShader.checkerLocation =
				f->glGetUniformLocation(canvasShader.program, "u_checker");
			qCDebug(lcDpGlCanvas, "Get canvas smooth uniform location");
			canvasShader.smoothLocation =
				highp
					? f->glGetUniformLocation(canvasShader.program, "u_smooth")
					: 0;
			qCDebug(lcDpGlCanvas, "Get canvas grid scale uniform location");
			canvasShader.gridScaleLocation =
				highp ? f->glGetUniformLocation(
							canvasShader.program, "u_gridScale")
					  : 0;
		} else {
			qCCritical(lcDpGlCanvas, "Can't link canvas shader program");
		}

		qCDebug(lcDpGlCanvas, "Detach canvas vertex shader");
		f->glDetachShader(canvasShader.program, canvasVertexShader);
		qCDebug(lcDpGlCanvas, "Detach canvas fragment shader");
		f->glDetachShader(canvasShader.program, canvasFragmentShader);
		qCDebug(lcDpGlCanvas, "Delete canvas vertex shader");
		f->glDeleteShader(canvasVertexShader);
		qCDebug(lcDpGlCanvas, "Delete canvas fragment shader");
		f->glDeleteShader(canvasFragmentShader);

		return canvasShader;
	}

	static OutlineShader initOutlineShader(
		QOpenGLFunctions *f, QOpenGLExtraFunctions *e, bool haveGles2,
		bool highp)
	{
		static constexpr char outlineVertexCode[] = R"""(
uniform vec2 u_view;
uniform vec2 u_pos;
uniform vec2 u_translation;
uniform mat3 u_transform;
DP_VERTEX_IN mediump vec2 v_pos;
#ifdef DP_HAVE_HIGHP
DP_VERTEX_OUT highp vec2 f_pos;
#else
DP_VERTEX_OUT vec2 f_pos;
#endif

void main()
{
	vec2 pos = floor(
		(u_transform * vec3(v_pos + u_pos, 0.0)).xy + u_translation + 0.5);
	float x = 2.0 * pos.x / u_view.x;
	float y = -2.0 * pos.y / u_view.y;
	gl_Position = vec4(x, y, 0.0, 1.0);
	f_pos = v_pos;
}
	)""";

		static constexpr char outlineFragmentCode[] = R"""(
precision mediump float;

#ifdef DP_HAVE_HIGHP
uniform highp float u_size;
uniform highp float u_width;
uniform highp float u_shape;
DP_FRAGMENT_IN highp vec2 f_pos;
#else
uniform float u_size;
uniform float u_width;
uniform float u_shape;
DP_FRAGMENT_IN vec2 f_pos;
#endif
#ifdef DP_FRAGMENT_OUT
DP_FRAGMENT_OUT vec4 DP_FRAG_COLOR;
#endif

void main()
{
#ifdef DP_HAVE_HIGHP
	highp float distance;
#else
	float distance;
#endif
	distance = u_shape < 0.5
		? abs(length(f_pos) - u_size + u_width) // Circle
		: abs(max(abs(f_pos.x), abs(f_pos.y)) - u_size + u_width); // Square
	if(distance <= u_width * 0.5) {
		DP_FRAG_COLOR = vec4(0.5, 1.0, 0.5, 1.0);
	} else {
		discard;
	}
}
	)""";

		const char *versionCode = getShaderVersionCode(haveGles2);
		const char *highpCode = getShaderHighpCode(highp);

		qCDebug(lcDpGlCanvas, "Create outline shader program");
		OutlineShader outlineShader;
		outlineShader.program = f->glCreateProgram();

		if(!haveGles2) {
			qCDebug(lcDpGlCanvas, "Create outline shader vertex array object");
			e->glGenVertexArrays(1, &outlineShader.vao);
		}

		qCDebug(
			lcDpGlCanvas, "Compile outline vertex shader: %s",
			outlineVertexCode);
		GLuint outlineVertexShader = Private::compileShader(
			f, GL_VERTEX_SHADER, {versionCode, highpCode, outlineVertexCode});

		qCDebug(
			lcDpGlCanvas, "Compile outline fragment shader: %s",
			outlineFragmentCode);
		GLuint outlineFragmentShader = Private::compileShader(
			f, GL_FRAGMENT_SHADER,
			{versionCode, highpCode, outlineFragmentCode});

		qCDebug(lcDpGlCanvas, "Attach outline vertex shader");
		f->glAttachShader(outlineShader.program, outlineVertexShader);

		qCDebug(lcDpGlCanvas, "Attach outline fragment shader");
		f->glAttachShader(outlineShader.program, outlineFragmentShader);

		qCDebug(lcDpGlCanvas, "Link outline shader program");
		f->glLinkProgram(outlineShader.program);

		dumpProgramInfoLog(f, outlineShader.program);
		if(getProgramLinkStatus(f, outlineShader.program)) {
			qCDebug(lcDpGlCanvas, "Get outline view uniform location");
			outlineShader.viewLocation =
				f->glGetUniformLocation(outlineShader.program, "u_view");
			qCDebug(lcDpGlCanvas, "Get outline pos uniform location");
			outlineShader.posLocation =
				f->glGetUniformLocation(outlineShader.program, "u_pos");
			qCDebug(lcDpGlCanvas, "Get outline translation uniform location");
			outlineShader.translationLocation =
				f->glGetUniformLocation(outlineShader.program, "u_translation");
			qCDebug(lcDpGlCanvas, "Get outline transform uniform location");
			outlineShader.transformLocation =
				f->glGetUniformLocation(outlineShader.program, "u_transform");
			qCDebug(lcDpGlCanvas, "Get outline size uniform location");
			outlineShader.sizeLocation =
				f->glGetUniformLocation(outlineShader.program, "u_size");
			qCDebug(lcDpGlCanvas, "Get outline width uniform location");
			outlineShader.widthLocation =
				f->glGetUniformLocation(outlineShader.program, "u_width");
			qCDebug(lcDpGlCanvas, "Get outline shape uniform location");
			outlineShader.shapeLocation =
				f->glGetUniformLocation(outlineShader.program, "u_shape");
		} else {
			qCCritical(lcDpGlCanvas, "Can't link outline shader program");
		}

		qCDebug(lcDpGlCanvas, "Detach outline vertex shader");
		f->glDetachShader(outlineShader.program, outlineVertexShader);
		qCDebug(lcDpGlCanvas, "Detach outline fragment shader");
		f->glDetachShader(outlineShader.program, outlineFragmentShader);
		qCDebug(lcDpGlCanvas, "Delete outline vertex shader");
		f->glDeleteShader(outlineVertexShader);
		qCDebug(lcDpGlCanvas, "Delete outline fragment shader");
		f->glDeleteShader(outlineFragmentShader);

		return outlineShader;
	}

	static GLuint initCheckerTexture(QOpenGLFunctions *f)
	{
		f->glActiveTexture(GL_TEXTURE0);
		GLuint texture;
		f->glGenTextures(1, &texture);

		if(texture == 0) {
			qCWarning(lcDpGlCanvas, "Can't generate checker texture");
		} else {
			f->glBindTexture(GL_TEXTURE_2D, texture);
			f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		}

		f->glBindTexture(GL_TEXTURE_2D, 0);
		return texture;
	}

	static void
	setCheckerColor(QByteArray &pixels, int x, int y, const QColor &color)
	{
		int i = (y * 64 + x) * 3;
		pixels[i] = color.red();
		pixels[i + 1] = color.green();
		pixels[i + 2] = color.blue();
	}

	void refreshChecker(QOpenGLFunctions *f)
	{
		if(dirty.checker) {
			dirty.checker = false;
			QByteArray pixels;
			pixels.resize(64 * 64 * 3);
			for(int y = 0; y < 32; ++y) {
				for(int x = 0; x < 32; ++x) {
					setCheckerColor(pixels, x, y, checkerColor1);
					setCheckerColor(pixels, x + 32, y, checkerColor2);
					setCheckerColor(pixels, x, y + 32, checkerColor2);
					setCheckerColor(pixels, x + 32, y + 32, checkerColor1);
				}
			}
			f->glTexImage2D(
				GL_TEXTURE_2D, 0, GL_RGB, 64, 64, 0, GL_RGB, GL_UNSIGNED_BYTE,
				pixels.constData());
		}
	}

	void refreshTransform()
	{
		if(dirty.transform) {
			dirty.transform = false;
			const QTransform &tf = controller->transform();
			translationX = tf.dx();
			translationY = tf.dy();
			transform[0] = tf.m11();
			transform[1] = tf.m12();
			transform[2] = tf.m13();
			transform[3] = tf.m21();
			transform[4] = tf.m22();
			transform[5] = tf.m23();
			transform[6] = tf.m31();
			transform[7] = tf.m32();
			transform[8] = tf.m33();
		}
	}

	void refreshOutlineVertices(
		QOpenGLFunctions *f, int outlineSize, qreal outlineWidth)
	{
		int effectiveOutlineSize = qCeil(outlineSize + outlineWidth * 2.0);
		if(effectiveOutlineSize != dirty.lastOutlineSize) {
			dirty.lastOutlineSize = effectiveOutlineSize;
			GLfloat topLeft = effectiveOutlineSize * -0.5f;
			GLfloat bottomRight = effectiveOutlineSize * 0.5f;
			outlineVertices[0] = topLeft;
			outlineVertices[1] = bottomRight;
			outlineVertices[2] = topLeft;
			outlineVertices[3] = topLeft;
			outlineVertices[4] = bottomRight;
			outlineVertices[5] = bottomRight;
			outlineVertices[6] = bottomRight;
			outlineVertices[7] = topLeft;
			f->glBufferData(
				GL_ARRAY_BUFFER, sizeof(outlineVertices), outlineVertices,
				GL_STATIC_DRAW);
		}
	}

	void setUpCanvasShader(QOpenGLFunctions *f, QOpenGLExtraFunctions *e)
	{
		f->glUseProgram(canvasShader.program);

		f->glActiveTexture(GL_TEXTURE1);
		f->glBindTexture(GL_TEXTURE_2D, checkerTexture);
		refreshChecker(f);

		GLfloat viewWidth = viewSize.width();
		GLfloat viewHeight = viewSize.height();
		f->glUniform2f(canvasShader.viewLocation, viewWidth, viewHeight);
		refreshTransform();
		f->glUniform2f(
			canvasShader.translationLocation, translationX, translationY);
		f->glUniformMatrix3fv(
			canvasShader.transformLocation, 1, GL_FALSE, transform);
		f->glUniform1i(canvasShader.canvasLocation, 0);
		f->glUniform1i(canvasShader.checkerLocation, 1);

		if(haveFragmentHighp) {
			qreal pixelGridScale = controller->pixelGridScale();
			f->glUniform1f(
				canvasShader.gridScaleLocation,
				pixelGridScale > 0.0 ? 1.0 / pixelGridScale : 0.0);
		}

		if(!haveGles2) {
			e->glBindVertexArray(canvasShader.vao);
		}

		f->glEnableVertexAttribArray(0);
		f->glBindBuffer(
			GL_ARRAY_BUFFER, buffers[Private::CANVAS_UV_BUFFER_INDEX]);
		f->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

		f->glActiveTexture(GL_TEXTURE0);
	}

	void updateCanvasTextureFilter(QOpenGLFunctions *f)
	{
		if(dirty.textureFilter) {
			dirty.textureFilter = false;
			bool linear = controller->shouldRenderSmooth();
			if(linear && !textureFilterLinear) {
				qCDebug(lcDpGlCanvas, "Set texture filter to linear");
				textureFilterLinear = true;
			} else if(!linear && textureFilterLinear) {
				qCDebug(lcDpGlCanvas, "Set texture filter to nearest");
				textureFilterLinear = false;
			}
		}
		if(haveFragmentHighp) {
			f->glUniform1f(
				canvasShader.smoothLocation, textureFilterLinear ? 1.0 : 0.0);
		}
	}

	void
	drawCanvasShader(QOpenGLFunctions *f, const QRect &rect, GLint &inOutFilter)
	{
		GLint minFilter, maxFilter;
		if(textureFilterLinear) {
			if(usingMipmaps) {
				minFilter = GL_LINEAR_MIPMAP_LINEAR;
			} else {
				minFilter = GL_LINEAR;
			}
			maxFilter = GL_LINEAR;
		} else {
			minFilter = GL_NEAREST;
			maxFilter = GL_NEAREST;
		}

		if(inOutFilter != maxFilter) {
			inOutFilter = maxFilter;
			f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
			f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, maxFilter);
		}
		f->glUniform4f(
			canvasShader.rectLocation, rect.x(), rect.y(), rect.width(),
			rect.height());
		f->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	}

	void drawCanvasDirtyTexture(
		QOpenGLFunctions *f, canvas::TileCache &tileCache, GLuint texture,
		const QRect &rect, GLint &inOutFilter)
	{
		QRect tileRect = QRect(
			QPoint(rect.left() / DP_TILE_SIZE, rect.top() / DP_TILE_SIZE),
			QPoint(rect.right() / DP_TILE_SIZE, rect.bottom() / DP_TILE_SIZE));
		QRect visibleTileRect =
			tileRect.intersected(controller->canvasViewTileArea());
		if(!visibleTileRect.isEmpty()) {
			// This is zero after a resize because the the textures will already
			// be bound from actually setting their size beforehand. If we're
			// just updating the tiles without resizing, it'll be non-zero
			if(texture != 0) {
				f->glBindTexture(GL_TEXTURE_2D, texture);
			}
			tileCache.eachDirtyTileReset(
				visibleTileRect,
				[&](const QRect &pixelRect, const void *pixels) {
					f->glTexSubImage2D(
						GL_TEXTURE_2D, 0, pixelRect.x() - rect.x(),
						pixelRect.y() - rect.y(), pixelRect.width(),
						pixelRect.height(), GL_RGBA, GL_UNSIGNED_BYTE, pixels);
				});
			if(usingMipmaps) {
				f->glGenerateMipmap(GL_TEXTURE_2D);
			}
			drawCanvasShader(f, rect, inOutFilter);
		}
	}

	void renderCanvasDirtyTexturesResize(
		QOpenGLFunctions *f, canvas::TileCache &tileCache)
	{
		int neededX = totalTextureSize.width() / maxTextureSize;
		if(neededX * maxTextureSize < totalTextureSize.width()) {
			++neededX;
		}

		int neededY = totalTextureSize.height() / maxTextureSize;
		if(neededY * maxTextureSize < totalTextureSize.height()) {
			++neededY;
		}

		int neededTotal = neededX * neededY;
		int previousTotal = canvasTextures.size();
		qCDebug(
			lcDpGlCanvas, "Need %d textures, previous %d", neededTotal,
			previousTotal);

		if(neededTotal < previousTotal) {
			f->glDeleteTextures(
				previousTotal - neededTotal,
				canvasTextures.constData() + neededTotal);
			canvasTextures.resize(neededTotal);
			canvasRects.resize(neededTotal);
			canvasFilters.resize(neededTotal);
		} else if(neededTotal > previousTotal) {
			canvasTextures.resize(neededTotal);
			canvasRects.resize(neededTotal);
			canvasFilters.resize(neededTotal);
			f->glGenTextures(
				neededTotal - previousTotal,
				canvasTextures.data() + previousTotal);
		}

		int i = 0;
		int y = 0;
		int heightLeft = totalTextureSize.height();
		while(heightLeft > 0) {
			int textureHeight = qMin(heightLeft, maxTextureSize);
			heightLeft -= textureHeight;
			int widthLeft = totalTextureSize.width();
			int x = 0;
			while(widthLeft > 0) {
				int textureWidth = qMin(widthLeft, maxTextureSize);
				widthLeft -= textureWidth;
				canvasRects[i] = QRect(x, y, textureWidth, textureHeight);

				f->glBindTexture(GL_TEXTURE_2D, canvasTextures[i]);
				bool fresh = i >= previousTotal;
				if(fresh) {
					canvasFilters[i] = 0;
					f->glTexParameteri(
						GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
					f->glTexParameteri(
						GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				}
				f->glTexImage2D(
					GL_TEXTURE_2D, 0, GL_RGBA, textureWidth, textureHeight, 0,
					GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

				drawCanvasDirtyTexture(
					f, tileCache, 0, canvasRects[i], canvasFilters[i]);

				x += textureWidth;
				++i;
			}
			y += textureHeight;
		}
	}

	void clearTextures(QOpenGLFunctions *f, canvas::TileCache &tileCache)
	{
		totalTextureSize = QSize();
		GLsizei count = canvasTextures.size();
		if(count != 0) {
			f->glDeleteTextures(count, canvasTextures.constData());
			canvasTextures.clear();
			canvasRects.clear();
			canvasFilters.clear();
		}
		tileCache.markAllTilesDirty();
	}

	void renderCanvasDirtyTextures(
		QOpenGLFunctions *f, QOpenGLExtraFunctions *e,
		canvas::TileCache &tileCache)
	{
		DP_PERF_SCOPE("paint_gl:canvas_dirty:textures");
		qCDebug(lcDpGlCanvas, "renderCanvasDirtyTexture");
		setUpCanvasShader(f, e);
		updateCanvasTextureFilter(f);

		bool mipmaps = textureFilterLinear && shouldUseMipmaps;
		if(mipmaps != usingMipmaps) {
			qCDebug(
				lcDpGlCanvas, mipmaps ? "Enable mipmaps" : "Disable mipmaps");
			usingMipmaps = mipmaps;
			// Can't really get rid of mipmaps separately, just just destroy and
			// recreate the textures. This only happens if the user changes the
			// setting, so it's not performance-relevant.
			clearTextures(f, tileCache);
		}

		QSize size = tileCache.size();
		bool textureSizeChanged = totalTextureSize != size;
		if(textureSizeChanged) {
			totalTextureSize = size;
			renderCanvasDirtyTexturesResize(f, tileCache);
		} else {
			int textureCount = canvasTextures.size();
			for(int i = 0; i < textureCount; ++i) {
				drawCanvasDirtyTexture(
					f, tileCache, canvasTextures[i], canvasRects[i],
					canvasFilters[i]);
			}
		}
	}

	void renderCanvasDirty(QOpenGLFunctions *f, QOpenGLExtraFunctions *e)
	{
		DP_PERF_SCOPE("paint_gl:canvas_dirty");
		// The loop is necessary because the canvas size may change
		// again while we're refreshing the tile cache visible area.
		while(true) {
			bool wasResized = false;
			canvas::TileCache::Resize resize;
			controller->withTileCache([&](canvas::TileCache &tileCache) {
				wasResized = tileCache.getResizeReset(resize);
				if(!wasResized) {
					renderCanvasDirtyTextures(f, e, tileCache);
				}
			});
			if(wasResized) {
				controller->updateCanvasSize(
					resize.width, resize.height, resize.offsetX,
					resize.offsetY);
			} else {
				break;
			}
		}
	}

	void renderCanvasClean(QOpenGLFunctions *f, QOpenGLExtraFunctions *e)
	{
		DP_PERF_SCOPE("paint_gl:canvas_clean");
		setUpCanvasShader(f, e);
		updateCanvasTextureFilter(f);
		int textureCount = canvasTextures.size();
		for(int i = 0; i < textureCount; ++i) {
			f->glBindTexture(GL_TEXTURE_2D, canvasTextures[i]);
			drawCanvasShader(f, canvasRects[i], canvasFilters[i]);
		}
	}

	void renderCanvas(QOpenGLFunctions *f, QOpenGLExtraFunctions *e)
	{
		// Rendering the canvas has three paths, to avoid state changes as much
		// as possible. When nothing changed since last time, we just render the
		// textures as-is. If the contents changed, but the size is the same, we
		// update the tiles and filtering of all visible textures and render
		// those out. Finally, if the size changed too, we reallocate the
		// textures entirely, rendering out each one along the way.
		if(dirty.texture) {
			dirty.texture = false;
			renderCanvasDirty(f, e);
		} else {
			renderCanvasClean(f, e);
		}
	}

	void renderOutline(QOpenGLFunctions *f, QOpenGLExtraFunctions *e, qreal dpr)
	{
		DP_PERF_SCOPE("paint_gl:outline");
		f->glEnable(GL_BLEND);
		f->glBlendFuncSeparate(GL_ONE, GL_SRC_COLOR, GL_ONE, GL_ONE);
		f->glBlendEquationSeparate(GL_FUNC_SUBTRACT, GL_FUNC_ADD);

		f->glUseProgram(outlineShader.program);

		f->glUniform2f(
			outlineShader.viewLocation, viewSize.width(), viewSize.height());
		QPointF outlinePos = controller->outlinePos();
		f->glUniform2f(
			outlineShader.posLocation, outlinePos.x(), outlinePos.y());
		f->glUniform2f(
			outlineShader.translationLocation, translationX, translationY);
		f->glUniformMatrix3fv(
			outlineShader.transformLocation, 1, GL_FALSE, transform);
		int outlineSize = controller->outlineSize();
		qreal outlineWidth =
			controller->outlineWidth() / controller->zoom() * dpr;
		f->glUniform1f(
			outlineShader.sizeLocation, outlineSize * 0.5f + outlineWidth);
		f->glUniform1f(outlineShader.widthLocation, outlineWidth);
		f->glUniform1f(
			outlineShader.shapeLocation,
			controller->isSquareOutline() ? 1.0f : 0.0f);

		if(haveGles2) {
			// Called after renderCanvas, assumes vertex attribute 0 is enabled.
		} else {
			e->glBindVertexArray(outlineShader.vao);
			f->glEnableVertexAttribArray(0);
		}

		f->glBindBuffer(
			GL_ARRAY_BUFFER, buffers[Private::OUTLINE_VERTEX_BUFFER_INDEX]);
		refreshOutlineVertices(f, outlineSize, outlineWidth);
		f->glVertexAttribPointer(0, 2, GL_FLOAT, false, 0, nullptr);

		f->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

		// Qt doesn't reset these in endNativePainting.
		f->glDisable(GL_BLEND);
		f->glBlendFunc(GL_ONE, GL_ZERO);
		f->glBlendEquation(GL_FUNC_ADD);
	}

	void renderScene(QPainter &painter)
	{
		DP_PERF_SCOPE("paint_gl:scene");
		controller->scene()->render(&painter);
	}

	CanvasController *controller;
	QSize viewSize;
	QColor checkerColor1;
	QColor checkerColor2;
	Dirty dirty;
	bool initialized = false;
	bool haveGles2 = false;
	bool haveFragmentHighp;
	GLint maxTextureSize;
	CanvasShader canvasShader;
	OutlineShader outlineShader;
	GLuint buffers[BUFFER_COUNT] = {0, 0};
	GLuint checkerTexture;
	QVector<GLuint> canvasTextures;
	QVector<QRect> canvasRects;
	QVector<GLint> canvasFilters;
	QSize totalTextureSize;
	bool textureFilterLinear = false;
	bool usingMipmaps = false;
	bool shouldUseMipmaps = false;
	GLfloat translationX;
	GLfloat translationY;
	GLfloat transform[9];
	GLfloat outlineVertices[VERTEX_COUNT];
};

GlCanvas::GlCanvas(CanvasController *controller, QWidget *parent)
	: QOpenGLWidget(parent)
	, d(new Private)
{
	d->controller = controller;
	controller->setCanvasWidget(this);

	desktop::settings::Settings &settings = dpApp().settings();
	settings.bindCheckerColor1(this, &GlCanvas::setCheckerColor1);
	settings.bindCheckerColor2(this, &GlCanvas::setCheckerColor2);
	settings.bindUseMipmaps(this, &GlCanvas::setShouldUseMipmaps);

	connect(
		controller, &CanvasController::clearColorChanged, this,
		QOverload<>::of(&GlCanvas::update));
	connect(
		controller, &CanvasController::renderSmoothChanged, this,
		&GlCanvas::onControllerRenderSmoothChanged);
	connect(
		controller, &CanvasController::canvasVisibleChanged, this,
		QOverload<>::of(&GlCanvas::update));
	connect(
		controller, &CanvasController::transformChanged, this,
		&GlCanvas::onControllerTransformChanged);
	connect(
		controller, &CanvasController::pixelGridScaleChanged, this,
		QOverload<>::of(&GlCanvas::update));
	connect(
		controller, &CanvasController::tileCacheDirtyCheckNeeded, this,
		&GlCanvas::onControllerTileCacheDirtyCheckNeeded);
	connect(
		controller, &CanvasController::outlineChanged, this,
		QOverload<>::of(&GlCanvas::update));
	connect(
		controller, &CanvasController::transformNoticeChanged, this,
		QOverload<>::of(&GlCanvas::update));
	connect(
		controller, &CanvasController::lockNoticeChanged, this,
		QOverload<>::of(&GlCanvas::update));
	connect(
		controller->scene(), &CanvasScene::changed, this,
		QOverload<>::of(&GlCanvas::update));
}

GlCanvas::~GlCanvas()
{
	if(d->initialized) {
		makeCurrent();
		QOpenGLContext *context = QOpenGLContext::currentContext();
		d->dispose(
			context->functions(),
			d->haveGles2 ? nullptr : context->extraFunctions());
		doneCurrent();
	}
	delete d;
}

QWidget *GlCanvas::asWidget()
{
	return this;
}

QSize GlCanvas::viewSize() const
{
	return d->viewSize;
}

QPointF GlCanvas::viewToCanvasOffset() const
{
	return QPointF(d->viewSize.width() / -2.0, d->viewSize.height() / -2.0);
}

QPointF GlCanvas::viewTransformOffset() const
{
	return QPointF(0.0, 0.0);
}

void GlCanvas::handleResize(QResizeEvent *event)
{
	resizeEvent(event);
}

void GlCanvas::handlePaint(QPaintEvent *event)
{
	paintEvent(event);
}

const QStringList &GlCanvas::getSystemInfo()
{
	return systemInfo;
}

void GlCanvas::initializeGL()
{
	if(d->initialized) {
		qCWarning(lcDpGlCanvas, "Already initialized!");
		return;
	}
	d->initialized = true;

#ifdef CANVAS_IMPLEMENTATION_FALLBACK
	// Calling OpenGL functions can crash if the driver is sufficiently bad.
	// Set it to the default renderer temporarily so the user doesn't get stuck.
	desktop::settings::Settings *settings = nullptr;
	int originalRenderCanvas = -1;
	if(dpApp().isCanvasImplementationFromSettings()) {
		constexpr int fallback = int(CANVAS_IMPLEMENTATION_FALLBACK);
		settings = &dpApp().settings();
		originalRenderCanvas = settings->renderCanvas();
		if(originalRenderCanvas == fallback) {
			qCWarning(
				lcDpGlCanvas, "Canvas implementation is already set to %d",
				fallback);
		} else {
			qCDebug(
				lcDpGlCanvas, "Reverting canvas implementation from %d to %d",
				originalRenderCanvas, fallback);
			settings->setRenderCanvas(fallback);
			settings->trySubmit();
		}
	}
#endif

	QOpenGLContext *context = QOpenGLContext::currentContext();
	QOpenGLFunctions *f = context->functions();

	d->haveGles2 = context->isOpenGLES();
	QOpenGLExtraFunctions *e =
		d->haveGles2 ? nullptr : context->extraFunctions();

	d->haveFragmentHighp = !d->haveGles2 ||
						   context->format().majorVersion() > 2 ||
						   Private::supportsFragmentHighp(f);

	GLint maxTextureSize = 0;
	f->glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
	if(maxTextureSize < 64) {
		qCWarning(
			lcDpGlCanvas, "Invalid max texture size %d, punting to %d",
			int(maxTextureSize), int(Private::FALLBACK_TEXTURE_SIZE));
		maxTextureSize = Private::FALLBACK_TEXTURE_SIZE;
	}
	d->maxTextureSize = maxTextureSize - maxTextureSize % DP_TILE_SIZE;

	QSurfaceFormat format = context->format();
	systemInfo = QStringList({
		QStringLiteral("Type %1 %2.%3 %4 (%5)")
			.arg(QMetaEnum::fromType<QSurfaceFormat::RenderableType>()
					 .valueToKey(format.renderableType()))
			.arg(format.majorVersion())
			.arg(format.minorVersion())
			.arg(QMetaEnum::fromType<QSurfaceFormat::OpenGLContextProfile>()
					 .valueToKey(format.profile()))
			.arg(
				context->isOpenGLES() ? QStringLiteral("ES")
									  : QStringLiteral("Desktop")),
		QStringLiteral("Buffer sizes: A=%1 R=%2 G=%3 B=%4 depth=%5 stencil=%6")
			.arg(format.alphaBufferSize())
			.arg(format.redBufferSize())
			.arg(format.greenBufferSize())
			.arg(format.greenBufferSize())
			.arg(format.depthBufferSize())
			.arg(format.stencilBufferSize()),
		QStringLiteral("Swap interval: %1").arg(format.swapInterval()),
		QStringLiteral("Swap behavior: %1\n")
			.arg(QMetaEnum::fromType<QSurfaceFormat::SwapBehavior>().valueToKey(
				format.swapBehavior())),
		QStringLiteral("GL_RENDERER: %1")
			.arg(Private::getGlString(f, GL_RENDERER)),
		QStringLiteral("GL_VERSION: %1")
			.arg(Private::getGlString(f, GL_VERSION)),
		QStringLiteral("GL_VENDOR: %1").arg(Private::getGlString(f, GL_VENDOR)),
		QStringLiteral("GL_SHADING_LANGUAGE_VERSION: %1")
			.arg(Private::getGlString(f, GL_SHADING_LANGUAGE_VERSION)),
		QStringLiteral("GL_FRAGMENT_PRECISION_HIGH: %1")
			.arg(d->haveFragmentHighp ? 1 : 0),
		QStringLiteral("GL_MAX_TEXTURE_SIZE: %1 (%2 tiles)")
			.arg(maxTextureSize)
			.arg(maxTextureSize / DP_TILE_SIZE),
		QStringLiteral("Vertex float lowp %1")
			.arg(Private::getGlShaderPrecision(
				context, f, GL_VERTEX_SHADER, GL_LOW_FLOAT)),
		QStringLiteral("Vertex float mediump %1")
			.arg(Private::getGlShaderPrecision(
				context, f, GL_VERTEX_SHADER, GL_MEDIUM_FLOAT)),
		QStringLiteral("Vertex float highp %1")
			.arg(Private::getGlShaderPrecision(
				context, f, GL_VERTEX_SHADER, GL_HIGH_FLOAT)),
		QStringLiteral("Fragment float lowp %1")
			.arg(Private::getGlShaderPrecision(
				context, f, GL_FRAGMENT_SHADER, GL_LOW_FLOAT)),
		QStringLiteral("Fragment float mediump %1")
			.arg(Private::getGlShaderPrecision(
				context, f, GL_FRAGMENT_SHADER, GL_MEDIUM_FLOAT)),
		QStringLiteral("Fragment float highp %1")
			.arg(Private::getGlShaderPrecision(
				context, f, GL_FRAGMENT_SHADER, GL_HIGH_FLOAT)),
	});
	if(lcDpGlCanvas().isDebugEnabled()) {
		for(const QString &s : systemInfo) {
			qCDebug(lcDpGlCanvas, "%s", qUtf8Printable(s));
		}
	}

	d->canvasShader =
		Private::initCanvasShader(f, e, d->haveGles2, d->haveFragmentHighp);
	d->outlineShader =
		Private::initOutlineShader(f, e, d->haveGles2, d->haveFragmentHighp);

	qCDebug(lcDpGlCanvas, "Generate buffers");
	f->glGenBuffers(Private::BUFFER_COUNT, d->buffers);

	static constexpr GLfloat UVS[Private::VERTEX_COUNT] = {
		0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f};
	f->glBindBuffer(
		GL_ARRAY_BUFFER, d->buffers[Private::CANVAS_UV_BUFFER_INDEX]);
	f->glBufferData(GL_ARRAY_BUFFER, sizeof(UVS), UVS, GL_STATIC_DRAW);

	qCDebug(lcDpGlCanvas, "Init checker texture");
	d->checkerTexture = Private::initCheckerTexture(f);
	d->canvasTextures.clear();
	d->canvasRects.clear();
	d->textureFilterLinear = false;

	for(int i = 0; i < Private::VERTEX_COUNT; ++i) {
		d->outlineVertices[i] = 0.0f;
	}

	d->dirty = Private::Dirty();

#ifdef CANVAS_IMPLEMENTATION_FALLBACK
	if(settings && originalRenderCanvas >= 0) {
		qCDebug(
			lcDpGlCanvas, "Restoring canvas implementation to %d",
			originalRenderCanvas);
		settings->setRenderCanvas(originalRenderCanvas);
	}
#endif
}

void GlCanvas::paintGL()
{
	qCDebug(lcDpGlCanvas, "paintGL");
	if(!d->viewSize.isEmpty()) {
		DP_PERF_SCOPE("paint_gl");
		QPainter painter(this);
		painter.beginNativePainting();

		CanvasController *controller = d->controller;
		const QColor &clearColor = controller->clearColor();
		QOpenGLContext *context = QOpenGLContext::currentContext();
		QOpenGLFunctions *f = context->functions();
		QOpenGLExtraFunctions *e =
			d->haveGles2 ? nullptr : context->extraFunctions();
		f->glClearColor(
			clearColor.redF(), clearColor.greenF(), clearColor.blueF(), 1.0);
		f->glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

		bool haveCanvas = controller->isCanvasVisible();
		if(haveCanvas) {
			d->renderCanvas(f, e);
			if(d->controller->isOutlineVisible()) {
				d->renderOutline(f, e, devicePixelRatioF());
			}
			// Qt5 doesn't reset these in endNativePainting.
			f->glDisableVertexAttribArray(0);
			f->glBindBuffer(GL_ARRAY_BUFFER, 0);
			if(!d->haveGles2) {
				e->glBindVertexArray(0);
			}
		}

		painter.endNativePainting();

		d->renderScene(painter);
	}
}

void GlCanvas::resizeGL(int w, int h)
{
	qCDebug(lcDpGlCanvas, "resizeGL(%d, %d)", w, h);
	d->viewSize.setWidth(w);
	d->viewSize.setHeight(h);
	d->controller->updateViewSize();
}

void GlCanvas::setCheckerColor1(const QColor &color)
{
	d->dirty.checker = true;
	d->checkerColor1 = color;
	update();
}

void GlCanvas::setCheckerColor2(const QColor &color)
{
	d->dirty.checker = true;
	d->checkerColor2 = color;
	update();
}

void GlCanvas::setShouldUseMipmaps(bool useMipmaps)
{
	if(useMipmaps != d->shouldUseMipmaps) {
		d->shouldUseMipmaps = useMipmaps;
		d->dirty.texture = true;
		update();
	}
}

void GlCanvas::onControllerRenderSmoothChanged()
{
	d->dirty.textureFilter = true;
	update();
}

void GlCanvas::onControllerTransformChanged()
{
	d->dirty.texture = true;
	d->dirty.textureFilter = true;
	d->dirty.transform = true;
	update();
}

void GlCanvas::onControllerTileCacheDirtyCheckNeeded()
{
	d->dirty.texture = true;
	update();
}

}
