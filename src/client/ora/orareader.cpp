/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2009-2013 Calle Laakkonen

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
#include <QScopedPointer>
#include <QColor>

#include "core/rasterop.h" // for blend modes
#include "ora/zipfile.h"
#include "ora/orareader.h"

#include "../shared/net/layer.h"
#include "../shared/net/annotation.h"
#include "net/utils.h"

using protocol::MessagePtr;

namespace openraster {

static const QString DP_NAMESPACE = "http://drawpile.sourceforge.net/";

Reader::Reader()
	: _error(QT_TR_NOOP("No error")), _warnings(NO_WARNINGS)
{
}

/**
 * If the load fails, you can get the (translateable) error message using \a error().
 * @return true on success
 */
bool Reader::load(const QString &filename)
{
	Zipfile zip(filename, Zipfile::READ);

	if(zip.open()==false) {
		_error = QApplication::tr("Error loading file: %1")
			.arg(zip.errorMessage());
		return false;
	}

	// Make sure this is an OpenRaster file
	{
		QScopedPointer<QIODevice> mimeio(zip.getFile("mimetype"));
		QString mimetype = mimeio->readLine();
		qDebug() << "read mimetype:" << mimetype;
		if(mimetype != "image/openraster") {
			_error = QApplication::tr("File is not an OpenRaster file");
			return false;
		}
	}

	// Read the stack
	QDomDocument doc;
	{
		QScopedPointer<QIODevice> stackio(zip.getFile("stack.xml"));
		if(stackio.isNull()) {
			_error = QApplication::tr(("Invalid OpenRaster file"));
			return false;
		}
		if(doc.setContent(stackio.data(), true, &_error) == false)
			return false;
	}

	const QDomElement stackroot = doc.documentElement().firstChildElement("stack");

	// Get the size of the image
	// These attributes are required by ORA standard.
	QSize imagesize(
			doc.documentElement().attribute("w").toInt(),
			doc.documentElement().attribute("h").toInt()
			);

	if(imagesize.isEmpty()) {
		_error = QApplication::tr("Image has zero size!");
		return false;
	}

	// Initialize the layer stack now that we know the size
	_commands.append(MessagePtr(new protocol::CanvasResize(imagesize.width(), imagesize.height())));

	_layerid = 0;
	_annotationid = 0;
	// Load the layer images
	if(loadLayers(zip, stackroot, QPoint()) == false)
		return false;

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

bool Reader::loadLayers(Zipfile &zip, const QDomElement& stack, QPoint offset)
{
	// TODO are layer coordinates relative to stack coordinates?
	// The spec, as of this writing, is not clear on this.
	offset += QPoint(
			stack.attribute("x", "0").toInt(),
			stack.attribute("y", "0").toInt()
			);

	QDomNodeList nodes = stack.childNodes();
	// Iterate backwards to get the layers in the right order (layers are always added to the top of the stack)
	for(int n=nodes.count()-1;n>=0;--n) {
		QDomElement e = nodes.at(n).toElement();
		if(e.isNull())
			continue;

		if(e.tagName()=="layer") {
			// Check for unknown attributes
			const char *layerattrs[] = {
					"x", "y", "name", "src", "opacity", "visibility", "composite-op", 0
			};
			if(!isKnown(e.attributes(), layerattrs))
				_warnings |= ORA_EXTENDED;

			// Load content image from the file
			const QString src = e.attribute("src");
			QImage content;
			{
				QScopedPointer<QIODevice> imgsrc(zip.getFile(src));
				if(imgsrc.isNull()) {
					_error = QApplication::tr("Couldn't get layer %1").arg(src);
					return false;
				}
				if(content.load(imgsrc.data(), "png")==false) {
					_error = QApplication::tr("Couldn't load layer %1").arg(src);
					return false;
				}
			}

			// Create layer
			QString name = e.attribute("name", QApplication::tr("Unnamed layer"));
			_commands.append(MessagePtr(new protocol::LayerCreate(
				++_layerid,
				0,
				name
			)));

			QPoint layerPos = offset + QPoint(
				e.attribute("x", "0").toInt(),
				e.attribute("y", "0").toInt()
				);
			_commands.append(net::putQImage(_layerid, layerPos.x(), layerPos.y(), content, false));

			QString compositeOp = e.attribute("composite-op", "src-over");
			int blendmode = dpcore::blendModeSvg(compositeOp);
			if(blendmode<0) {
				_warnings |= ORA_EXTENDED;
				blendmode = 1;
			}

			_commands.append(MessagePtr(new protocol::LayerAttributes(
				_layerid,
				qRound(255 * e.attribute("opacity", "1.0").toDouble()),
				blendmode
			)));

			// TODO visibility flag
			//layer->setHidden(e.attribute("visibility", "visible") != "visible");
		} else if(e.tagName()=="stack") {
			// Nested stacks are not fully supported
			_warnings |= ORA_NESTED;
			if(loadLayers(zip, e, offset)==false)
				return false;
		} else if(e.namespaceURI()==DP_NAMESPACE && e.localName()=="annotations") {
			loadAnnotations(e);
		} else if(e.namespaceURI()==DP_NAMESPACE) {
			qWarning() << "Unhandled drawpile extension in stack:" << e.tagName();
			_warnings |= ORA_EXTENDED;
		} else if(e.prefix()=="") {
			qWarning() << "Unhandled stack element:" << e.tagName();
			_warnings |= ORA_EXTENDED;
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
		if(e.namespaceURI()==DP_NAMESPACE && e.localName()=="a") {
			_commands.append(MessagePtr(new protocol::AnnotationCreate(
				0,
				++_annotationid,
				e.attribute("x").toInt(),
				e.attribute("y").toInt(),
				e.attribute("w").toInt(),
				e.attribute("h").toInt()
			)));
			_commands.append(MessagePtr(new protocol::AnnotationEdit(
				_annotationid,
				e.attribute("bg").mid(1).toUInt(0,16),
				e.text()
			)));
		} else {
			qWarning() << "unhandled annotations (DP ext.) element:" << e.tagName();
		}
	}
}

}
