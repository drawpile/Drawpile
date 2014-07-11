/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2009-2014 Calle Laakkonen

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

#include "core/rasterop.h" // for blend modes
#include "ora/orareader.h"

#include "../shared/net/layer.h"
#include "../shared/net/annotation.h"
#include "net/utils.h"
#include "utils/archive.h"

#include <QApplication>
#include <QImageReader>
#include <QDomDocument>
#include <QDebug>
#include <QScopedPointer>
#include <QColor>
#include <QFile>

#include <KZip>

using protocol::MessagePtr;

namespace openraster {

namespace {
	const QString DP_NAMESPACE = "http://drawpile.sourceforge.net/";

	bool checkIsOraFile(KArchive &zip)
	{
		const QByteArray expected = "image/openraster";
		QByteArray mimetype = utils::getArchiveFile(zip, "mimetype", expected.length());

		return mimetype == expected;
	}
}

Reader::Reader()
	: _error(QT_TR_NOOP("No error")), _warnings(NO_WARNINGS)
{
}

QImage Reader::loadThumbnail(const QString &filename)
{
	QImage image;

	KZip zip(filename);
	if(zip.open(QIODevice::ReadOnly))
		return image;

	// Make sure this is an OpenRaster file
	if(!checkIsOraFile(zip))
		return image;

	// Load thumbnail
	QByteArray imgdata = utils::getArchiveFile(zip, "Thumbnails/thumbnail.png");
	if(imgdata.isNull())
		return image;

	image.loadFromData(imgdata, "png");

	return image;
}

/**
 * If the load fails, you can get the (translateable) error message using \a error().
 * @return true on success
 */
bool Reader::load(const QString &filename)
{
	QFile orafile(filename);
	KZip zip(&orafile);

	if(!zip.open(QIODevice::ReadOnly)) {
		_error = orafile.errorString();
		return false;
	}

	// Make sure this is an OpenRaster file
	if(!checkIsOraFile(zip)) {
		_error = tr("File is not an OpenRaster file");
		return false;
	}

	// Read the stack
	QDomDocument doc;
	if(doc.setContent(utils::getArchiveFile(zip, "stack.xml"), true, &_error) == false)
		return false;

	const QDomElement stackroot = doc.documentElement().firstChildElement("stack");

	// Get the size of the image
	// These attributes are required by ORA standard.
	QSize imagesize(
			doc.documentElement().attribute("w").toInt(),
			doc.documentElement().attribute("h").toInt()
			);

	if(imagesize.isEmpty()) {
		_error = tr("Image has zero size!");
		return false;
	}

	// Initialize the layer stack now that we know the size
	_commands.append(MessagePtr(new protocol::CanvasResize(1, 0, imagesize.width(), imagesize.height(), 0)));

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

bool Reader::loadLayers(KArchive &zip, const QDomElement& stack, QPoint offset)
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
				QByteArray image = utils::getArchiveFile(zip, src);
				if(image.isNull() || !content.loadFromData(image, "png")) {
					_error = tr("Couldn't load layer %1").arg(src);
					return false;
				}
			}

			// Create layer
			QString name = e.attribute("name", tr("Unnamed layer"));
			_commands.append(MessagePtr(new protocol::LayerCreate(
				1,
				++_layerid,
				0,
				name
			)));

			QPoint layerPos = offset + QPoint(
				e.attribute("x", "0").toInt(),
				e.attribute("y", "0").toInt()
				);
			_commands.append(net::putQImage(1, _layerid, layerPos.x(), layerPos.y(), content, false));

			QString compositeOp = e.attribute("composite-op", "src-over");
			int blendmode = paintcore::blendModeSvg(compositeOp);
			if(blendmode<0) {
				_warnings |= ORA_EXTENDED;
				blendmode = 1;
			}

			_commands.append(MessagePtr(new protocol::LayerAttributes(
				1,
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
				1,
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
