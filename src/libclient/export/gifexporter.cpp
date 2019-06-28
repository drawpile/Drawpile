/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <QImage>

#include <gif_lib.h>

#include "gifexporter.h"

#if !defined(GIFLIB_MAJOR) || GIFLIB_MAJOR < 5
#define OLD_API
#define GifMakeMapObject MakeMapObject
#define GifFreeMapObject FreeMapObject
#define EGifCloseFile(a, b) EGifCloseFile(a)
#endif

struct GifExporter::Private {
	QString path;
	Qt::ImageConversionFlags conversionFlags;
	bool optimize;

	GifFileType *gif;

	QImage prevImage;

	Private() : gif(nullptr) { }
};

GifExporter::GifExporter(QObject *parent)
	: VideoExporter(parent), p(new Private)
{
}

GifExporter::~GifExporter()
{
	if(p->gif) {
		int errorcode;
		EGifCloseFile(p->gif, &errorcode);
	}

	delete p;
}

void GifExporter::setFilename(const QString &path)
{
	p->path = path;
}

void GifExporter::setDithering(DitheringMode mode)
{
	p->conversionFlags = Qt::ColorOnly | Qt::AvoidDither;
	switch(mode) {
		case DIFFUSE: p->conversionFlags |= Qt::DiffuseDither; break;
		case ORDERED: p->conversionFlags |= Qt::OrderedDither; break;
		case THRESHOLD: p->conversionFlags |= Qt::ThresholdDither; break;
	}
}

void GifExporter::setOptimize(bool optimze)
{
	p->optimize = optimze;
}

static QString gifErrorQString(int error)
{
#ifdef OLD_API
	return QString("Gif error %1").arg(GifLastError());
#else
	return QString::fromUtf8(GifErrorString(error));
#endif
}

struct Subframe {
	quint16 x, y, w, h;
	QImage frame;
};

static Subframe optimizeFrame(const QImage &prev, const QImage &current)
{
	Q_ASSERT(prev.size() == current.size());

	const int w = current.width();
	const int h = current.height();

	// Find bounding rectangle of differing pixels
	int x1=w, y1=h, x2=0, y2=0;
	for(int y=0;y<h;++y) {
		for(int x=0;x<w;++x) {
			if(prev.pixel(x, y) != current.pixel(x, y)) {
				x1 = qMin(x1, x);
				x2 = qMax(x2, x);
				y1 = qMin(y1, y);
				y2 = qMax(y2, y);
			}
		}
	}

	if(x1==w) {
		// No difference between frames found!
		x1=0; y1=0;
		x2=0; y2=0;
	}

	// Extract changed pixels
	Subframe s {
		quint16(x1), quint16(y1),
		quint16(x2-x1+1), quint16(y2-y1+1),
		QImage()
	};
	s.frame = current.copy(s.x, s.y, s.w, s.h);
	return s;
}

void GifExporter::writeFrame(const QImage &image, int repeat)
{
	Q_ASSERT(repeat>0);
	Q_ASSERT(image.size() == framesize());

	// Frame duration in 1/100 seconds
	int delay = qMax(1, repeat * 100 / fps());

	// Extract changed part of the image if frame optimization is enabled
	Subframe subframe {0, 0, 0, 0, QImage() };

	if(p->optimize) {
		if(!p->prevImage.isNull())
			subframe = optimizeFrame(p->prevImage, image);
		p->prevImage = image;
	}

	if(subframe.frame.isNull()) {
		subframe.frame = image;
		subframe.w = quint16(image.width());
		subframe.h = quint16(image.height());
	}

	// Convert to 8-bit indexed
	subframe.frame = subframe.frame.convertToFormat(QImage::Format_Indexed8, p->conversionFlags);

	// Get the image palette
	// note: palette size must be a power of two
	ColorMapObject *palette = GifMakeMapObject(256, nullptr);
	Q_ASSERT(subframe.frame.colorCount() > 0 && subframe.frame.colorCount() <= 256);
	for(int i=0;i<subframe.frame.colorCount();++i) {
		const QRgb c = subframe.frame.color(i);
		palette->Colors[i].Red = qRed(c);
		palette->Colors[i].Green = qGreen(c);
		palette->Colors[i].Blue = qBlue(c);
	}

	// Write frame headers
	const uchar extcode[4] = {
		0x04,                // disposal method (leave previous frame in place, no transparency)
		uchar(delay & 0xff), // frame delay in 1/100 seconds
		uchar(delay >> 8),   // (little endian)
		0x00                 // transparency index (not used)
	};
	EGifPutExtension(p->gif, GRAPHICS_EXT_FUNC_CODE, 4, extcode);
	EGifPutImageDesc(p->gif, subframe.x, subframe.y, subframe.w, subframe.h, false, palette);

	GifFreeMapObject(palette);

	// Write pixel data
	for(int y=0;y<subframe.h;++y) {
		if(EGifPutLine(p->gif, subframe.frame.scanLine(y), subframe.w) == GIF_ERROR) {
#ifdef OLD_API
			emit exporterError(gifErrorQString(0));
#else
			emit exporterError(gifErrorQString(p->gif->Error));
#endif
			return;
		}
	}

	emit exporterReady();
}

void GifExporter::initExporter()
{
	Q_ASSERT(!p->path.isEmpty());

	int errorcode=0;
#ifdef OLD_API
	EGifSetGifVersion("89a");
	p->gif = EGifOpenFileName(p->path.toLocal8Bit().constData(), false);
#else
	p->gif = EGifOpenFileName(p->path.toLocal8Bit().constData(), false, &errorcode);
#endif
	if(!p->gif) {
		emit exporterError(gifErrorQString(errorcode));
		return;
	}

	emit exporterReady();
}

void GifExporter::startExporter()
{
	// Main header
	// note: no global palette, each frame has its own.
	if(EGifPutScreenDesc(p->gif, framesize().width(), framesize().height(), 8, 0, nullptr) == GIF_ERROR) {
#ifdef OLD_API
		emit exporterError(gifErrorQString(0));
#else
		emit exporterError(gifErrorQString(p->gif->Error));
#endif
		return;
	}

	// Make looping GIF
	const char *nsle = "NETSCAPE2.0";
	const char subblock[] = {
		1, // always 1
		0, // little-endian loop counter:
		0  // 0 for infinite loop.
	};
#ifdef OLD_API
	EGifPutExtensionFirst(p->gif, APPLICATION_EXT_FUNC_CODE, 11, nsle);
	EGifPutExtensionLast(p->gif, APPLICATION_EXT_FUNC_CODE, 3, subblock);
#else
	EGifPutExtensionLeader(p->gif, APPLICATION_EXT_FUNC_CODE);
	EGifPutExtensionBlock(p->gif, 11, nsle);
	EGifPutExtensionBlock(p->gif, 3, subblock);
	EGifPutExtensionTrailer(p->gif);
#endif
}

void GifExporter::shutdownExporter()
{
	if(p->gif) {
		int errorcode;
		EGifCloseFile(p->gif, &errorcode);
		p->gif = nullptr;
	}
	emit exporterFinished();
}

