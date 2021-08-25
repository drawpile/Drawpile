/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2019 Calle Laakkonen

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

#include "loader.h"
#include "net/client.h"
#include "net/envelopebuilder.h"
#include "ora/orareader.h"

#include "../rustpile/rustpile.h"

#include <QDebug>
#include <QGuiApplication>
#include <QImage>
#include <QPixmap>
#include <QImageReader>

namespace canvas {

using protocol::MessageList;
using protocol::MessagePtr;

SessionLoader::~SessionLoader()
{
}

net::Envelope BlankCanvasLoader::loadInitCommands()
{
	const uint32_t color = qToBigEndian(m_color.rgba());
	const QString layerName = QStringLiteral("Layer 1"); // TODO i18n

	net::EnvelopeBuilder eb;
	rustpile::write_resize(eb, 1, 0, m_size.width(), m_size.height(), 0);
	rustpile::write_background(eb, 1, reinterpret_cast<const uint8_t*>(&color), 4);
	rustpile::write_newlayer(eb, 1, 0x0101, 0, 0, 0, reinterpret_cast<const uint16_t*>(layerName.constData()), layerName.length());

	return eb.toEnvelope();
}

QPixmap ImageCanvasLoader::loadThumbnail(const QSize &maxSize) const
{
	QImage thumbnail;

	if(m_filename.endsWith(".ora", Qt::CaseInsensitive))
		thumbnail = openraster::loadOpenRasterThumbnail(m_filename);
	else
		thumbnail.load(m_filename);

	if(thumbnail.isNull())
		return QPixmap();

	if(thumbnail.width() > maxSize.width() || thumbnail.height() > maxSize.height()) {
		thumbnail = thumbnail.scaled(maxSize, Qt::KeepAspectRatio);
	}

	return QPixmap::fromImage(thumbnail);
}

net::Envelope ImageCanvasLoader::loadInitCommands()
{
#if 0 // FIXME
	if(m_filename.endsWith(".ora", Qt::CaseInsensitive)) {
		// Load OpenRaster image
		// TODO identify by filetype magic?
		openraster::OraResult ora = openraster::loadOpenRaster(m_filename);

		if(!ora.error.isEmpty()) {
			m_error = ora.error;
			return MessageList();
		}

		if(ora.warnings != openraster::OraResult::NO_WARNINGS) {
			QString text = QGuiApplication::tr("Drawpile does not support all the features used in this OpenRaster file. Saving this file may result in data loss.\n");
			if((ora.warnings & openraster::OraResult::ORA_EXTENDED))
				text += "\n- " + QGuiApplication::tr("Application specific extensions are used");
			if((ora.warnings & openraster::OraResult::ORA_NESTED))
				text += "\n- " + QGuiApplication::tr("Nested layers are not fully supported");
			if((ora.warnings & openraster::OraResult::UNSUPPORTED_BACKGROUND_TILE))
				text += "\n- " + QGuiApplication::tr("Unsupported background tile size");

			m_warning = text;
		}
		m_dpi = QPair<int,int>(ora.dpiX, ora.dpiY);
		return ora.commands;

	} else {
		// Load an image using Qt's image loader.
		// If the image is animated, each frame is loaded as a layer
		MessageList msgs;
		QImageReader ir(m_filename);
		int layerId = 1;

		while(true) {
			QImage image = ir.read();

			if(image.isNull()) {
				if(layerId>1)
					break;
				m_error = ir.errorString();
				return MessageList();
			}

			if(layerId==1) {
				m_dpi = QPair<int,int>(int(image.dotsPerMeterX() * 0.0254), int(image.dotsPerMeterY() * 0.0254));
				msgs << MessagePtr(new protocol::CanvasResize(1, 0, image.size().width(), image.size().height(), 0));
			}

			const auto tileset = paintcore::LayerTileSet::fromImage(
				image.convertToFormat(QImage::Format_ARGB32_Premultiplied)
				);

			msgs << protocol::MessagePtr(new protocol::LayerCreate(
				1,
				layerId,
				0,
				tileset.background.rgba(),
				0,
				QStringLiteral("Layer %1").arg(layerId)
			));

			msgs << protocol::MessagePtr(new protocol::LayerAttributes(
				1,
				layerId,
				0,
				0,
				255,
				paintcore::BlendMode::MODE_NORMAL
			));

			tileset.toPutTiles(1, layerId, 0, msgs);

			++layerId;

			if(!ir.supportsAnimation()) {
				// Don't try to read any more frames if this format
				// does not support animation.
				break;
			}
		}

		return msgs;
	}
#endif
	return net::Envelope();
}

}

