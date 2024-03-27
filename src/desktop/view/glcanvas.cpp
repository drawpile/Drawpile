// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/view/glcanvas.h"
#include "desktop/main.h"
#include "desktop/view/canvascontroller.h"
#include "desktop/view/canvasscene.h"
#include "libclient/canvas/tilecache.h"
#include <QOpenGLFunctions>
#include <QPaintEvent>
#include <QPainter>

Q_LOGGING_CATEGORY(lcDpGlCanvas, "net.drawpile.view.canvas.gl", QtWarningMsg)

namespace view {

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
		GLint rectLocation = 0;
		GLint viewLocation = 0;
		GLint translationLocation = 0;
		GLint transformLocation = 0;
		GLint canvasLocation = 0;
		GLint checkerLocation = 0;
		GLint gridScaleLocation = 0;
	};

	struct OutlineShader {
		GLuint program = 0;
		GLint viewLocation = 0;
		GLint posLocation = 0;
		GLint translationLocation = 0;
		GLint transformLocation = 0;
		GLint sizeLocation = 0;
		GLint widthLocation = 0;
		GLint shapeLocation = 0;
	};

	void dispose(QOpenGLFunctions *f)
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

		qCDebug(lcDpGlCanvas, "Delete program");
		f->glDeleteProgram(canvasShader.program);
		canvasShader = CanvasShader();
	}

	static QString getGlString(QOpenGLFunctions *f, GLenum name)
	{
		const GLubyte *value = f->glGetString(name);
		return value ? QString::fromUtf8(reinterpret_cast<const char *>(value))
					 : QString();
	}

	static GLuint
	parseShader(QOpenGLFunctions *f, GLenum type, const char *code)
	{
		GLuint shader = f->glCreateShader(type);
		f->glShaderSource(shader, 1, &code, nullptr);
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

	static GLuint
	compileShader(QOpenGLFunctions *f, GLenum type, const char *code)
	{
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
		if(logLength > 0) {
			QByteArray buffer(logLength, '\0');
			GLsizei bufferLength;
			f->glGetProgramInfoLog(
				program, logLength, &bufferLength, buffer.data());
			qCWarning(
				lcDpGlCanvas, "Program info log: %.*s", int(bufferLength),
				buffer.constData());
		}
	}

	static GLint getProgramLinkStatus(QOpenGLFunctions *f, GLuint program)
	{
		GLint linkStatus;
		f->glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
		return linkStatus;
	}

	static CanvasShader initCanvasShader(QOpenGLFunctions *f)
	{
		static constexpr char canvasVertexCode[] = R"""(
#version 100

uniform vec4 u_rect;
uniform vec2 u_view;
uniform vec2 u_translation;
uniform mat3 u_transform;
attribute mediump vec2 v_uv;
varying vec2 f_pos;
varying vec2 f_uv;
varying vec2 f_uvChecker;

void main()
{
	vec2 vp = vec2(u_rect.x + v_uv.x * u_rect.z, u_rect.y + v_uv.y * u_rect.w);
	vec2 pos = floor((u_transform * vec3(vp, 0.0)).xy + u_translation + 0.5);
	float x = 2.0 * pos.x / u_view.x;
	float y = -2.0 * pos.y / u_view.y;
	gl_Position = vec4(x, y, 0.0, 1.0);
	f_pos = vp;
	f_uv = v_uv;
	f_uvChecker = pos / 64.0;
}
	)""";

		static constexpr char canvasFragmentCode[] = R"""(
#version 100
precision mediump float;

uniform sampler2D u_canvas;
uniform sampler2D u_checker;
uniform float u_gridScale;
varying vec2 f_pos;
varying vec2 f_uv;
varying vec2 f_uvChecker;

void main()
{
	vec3 c;

	vec4 canvasColor = texture2D(u_canvas, f_uv);
	if(canvasColor.a >= 1.0) {
		c = canvasColor.bgr;
	} else {
		c = texture2D(u_checker, f_uvChecker).rgb * (1.0 - canvasColor.a)
			+ canvasColor.bgr;
	}

	if(u_gridScale > 0.0) {
		vec2 gridPos = mod(abs(f_pos), 1.0);
		if(min(gridPos.x, gridPos.y) < u_gridScale) {
			float lum = 0.3 * c.r + 0.59 * c.g + 0.11 * c.b;
			c = mix(c, vec3(lum < 0.2 ? 0.3 - lum : 0.0), 0.3);
		}
	}

	gl_FragColor = vec4(c, 1.0);
}
	)""";

		qCDebug(lcDpGlCanvas, "Create canvas shader program");
		CanvasShader canvasShader;
		canvasShader.program = f->glCreateProgram();

		qCDebug(
			lcDpGlCanvas, "Compile canvas vertex shader: %s", canvasVertexCode);
		GLuint canvasVertexShader =
			Private::compileShader(f, GL_VERTEX_SHADER, canvasVertexCode);

		qCDebug(
			lcDpGlCanvas, "Compile canvas fragment shader: %s",
			canvasFragmentCode);
		GLuint canvasFragmentShader =
			Private::compileShader(f, GL_FRAGMENT_SHADER, canvasFragmentCode);

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
			qCDebug(lcDpGlCanvas, "Get checker sampler uniform location");
			canvasShader.checkerLocation =
				f->glGetUniformLocation(canvasShader.program, "u_checker");
			canvasShader.gridScaleLocation =
				f->glGetUniformLocation(canvasShader.program, "u_gridScale");
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

	static OutlineShader initOutlineShader(QOpenGLFunctions *f)
	{
		static constexpr char outlineVertexCode[] = R"""(
#version 100

uniform vec2 u_view;
uniform vec2 u_pos;
uniform vec2 u_translation;
uniform mat3 u_transform;
attribute mediump vec2 v_pos;
varying vec2 f_uv;
varying vec2 f_pos;

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
#version 100
precision mediump float;

uniform float u_size;
uniform float u_width;
uniform float u_shape;

varying vec2 f_pos;

void main()
{
	float distance = u_shape < 0.5
		? abs(length(f_pos) - u_size + u_width) // Circle
		: abs(max(abs(f_pos.x), abs(f_pos.y)) - u_size + u_width); // Square
	if(distance <= u_width * 0.5) {
		gl_FragColor = vec4(0.5, 1.0, 0.5, 1.0);
	} else {
		discard;
	}
}
	)""";

		qCDebug(lcDpGlCanvas, "Create outline shader program");
		OutlineShader outlineShader;
		outlineShader.program = f->glCreateProgram();

		qCDebug(
			lcDpGlCanvas, "Compile outline vertex shader: %s",
			outlineVertexCode);
		GLuint outlineVertexShader =
			Private::compileShader(f, GL_VERTEX_SHADER, outlineVertexCode);

		qCDebug(
			lcDpGlCanvas, "Compile outline fragment shader: %s",
			outlineFragmentCode);
		GLuint outlineFragmentShader =
			Private::compileShader(f, GL_FRAGMENT_SHADER, outlineFragmentCode);

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

	void refreshTextureFilter(QOpenGLFunctions *f)
	{
		if(dirty.textureFilter) {
			dirty.textureFilter = false;
			bool linear = controller->shouldRenderSmooth();
			if(linear && !textureFilterLinear) {
				qCDebug(lcDpGlCanvas, "Set texture filter to linear");
				textureFilterLinear = true;
				f->glTexParameteri(
					GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				f->glTexParameteri(
					GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			} else if(!linear && textureFilterLinear) {
				qCDebug(lcDpGlCanvas, "Set texture filter to nearest");
				textureFilterLinear = false;
				f->glTexParameteri(
					GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
				f->glTexParameteri(
					GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			}
		}
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

	void setUpCanvasShader(QOpenGLFunctions *f)
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

		qreal pixelGridScale = controller->pixelGridScale();
		f->glUniform1f(
			canvasShader.gridScaleLocation,
			pixelGridScale > 0.0 ? 1.0 / pixelGridScale : 0.0);

		f->glEnableVertexAttribArray(0);
		f->glBindBuffer(
			GL_ARRAY_BUFFER, buffers[Private::CANVAS_UV_BUFFER_INDEX]);
		f->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

		f->glActiveTexture(GL_TEXTURE0);
	}

	void updateCanvasTextureFilter()
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
	}

	void
	drawCanvasShader(QOpenGLFunctions *f, const QRect &rect, GLint &inOutFilter)
	{
		GLint filter = textureFilterLinear ? GL_LINEAR : GL_NEAREST;
		if(inOutFilter != filter) {
			inOutFilter = filter;
			f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
			f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
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

	void
	renderCanvasDirtyTextures(QOpenGLFunctions *f, canvas::TileCache &tileCache)
	{
		qCDebug(lcDpGlCanvas, "renderCanvasDirtyTexture");
		setUpCanvasShader(f);
		updateCanvasTextureFilter();

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

	void renderCanvasDirty(QOpenGLFunctions *f)
	{
		// The loop is necessary because the canvas size may change
		// again while we're refreshing the tile cache visible area.
		while(true) {
			bool wasResized;
			canvas::TileCache::Resize resize;
			controller->withTileCache([&](canvas::TileCache &tileCache) {
				wasResized = tileCache.getResizeReset(resize);
				if(!wasResized) {
					renderCanvasDirtyTextures(f, tileCache);
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

	void renderCanvasClean(QOpenGLFunctions *f)
	{
		setUpCanvasShader(f);
		updateCanvasTextureFilter();
		int textureCount = canvasTextures.size();
		for(int i = 0; i < textureCount; ++i) {
			f->glBindTexture(GL_TEXTURE_2D, canvasTextures[i]);
			drawCanvasShader(f, canvasRects[i], canvasFilters[i]);
		}
	}

	void renderCanvas(QOpenGLFunctions *f)
	{
		// Rendering the canvas has three paths, to avoid state changes as much
		// as possible. When nothing changed since last time, we just render the
		// textures as-is. If the contents changed, but the size is the same, we
		// update the tiles and filtering of all visible textures and render
		// those out. Finally, if the size changed too, we reallocate the
		// textures entirely, rendering out each one along the way.
		if(dirty.texture) {
			dirty.texture = false;
			renderCanvasDirty(f);
		} else {
			renderCanvasClean(f);
		}
	}

	void renderOutline(QOpenGLFunctions *f, qreal dpr)
	{
		// Called after renderCanvas, assumes vertex attribute 0 is enabled.
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

	CanvasController *controller;
	QSize viewSize;
	QColor checkerColor1;
	QColor checkerColor2;
	Dirty dirty;
	bool initialized = false;
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
		d->dispose(QOpenGLContext::currentContext()->functions());
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

void GlCanvas::handleResize(QResizeEvent *event)
{
	resizeEvent(event);
}

void GlCanvas::handlePaint(QPaintEvent *event)
{
	paintEvent(event);
}

void GlCanvas::initializeGL()
{
	if(d->initialized) {
		qCWarning(lcDpGlCanvas, "Already initialized!");
		return;
	}
	d->initialized = true;

	QOpenGLContext *context = QOpenGLContext::currentContext();
	QOpenGLFunctions *f = context->functions();

	if(lcDpGlCanvas().isDebugEnabled()) {
		QPair<int, int> version = context->format().version();
		bool supportsDeprecatedFunctions =
			context->format().options() &
			QSurfaceFormat::FormatOption::DeprecatedFunctions;
		qCDebug(
			lcDpGlCanvas, "initializeGL %s %d.%d (%s)",
			context->isOpenGLES() ? "OpenGL ES" : "OpenGL", version.first,
			version.second, supportsDeprecatedFunctions ? "full" : "core");
		qCDebug(
			lcDpGlCanvas, "GL_RENDERER '%s'",
			qUtf8Printable(Private::getGlString(f, GL_RENDERER)));
		qCDebug(
			lcDpGlCanvas, "GL_VERSION '%s'",
			qUtf8Printable(Private::getGlString(f, GL_VERSION)));
		qCDebug(
			lcDpGlCanvas, "GL_VENDOR '%s'",
			qUtf8Printable(Private::getGlString(f, GL_VENDOR)));
		qCDebug(
			lcDpGlCanvas, "GL_SHADING_LANGUAGE_VERSION '%s'",
			qUtf8Printable(
				Private::getGlString(f, GL_SHADING_LANGUAGE_VERSION)));
	}

	GLint maxTextureSize = 0;
	f->glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
	if(maxTextureSize < 64) {
		qCWarning(
			lcDpGlCanvas, "Invalid max texture size %d, punting to %d",
			int(maxTextureSize), int(Private::FALLBACK_TEXTURE_SIZE));
		maxTextureSize = Private::FALLBACK_TEXTURE_SIZE;
	}
	d->maxTextureSize = maxTextureSize - maxTextureSize % 64;
	qCDebug(
		lcDpGlCanvas, "Max texture size %d (effectively %d)",
		int(maxTextureSize), int(d->maxTextureSize));

	d->canvasShader = Private::initCanvasShader(f);
	d->outlineShader = Private::initOutlineShader(f);

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
}

void GlCanvas::paintGL()
{
	qCDebug(lcDpGlCanvas, "paintGL");
	if(!d->viewSize.isEmpty()) {
		QPainter painter(this);
		painter.beginNativePainting();

		CanvasController *controller = d->controller;
		const QColor &clearColor = controller->clearColor();
		QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
		f->glClearColor(
			clearColor.redF(), clearColor.greenF(), clearColor.blueF(), 1.0);
		f->glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

		bool haveCanvas = controller->isCanvasVisible();
		if(haveCanvas) {
			d->renderCanvas(f);
			if(d->controller->isOutlineVisible()) {
				d->renderOutline(f, devicePixelRatioF());
			}
			// Qt5 doesn't reset these in endNativePainting.
			f->glDisableVertexAttribArray(0);
			f->glBindBuffer(GL_ARRAY_BUFFER, 0);
		}

		painter.endNativePainting();
		d->controller->scene()->render(&painter);
	}
}

void GlCanvas::resizeGL(int w, int h)
{
	qCDebug(lcDpGlCanvas, "resizeGL(%d, %d)", w, h);
	d->viewSize.setWidth(w);
	d->viewSize.setHeight(h);
	d->controller->scene()->setSceneRect(0.0, 0.0, qreal(w), qreal(h));
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
