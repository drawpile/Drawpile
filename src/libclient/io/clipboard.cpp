// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/io/clipboard.h"
#include <QClipboard>
#include <QGuiApplication>
#ifdef Q_OS_LINUX
#	include <QBuffer>
#endif

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
		QByteArray bytes;
		{
			QBuffer buffer(&bytes);
			buffer.open(QIODevice::WriteOnly);
			img.save(&buffer, "PNG");
		}
		m_data->setData(QStringLiteral("image/png"), bytes);
	} else {
		m_data->setImageData(img);
	}
#else
	m_data->setImageData(img);
#endif
}

}
}
