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
#include "ora/orareader.h"
#include "canvas/canvasmodel.h"
#include "canvas/layerlist.h"
#include "canvas/aclfilter.h"

#include "core/layerstack.h"
#include "core/layer.h"
#include "core/tilevector.h"

#include "../shared/net/layer.h"
#include "../shared/net/annotation.h"
#include "../shared/net/meta.h"
#include "../shared/net/meta2.h"
#include "../shared/net/image.h"

#include <QDebug>
#include <QGuiApplication>
#include <QImage>
#include <QImageReader>

namespace canvas {

using protocol::MessagePtr;

SessionLoader::~SessionLoader()
{
}

QList<MessagePtr> BlankCanvasLoader::loadInitCommands()
{
	return QList<MessagePtr>()
		<< MessagePtr(new protocol::CanvasResize(1, 0, _size.width(), _size.height(), 0))
		<< MessagePtr(new protocol::CanvasBackground(1, _color.rgba()))
		<< MessagePtr(new protocol::LayerCreate(1, 0x0102, 0, 0, 0, QStringLiteral("Layer 1")))
		;
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

QList<MessagePtr> ImageCanvasLoader::loadInitCommands()
{
	if(m_filename.endsWith(".ora", Qt::CaseInsensitive)) {
		// Load OpenRaster image
		// TODO identify by filetype magic?
		openraster::OraResult ora = openraster::loadOpenRaster(m_filename);

		if(!ora.error.isEmpty()) {
			m_error = ora.error;
			return QList<MessagePtr>();
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
		QList<MessagePtr> msgs;
		QImageReader ir(m_filename);
		int layerId = 1;

		while(true) {
			QImage image = ir.read();

			if(image.isNull()) {
				if(layerId>1)
					break;
				m_error = ir.errorString();
				return QList<MessagePtr>();
			}

			if(layerId==1) {
				m_dpi = QPair<int,int>(int(image.dotsPerMeterX() * 0.0254), int(image.dotsPerMeterY() * 0.0254));
				msgs << MessagePtr(new protocol::CanvasResize(1, 0, image.size().width(), image.size().height(), 0));
			}

			msgs << paintcore::LayerTileSet::fromImage(
				image.convertToFormat(QImage::Format_ARGB32_Premultiplied)
				).toInitCommands(1, paintcore::LayerInfo(layerId, QStringLiteral("Layer %1").arg(layerId)));

			++layerId;
		}

		return msgs;
	}
}

QList<MessagePtr> QImageCanvasLoader::loadInitCommands()
{
	QList<MessagePtr> msgs;

	msgs << MessagePtr(new protocol::CanvasResize(1, 0, m_image.size().width(), m_image.size().height(), 0));

	msgs << paintcore::LayerTileSet::fromImage(
		m_image.convertToFormat(QImage::Format_ARGB32_Premultiplied)
		).toInitCommands(1, paintcore::LayerInfo(1, QStringLiteral("Layer 1")));

	return msgs;
}

QList<MessagePtr> SnapshotLoader::loadInitCommands()
{
	QList<MessagePtr> msgs;

	// Most important bit first: canvas initialization
	const QSize imgsize = m_layers->size();
	msgs.append(MessagePtr(new protocol::CanvasResize(m_contextId, 0, imgsize.width(), imgsize.height(), 0)));

	const QColor solidBgColor = m_layers->background().solidColor();
	if(solidBgColor.isValid())
		msgs.append(MessagePtr(new protocol::CanvasBackground(m_contextId, solidBgColor.rgba())));
	else
		msgs.append(MessagePtr(new protocol::CanvasBackground(
			m_contextId,
			qCompress(reinterpret_cast<const uchar*>(m_layers->background().constData()), paintcore::Tile::BYTES)
			)));

	// Preset default layer
	if(m_session && m_session->layerlist()->defaultLayer()>0)
		msgs.append(MessagePtr(new protocol::DefaultLayer(m_contextId, m_session->layerlist()->defaultLayer())));

	// Add pinned message (if any)
	if(m_session && !m_session->pinnedMessage().isEmpty()) {
		msgs.append(protocol::Chat::pin(m_contextId, m_session->pinnedMessage()));
	}

	// Create layers
	for(int i=0;i<m_layers->layerCount();++i) {
		const paintcore::Layer *layer = m_layers->getLayerByIndex(i);

		msgs << paintcore::LayerTileSet::fromLayer(*layer)
			.toInitCommands(m_contextId, layer->info());

		// Set layer ACLs (if found)
		if(m_session) {
			const canvas::AclFilter::LayerAcl acl = m_session->aclFilter()->layerAcl(layer->id());
			if(acl.locked || acl.tier != canvas::Tier::Guest || !acl.exclusive.isEmpty())
				msgs << MessagePtr(new protocol::LayerACL(m_contextId, layer->id(), acl.locked, int(acl.tier), acl.exclusive));
		}
	}

	// Create annotations
	for(const paintcore::Annotation &a : m_layers->annotations()->getAnnotations()) {
		const QRect g = a.rect;
		msgs.append(MessagePtr(new protocol::AnnotationCreate(m_contextId, a.id, g.x(), g.y(), g.width(), g.height())));
		msgs.append((MessagePtr(new protocol::AnnotationEdit(m_contextId, a.id, a.background.rgba(), a.flags(), 0, a.text))));
	}

	// Session and user ACLs
	if(m_session) {
		uint8_t features[canvas::FeatureCount];
		for(int i=0;i<canvas::FeatureCount;++i)
			features[i] = uint8_t(m_session->aclFilter()->featureTier(Feature(i)));

		msgs.append(MessagePtr(new protocol::FeatureAccessLevels(m_contextId, features)));
		msgs.append(MessagePtr(new protocol::UserACL(m_contextId, m_session->aclFilter()->lockedUsers())));
	}

	return msgs;
}

QPair<int,int> SnapshotLoader::dotsPerInch() const
{
	return m_layers->dotsPerInch();
}

}

