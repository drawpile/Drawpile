/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2009-2010 Calle Laakkonen

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
	: error_(QT_TR_NOOP("No error")), warnings_(NO_WARNINGS)
{
	ora_ = new Zipfile(filename, Zipfile::READ);
}

Reader::~Reader()
{
	delete ora_;
}

/**
 * If the load fails, you can get the (translateable) error message using \a error().
 * @return true on success
 */
bool Reader::load()
{
	if(ora_->open()==false) {
		error_ = QApplication::tr("Error loading file: %1")
			.arg(ora_->errorMessage());
		return false;
	}

	// Make sure this is an OpenRaster file
	QIODevice *mimeio = ora_->getFile("mimetype");
	QString mimetype = mimeio->readLine();
	delete mimeio;
	qDebug() << "read mimetype:" << mimetype;
	if(mimetype != "image/openraster") {
		error_ = QApplication::tr("File is not an OpenRaster file");
		return false;
	}

	// Read the stack
	QDomDocument doc;
	QIODevice *stackio = ora_->getFile("stack.xml");
	doc.setContent(stackio, true);
	delete stackio;

	stack_ = new dpcore::LayerStack();

	const QDomElement stackroot = doc.documentElement().firstChildElement("stack");

	// Get the size of the image
	// These attributes are required by ORA standard.
	QSize imagesize(
			doc.documentElement().attribute("w").toInt(),
			doc.documentElement().attribute("h").toInt()
			);
	if(imagesize.isEmpty()) {
		error_ = QApplication::tr("Image has zero size!");
		ora_->close();
		return false;
	}

	// Initialize the layer stack now that we know the size
	stack_->init(imagesize);

	// Load the layer images
	if(loadLayers(stackroot, QPoint()) == false) {
		ora_->close();
		return false;
	}

	ora_->close();
	return true;
}

/**
 * Check that the node map doesn't contain any unknown elements
 * @param attrs node map to check
 * @param names list of known names
 * @return false if node map contains unknown elements
 */
bool isKnown(const QDomNamedNodeMap& attrs, const char **names) {
	bool ok=true;
	for(int i=0;i<attrs.length();++i) {
		QDomNode n = attrs.item(i);
		const char **name = names;
		bool found=false;
		while(!found && *name!=0) {
			found = n.nodeName() == *name;
			++name;
		}
		if(!found) {
			qWarning() << "Unknown attribute" << n.nodeName();
			ok=false;
		}
	}
	return ok;
}

bool Reader::loadLayers(const QDomElement& stack, QPoint offset)
{
	// TODO are layer coordinates relative to stack coordinates?
	// The spec, as of this writing, is not clear on this.
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
			// Check for unknown attributes
			const char *layerattrs[] = {
					"x", "y", "name", "src", "opacity", "visibility", 0
			};
			if(!isKnown(e.attributes(), layerattrs))
				warnings_ |= ORA_EXTENDED;

			// Get the layer content file
			const QString src = e.attribute("src");
			QIODevice *imgsrc = ora_->getFile(src);
			if(imgsrc==0) {
				error_ = QApplication::tr("Couldn't get layer %1").arg(src);
				return false;
			}

			// Load content image from the file
			QImage content;
			if(content.load(imgsrc, "png")==false) {
				error_ = QApplication::tr("Couldn't load layer %1").arg(src);
				return false;
			}

			// Create layer
#if 0
			dpcore::Layer *layer = stack_->addLayer(
					0, // TODO proper ID
					e.attribute("name", QApplication::tr("Unnamed layer")),
					content,
					offset + QPoint(
						e.attribute("x", "0").toInt(),
						e.attribute("y", "0").toInt()
						)
					);
			layer->setOpacity(qRound(255 * e.attribute("opacity", "1.0").toDouble()));
			// TODO this isn't in the spec yet, but (at least) myPaint uses it.
			layer->setHidden(e.attribute("visibility", "visible") != "visible");
#endif
		} else if(e.tagName()=="stack") {
			// Nested stacks are not fully supported
			warnings_ |= ORA_NESTED;
			return loadLayers(e, offset);
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
