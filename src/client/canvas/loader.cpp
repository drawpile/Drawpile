/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2015 Calle Laakkonen

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
#include "net/commands.h"
#include "ora/orareader.h"
#include "canvas/canvasmodel.h"
#include "canvas/layerlist.h"
#include "canvas/aclfilter.h"

#include "core/layerstack.h"
#include "core/layer.h"

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

QList<MessagePtr> BlankCanvasLoader::loadInitCommands()
{
	QList<MessagePtr> msgs;

	msgs.append(MessagePtr(new protocol::CanvasResize(1, 0, _size.width(), _size.height(), 0)));
	msgs.append(MessagePtr(new protocol::LayerCreate(1, 0x0101, 0, _color.rgba(), 0, QGuiApplication::tr("Background"))));
	msgs.append(MessagePtr(new protocol::LayerCreate(1, 0x0102, 0, 0, 0, QGuiApplication::tr("Foreground"))));
	return msgs;
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
				text += "\n- " + QGuiApplication::tr("Nested layers are not fully supported.");

			m_warning = text;
		}
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
				msgs << MessagePtr(new protocol::CanvasResize(1, 0, image.size().width(), image.size().height(), 0));
			}

			image = image.convertToFormat(QImage::Format_ARGB32);
			msgs << MessagePtr(new protocol::LayerCreate(1, layerId, 0, 0, 0, QStringLiteral("Layer %1").arg(layerId)));
			msgs << net::command::putQImage(1, layerId, 0, 0, image, paintcore::BlendMode::MODE_REPLACE);
			++layerId;
		}

		return msgs;
	}
}

QList<MessagePtr> QImageCanvasLoader::loadInitCommands()
{
	QList<MessagePtr> msgs;

	QImage image = _image.convertToFormat(QImage::Format_ARGB32);

	msgs.append(MessagePtr(new protocol::CanvasResize(1, 0, image.size().width(), image.size().height(), 0)));
	msgs.append(MessagePtr(new protocol::LayerCreate(1, 1, 0, 0, 0, "Background")));
	msgs.append(net::command::putQImage(1, 1, 0, 0, image, paintcore::BlendMode::MODE_REPLACE));

	return msgs;
}

QList<MessagePtr> SnapshotLoader::loadInitCommands()
{
	QList<MessagePtr> msgs;

	// Most important bit first: canvas initialization
	const QSize imgsize = m_layers->size();
	msgs.append(MessagePtr(new protocol::CanvasResize(m_contextId, 0, imgsize.width(), imgsize.height(), 0)));

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

		const QColor fill = layer->isSolidColor();

		msgs.append(MessagePtr(new protocol::LayerCreate(m_contextId, layer->id(), 0, fill.isValid() ? fill.rgba() : 0, 0, layer->title())));
		msgs.append(MessagePtr(new protocol::LayerAttributes(m_contextId, layer->id(), layer->opacity(), 1)));

		if(!fill.isValid())
			msgs.append(net::command::putQImage(m_contextId, layer->id(), 0, 0, layer->toImage(), paintcore::BlendMode::MODE_REPLACE));

		AclFilter::LayerAcl acl;
		if(m_session)
			acl = m_session->aclFilter()->layerAcl(layer->id());
		if(acl.locked || !acl.exclusive.isEmpty())
			msgs.append(MessagePtr(new protocol::LayerACL(m_contextId, layer->id(), acl.locked, acl.exclusive)));
	}

	// Create annotations
	for(const paintcore::Annotation &a : m_layers->annotations()->getAnnotations()) {
		const QRect g = a.rect;
		msgs.append(MessagePtr(new protocol::AnnotationCreate(m_contextId, a.id, g.x(), g.y(), g.width(), g.height())));
		msgs.append((MessagePtr(new protocol::AnnotationEdit(m_contextId, a.id, a.background.rgba(), a.flags(), 0, a.text))));
	}

	// Session and user ACLs
	if(m_session) {
		// Note: Starting the reset process automatically sets the LOCK_SESSION flag. We don't want that after the reset.
		msgs.append(MessagePtr(new protocol::SessionACL(m_contextId,
						m_session->aclFilter()->sessionAclFlags() & ~protocol::SessionACL::LOCK_SESSION)));
		msgs.append(MessagePtr(new protocol::UserACL(m_contextId, m_session->aclFilter()->lockedUsers())));
	}

	return msgs;
}

}

