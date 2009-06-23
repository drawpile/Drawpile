/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2009 Calle Laakkonen

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
#include <QApplication>
#include <QImageReader>
#include <QDomDocument>
#include <QDebug>

#include "zipfile.h"
#include "orareader.h"

#include "../core/layerstack.h"
#include "../core/layer.h"

namespace openraster {

static const QString DP_NAMESPACE = "http://drawpile.sourceforge.net/";

Reader::Reader(const QString& filename)
	: error_("No error"), warnings_(NO_WARNINGS)
{
	ora = new Zipfile(filename, Zipfile::READ);
}

Reader::~Reader()
{
	delete ora;
}

/**
 * If the load fails, you can get the (translateable) error message using \a error().
 * @return true on success
 */
bool Reader::load()
{
	if(ora->open()==false) {
		error_ = QApplication::tr("Error loading file: %1")
			.arg(QApplication::tr(ora->errorMessage()));
		return false;
	}

	// Make sure this is an OpenRaster file
	QIODevice *mimeio = ora->getFile(0);
	QString mimetype = mimeio->readLine();
	delete mimeio;
	if(mimetype != "image/openraster") {
		error_ = QApplication::tr("File is not an OpenRaster file");
		return false;
	}

	// Read the stack
	QDomDocument doc;
	QIODevice *stackio = ora->getFile("stack.xml");
	doc.setContent(stackio, true);
	delete stackio;

	stack_ = new dpcore::LayerStack();

	const QDomElement stackroot = doc.documentElement().firstChildElement("stack");

	// Calculate the final image bounds. In DrawPile, all layers
	// are the same size, so we need to know this in advance.
	QRect bounds = computeBounds(stackroot, QPoint());
	if(bounds.isNull()) {
		ora->close();
		return false;
	}
	stack_->init(bounds.size());

	qDebug() << "image bounds" << bounds;

	// Load the layer images
	if(loadLayers(stack_, stackroot, QPoint()) == false) {
		ora->close();
		return false;
	}

	ora->close();
	return true;
}

/**
 * Calculate the final size of the layer stack
 * @param stack layer stack root element
 */
QRect Reader::computeBounds(const QDomElement& stack, QPoint offset)
{
	// Add stack coordinates to offset. Stack child element coordinates
	// are relative to parent.
	offset += QPoint(
			stack.attribute("x", "0").toInt(),
			stack.attribute("y", "0").toInt()
			);
	QRect area;
	QDomNodeList nodes = stack.childNodes();
	for(int n=nodes.count()-1;n>=0;--n) {
		QDomElement e = nodes.at(n).toElement();
		if(e.isNull())
			continue;
		if(e.tagName()=="layer") {
			// Get layer position (absolute)
			QPoint coords = QPoint(
					e.attribute("x", "0").toInt(),
					e.attribute("y", "0").toInt()
					) - offset;
			// See if a size has been defined for the layer
			QSize size(
					e.attribute("w", "0").toInt(),
					e.attribute("h", "0").toInt()
					);
			if(size.isEmpty()) {
				// If size is not defined, get it from
				// the layer source file
				const QString src = e.attribute("src");
				QIODevice *imgsrc = ora->getFile(src);
				if(imgsrc==0) {
					error_ = QApplication::tr("Couldn't get layer %1").arg(src);
					return QRect();
				}

				QImageReader image(imgsrc, src.mid(src.lastIndexOf('.')+1).toUtf8());
				size = image.size();
				delete imgsrc;
				if(size.isNull()) {
					error_ = QApplication::tr("Couldn't get layer %1 size").arg(src);
					return QRect();
				}
			}
			// Now we know the absolute dimensions of the layer.
			area |= QRect(coords, size);
		} else if(e.tagName()=="stack") {
			// Nested stack.
			area |= computeBounds(e, offset);
		}
	}
	return area;
}

bool Reader::loadLayers(dpcore::LayerStack *layers, const QDomElement& stack, QPoint offset)
{
	offset += QPoint(
			stack.attribute("x", "0").toInt(),
			stack.attribute("y", "0").toInt()
			);

	QDomNodeList nodes = stack.childNodes();
	// Iterate backwards to get the layers in the right order (addLayer always adds to the top of the stack)
	for(int n=nodes.count()-1;n>=0;--n) {
		QDomElement e = nodes.at(n).toElement();
		if(e.isNull())
			continue;

		if(e.tagName()=="layer") {
			// Get the layer content file
			const QString src = e.attribute("src");
			QIODevice *imgsrc = ora->getFile(src);
			if(imgsrc==0) {
				error_ = QApplication::tr("Couldn't get layer %1").arg(src);
				return false;
			}

			// Load content image from the file
			QImage content;
			if(content.load(imgsrc, src.mid(src.lastIndexOf('.')+1).toUtf8().constData())==false) {
				error_ = QApplication::tr("Couldn't load layer %1").arg(src);
				return false;
			}

			// Create layer
			dpcore::Layer *layer = layers->addLayer(
					e.attribute("name"),
					content,
					offset + QPoint(
						e.attribute("x", "0").toInt(),
						e.attribute("y", "0").toInt()
						)
					);
			layer->setOpacity(qRound(255 * e.attribute("opacity", "1.0").toDouble()));
		} else if(e.tagName()=="stack") {
			// Nested stacks are not fully supported
			warnings_ |= ORA_NESTED;
			return loadLayers(layers, e, offset);
		} else if(e.namespaceURI()==DP_NAMESPACE && e.localName()=="annotations") {
			loadAnnotations(e);
		} else if(e.namespaceURI()==DP_NAMESPACE) {
			qWarning() << "Unhandled drawpile extension in stack:" << e.tagName();
			warnings_ |= ORA_EXTENDED;
		} else if(e.prefix()=="") {
			qWarning() << "Unhandled stack element:" << e.tagName();
			warnings_ |= ORA_EXTENDED;
		}
	}
	return true;
}

void Reader::loadAnnotations(const QDomElement& annotations)
{
	QDomNodeList nodes = annotations.childNodes();
	for(int n=0;n<nodes.count();++n) {
		QDomElement e = nodes.at(n).toElement();
		if(e.isNull())
			continue;
		if(e.namespaceURI()==DP_NAMESPACE && e.localName()=="a")
			annotations_ << e.text();
		else
			qWarning() << "unhandled annotations (DP ext.) element:" << e.tagName();

	}
}

}
