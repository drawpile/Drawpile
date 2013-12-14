/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2013 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include <QDebug>
#include <QApplication>
#include <QImage>
#include <QMessageBox>

#include "loader.h"
#include "textloader.h"
#include "net/client.h"
#include "net/utils.h"
#include "ora/orareader.h"
#include "canvasscene.h"
#include "annotationitem.h"
#include "statetracker.h"
#include "core/layerstack.h"
#include "core/layer.h"

#include "../shared/net/layer.h"
#include "../shared/net/annotation.h"
#include "../shared/net/meta.h"
#include "../shared/net/image.h"

using protocol::MessagePtr;

QList<MessagePtr> BlankCanvasLoader::loadInitCommands()
{
	QList<MessagePtr> msgs;
	msgs.append(MessagePtr(new protocol::CanvasResize(_size.width(), _size.height())));
	msgs.append(MessagePtr(new protocol::LayerCreate(1, 1, _color.rgba(), "Background")));
	return msgs;
}

QList<MessagePtr> ImageCanvasLoader::loadInitCommands()
{
	if(_filename.endsWith(".ora", Qt::CaseInsensitive)) {
		// Load OpenRaster image
		using openraster::Reader;
		// TODO identify by filetype magic?
		Reader reader;

		if(reader.load(_filename) == false) {
			_error = reader.error();
			return QList<MessagePtr>();
		}

		if(reader.warnings() != Reader::NO_WARNINGS) {
			QString text = QApplication::tr("DrawPile does not support all the features used in this OpenRaster file. Saving this file may result in data loss.\n");
			if((reader.warnings() & Reader::ORA_EXTENDED))
				text += "\n- " + QApplication::tr("Application specific extensions are used");
			if((reader.warnings() & Reader::ORA_NESTED))
				text += "\n- " + QApplication::tr("Nested layers are not fully supported.");
			QMessageBox::warning(0, QApplication::tr("Partially supported OpenRaster"), text);
		}
		return reader.initCommands();
	} else if(_filename.endsWith(".dptxt", Qt::CaseInsensitive)) {
		TextCommandLoader txt(filename());

		if(!txt.load()) {
			_error = txt.errorMessage();
			return QList<MessagePtr>();
		}

		return txt.loadInitCommands();
	} else {
		// Load a simple single-layer image using Qt's image loader
		QList<MessagePtr> msgs;

		QImage image;
		if(image.load(_filename)==false) {
			_error = "Couldn't load file"; // TODO get proper error message
			return msgs;
		}

		QImageCanvasLoader imgloader(image);
		return imgloader.loadInitCommands();
	}
}

QList<MessagePtr> QImageCanvasLoader::loadInitCommands()
{
	QList<MessagePtr> msgs;

	QImage image = _image.convertToFormat(QImage::Format_ARGB32);

	msgs.append(MessagePtr(new protocol::CanvasResize(image.size().width(), image.size().height())));
	msgs.append(MessagePtr(new protocol::LayerCreate(1, 1, 0, "Background")));
	msgs.append(net::putQImage(1, 1, 0, 0, image, false));

	return msgs;
}

QList<MessagePtr> SnapshotLoader::loadInitCommands()
{
	QList<MessagePtr> msgs;

	// Most important bit first: canvas initialization
	msgs.append(MessagePtr(new protocol::CanvasResize(_scene->width(), _scene->height())));

	// Less important, but it's nice to see it straight away
	if(!_scene->title().isEmpty())
		msgs.append((MessagePtr(new protocol::SessionTitle(_scene->title()))));

	// Create layers
	for(int i=0;i<_scene->layers()->layers();++i) {
		const dpcore::Layer *layer = _scene->layers()->getLayerByIndex(i);
		msgs.append(MessagePtr(new protocol::LayerCreate(1, layer->id(), 0, layer->title())));
		msgs.append(MessagePtr(new protocol::LayerAttributes(layer->id(), layer->opacity(), 1)));
		msgs.append(net::putQImage(1, layer->id(), 0, 0, layer->toImage(), false));
	}

	// Create annotations
	foreach(const drawingboard::AnnotationItem *a, _scene->getAnnotations()) {
		QRect g = a->geometry();
		msgs.append(MessagePtr(new protocol::AnnotationCreate(0, a->id(), g.x(), g.y(), g.width(), g.height())));
		msgs.append((MessagePtr(new protocol::AnnotationEdit(a->id(), a->backgroundColor().rgba(), a->text()))));
	}

	// User tool changes
	QHashIterator<int, drawingboard::DrawingContext> iter(_scene->statetracker()->drawingContexts());
	while(iter.hasNext()) {
		iter.next();

		msgs.append(net::brushToToolChange(
			iter.key(),
			iter.value().tool.layer_id,
			iter.value().tool.brush
		));
	}

	return msgs;
}
