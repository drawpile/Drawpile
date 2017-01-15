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
#include "textloader.h"
#include "net/client.h"
#include "net/commands.h"
#include "ora/orareader.h"
#include "canvas/canvasmodel.h"

#include "core/layerstack.h"
#include "core/layer.h"

#include "../shared/net/layer.h"
#include "../shared/net/annotation.h"
#include "../shared/net/meta2.h"
#include "../shared/net/image.h"

#include <QDebug>
#include <QApplication>
#include <QImage>
#include <QImageReader>

namespace canvas {

using protocol::MessagePtr;

QList<MessagePtr> BlankCanvasLoader::loadInitCommands()
{
	QList<MessagePtr> msgs;

	msgs.append(MessagePtr(new protocol::CanvasResize(1, 0, _size.width(), _size.height(), 0)));
	msgs.append(MessagePtr(new protocol::LayerCreate(1, 1, 0, _color.rgba(), 0, QApplication::tr("Background"))));
	msgs.append(MessagePtr(new protocol::LayerCreate(1, 2, 0, 0, 0, QApplication::tr("Foreground"))));
	return msgs;
}

QList<MessagePtr> ImageCanvasLoader::loadInitCommands()
{
	if(m_filename.endsWith(".ora", Qt::CaseInsensitive)) {
		// Load OpenRaster image
		using openraster::Reader;
		// TODO identify by filetype magic?
		Reader reader;

		if(reader.load(m_filename) == false) {
			m_error = reader.error();
			return QList<MessagePtr>();
		}

		if(reader.warnings() != Reader::NO_WARNINGS) {
			QString text = QApplication::tr("Drawpile does not support all the features used in this OpenRaster file. Saving this file may result in data loss.\n");
			if((reader.warnings() & Reader::ORA_EXTENDED))
				text += "\n- " + QApplication::tr("Application specific extensions are used");
			if((reader.warnings() & Reader::ORA_NESTED))
				text += "\n- " + QApplication::tr("Nested layers are not fully supported.");

			m_warning = text;
		}
		return reader.initCommands();
	} else if(m_filename.endsWith(".dptxt", Qt::CaseInsensitive)) {
		TextCommandLoader txt(filename());

		if(!txt.load()) {
			m_error = txt.errorMessage();
			return QList<MessagePtr>();
		}

		m_warning = txt.warningMessage();

		return txt.loadInitCommands();
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

	// Create layers
	for(int i=0;i<m_layers->layerCount();++i) {
		const paintcore::Layer *layer = m_layers->getLayerByIndex(i);

		const QColor fill = layer->isSolidColor();

		msgs.append(MessagePtr(new protocol::LayerCreate(m_contextId, layer->id(), 0, fill.isValid() ? fill.rgba() : 0, 0, layer->title())));
		msgs.append(MessagePtr(new protocol::LayerAttributes(m_contextId, layer->id(), layer->opacity(), 1)));

		if(!fill.isValid())
			msgs.append(net::command::putQImage(m_contextId, layer->id(), 0, 0, layer->toImage(), paintcore::BlendMode::MODE_REPLACE));

		if(m_session && m_session->stateTracker()->isLayerLocked(layer->id()))
			msgs.append(MessagePtr(new protocol::LayerACL(m_contextId, layer->id(), true, QList<uint8_t>())));
	}

	// Create annotations
	for(const paintcore::Annotation &a : m_layers->annotations()->getAnnotations()) {
		const QRect g = a.rect;
		msgs.append(MessagePtr(new protocol::AnnotationCreate(m_contextId, a.id, g.x(), g.y(), g.width(), g.height())));
		msgs.append((MessagePtr(new protocol::AnnotationEdit(m_contextId, a.id, a.background.rgba(), 0, 0, a.text))));
	}

	// User tool changes
	if(m_session) {
		QHashIterator<int, canvas::DrawingContext> iter(m_session->stateTracker()->drawingContexts());
		while(iter.hasNext()) {
			iter.next();

			msgs.append(net::command::brushToToolChange(
				iter.key(),
				iter.value().tool.layer_id,
				iter.value().tool.brush
			));
		}
	}

	return msgs;
}

}

