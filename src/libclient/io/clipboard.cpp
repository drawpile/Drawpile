// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/io/clipboard.h"
#include <QBuffer>
#include <QClipboard>
#include <QDataStream>
#include <QGuiApplication>

namespace io {
namespace clip {

#ifdef HAVE_CLIPBOARD_EMULATION
namespace {
static QMimeData clipboardData;
}
#endif

const QMimeData *mimeData()
{
#ifdef HAVE_CLIPBOARD_EMULATION
	return &clipboardData;
#else
	return QGuiApplication::clipboard()->mimeData();
#endif
}

bool hasImage()
{
	const QMimeData *data = mimeData();
	return data &&
		   (data->hasImage() || data->hasFormat(QStringLiteral("image/png")));
}

QImage getImageFrom(const QMimeData *data)
{
	if(data) {
		if(data->hasImage()) {
			return data->imageData().value<QImage>();
		} else if(data->hasFormat(QStringLiteral("image/png"))) {
			QImage img;
			if(img.loadFromData(
				   data->data(QStringLiteral("image/png")), "PNG")) {
				return img;
			}
		}
	}
	return QImage();
}

QImage getSourceImageFrom(const QMimeData *data)
{
	if(!data) {
		return QImage();
	}

	if(!data->hasFormat(QStringLiteral("x-drawpile/srcimg"))) {
		return QImage();
	}

	QImage img;
	if(!img.loadFromData(
		   data->data(QStringLiteral("x-drawpile/srcimg")), "PNG")) {
		return QImage();
	}

	return img;
}

TransformState getTransformStateFrom(const QMimeData *data)
{
	if(!data) {
		return TransformState();
	}

	if(!data->hasFormat(QStringLiteral("x-drawpile/transformstate"))) {
		return TransformState();
	}

	QByteArray bytes = data->data(QStringLiteral("x-drawpile/transformstate"));
	QDataStream stream(bytes);
	QPointF topLeft, topRight, bottomRight, bottomLeft;
	QRect srcBounds;
	float opacity;
	int blendMode, handleMode;
	bool constrain, center;
	stream >> topLeft >> topRight >> bottomRight >> bottomLeft >> srcBounds >>
		opacity >> blendMode >> handleMode >> constrain >> center;
	if(stream.status() != QDataStream::Ok) {
		qWarning("Clipboard: invalid transform state data");
		return TransformState();
	}

	return {
		TransformQuad(topLeft, topRight, bottomRight, bottomLeft),
		srcBounds,
		opacity,
		blendMode,
		handleMode,
		constrain,
		center,
	};
}

QByteArray getLiquifyDataFrom(const QMimeData *data)
{
	if(!data) {
		return QByteArray();
	}

	if(!data->hasFormat(QStringLiteral("x-drawpile/liquify"))) {
		return QByteArray();
	}

	return data->data(QStringLiteral("x-drawpile/liquify"));
}


EditableMimeData::EditableMimeData()
	: m_data(
#ifdef HAVE_CLIPBOARD_EMULATION
		  &clipboardData
#else
		  new QMimeData
#endif
	  )
{
#ifdef HAVE_CLIPBOARD_EMULATION
	m_data->clear();
#endif
}

EditableMimeData::~EditableMimeData()
{
#ifndef HAVE_CLIPBOARD_EMULATION
	QGuiApplication::clipboard()->setMimeData(m_data);
#endif
}

void EditableMimeData::setData(const QString &mimeType, const QByteArray &data)
{
	m_data->setData(mimeType, data);
}

void EditableMimeData::setImageData(const QImage &img)
{
#ifdef Q_OS_LINUX
	// Copying image data from one Drawpile canvas to another is busted on
	// Wayland, obliterating transparency. We store a PNG directly instead.
	if(QGuiApplication::platformName() == QStringLiteral("wayland")) {
		setPngData(QStringLiteral("image/png"), img);
	} else {
		m_data->setImageData(img);
	}
#else
	m_data->setImageData(img);
#endif
}

void EditableMimeData::setSourceImage(const QImage &img)
{
	if(img.isNull()) {
		qWarning("Clipboard: source image is null");
	} else {
		setPngData(QStringLiteral("x-drawpile/srcimg"), img);
	}
}

void EditableMimeData::setTransformState(const TransformState &state)
{
	if(state.isNull()) {
		qWarning("Clipboard: transform state is null");
	} else {
		QByteArray bytes;
		{
			QDataStream stream(&bytes, QIODevice::WriteOnly);
			stream << state.quad.topLeft();
			stream << state.quad.topRight();
			stream << state.quad.bottomRight();
			stream << state.quad.bottomLeft();
			stream << state.srcBounds;
			stream << state.opacity;
			stream << state.blendMode;
			stream << state.handleMode;
			stream << state.constrain;
			stream << state.center;
		}
		m_data->setData(QStringLiteral("x-drawpile/transformstate"), bytes);
	}
}

void EditableMimeData::setLiquifyData(const QByteArray &data)
{
	if(data.isEmpty()) {
		qWarning("Clipboard: liquify data is null");
	} else {
		m_data->setData(QStringLiteral("x-drawpile/liquify"), data);
	}
}

void EditableMimeData::setPngData(const QString &mimeType, const QImage &img)
{
	QByteArray bytes;
	{
		QBuffer buffer(&bytes);
		buffer.open(QIODevice::WriteOnly);
		img.save(&buffer, "PNG");
	}
	m_data->setData(mimeType, bytes);
}

}
}
