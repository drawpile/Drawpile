/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2009-2017 Calle Laakkonen

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

#include "core/blendmodes.h"
#include "core/annotationmodel.h"
#include "ora/orareader.h"
#include "ora/orawriter.h"

#include "../shared/net/layer.h"
#include "../shared/net/annotation.h"
#include "../shared/net/meta2.h"
#include "net/commands.h"
#include "utils/archive.h"
#include "utils/images.h"

#include <QGuiApplication>
#include <QImage>
#include <QXmlStreamReader>
#include <QDebug>
#include <QColor>
#include <QFile>

#include <KZip>

using protocol::MessagePtr;

namespace openraster {

namespace {
	struct Annotation {
		QRect rect;
		QColor bg;
		QString valign;
		QString content;
	};

	struct Layer {
		QString name;
		QString src;
		QPoint offset;
		qreal opacity;
		bool visibility;
		bool locked;
		QString compositeOp;
	};

	struct Canvas {
		QString error;

		QSize size;
		QList<Layer> layers; // note: layer order in an ORA stack is topmost first
		QList<Annotation> annotations;

		bool nestedWarning;
		bool extensionsWarning;

		Canvas() : nestedWarning(false), extensionsWarning(false) { }
	};
}

static bool checkIsOraFile(KArchive &zip)
{
	const QByteArray expected = "image/openraster";
	QByteArray mimetype = utils::getArchiveFile(zip, "mimetype", expected.length());

	return mimetype == expected;
}

static void skipElement(QXmlStreamReader &reader)
{
	int stack=1;
	while(!reader.atEnd() && stack>0) {
		const auto tokentype = reader.readNext();
		switch(tokentype) {
			case QXmlStreamReader::StartElement: ++stack; break;
			case QXmlStreamReader::EndElement: --stack; break;
			default: break;
		}
	}
}

/**
 * Check if the given list of attributes anything that isn't in the list of known keys
 * @param attrs attributes to check
 * @param names list of known names
 * @return true if unknown attributes are present
 */
static bool hasUnknownAttributes(const char *element, const QXmlStreamAttributes &attrs, const char **names) {
	for(const QXmlStreamAttribute &attr : attrs) {
		const char **name = names;
		bool found = false;
		while(*name) {
			if(attr.name() == *name) {
				found = true;
				break;
			}
			++name;
		}
		if(!found) {
			qWarning() << "Unknown" << element << "attribute:" << attr.name();
			return true;
		}
	}
	return false;
}

static QString attrToString(const QStringRef &attr, const QString &def)
{
	return attr.isEmpty() ? def : attr.toString();
}

static qreal attrToReal(const QStringRef &attr, double def)
{
	bool ok;
	const qreal v = attr.toDouble(&ok);
	return ok ? v : def;
}

static bool attrToBool(const QStringRef &attr, bool def, const char *trueVal)
{
	if(attr.isEmpty())
		return def;
	return attr == trueVal;
}

static QColor attrToColor(const QStringRef &attr, const QColor &def)
{
	if(attr.length() == 7 && attr.at(0) == '#')
		return QColor::fromRgb(attr.mid(1).toUInt(0, 16));
	else if(attr.length() == 9 && attr.at(0) == '#')
		return QColor::fromRgba(attr.mid(1).toUInt(0, 16));
	else
		return def;
}

//! Read a layer element
static bool readStackLayer(QXmlStreamReader &reader, Canvas &canvas, const QPoint &parentOffset)
{
	// Grab <layer> element attributes first
	const QXmlStreamAttributes attrs = reader.attributes();

	static const char *knownLayerAttributes[] = {"x", "y", "name", "src", "opacity", "visibility", "composite-op", "selected", "edit-locked", nullptr };

	if(hasUnknownAttributes("layer", attrs, knownLayerAttributes))
		canvas.extensionsWarning = true;

	Layer layer {
		attrToString(attrs.value("name"), QString()),
		attrToString(attrs.value("src"), QString()),
		parentOffset + QPoint(attrs.value("x").toInt(), attrs.value("y").toInt()),
		qBound(0.0, attrToReal(attrs.value("opacity"), 1.0), 1.0),
		attrToBool(attrs.value("visibility"), true, "visible"),
		attrToBool(attrs.value("edit-locked"), false, "true"),
		attrToString(attrs.value("composite-op"), QStringLiteral("src-over"))
	};

	canvas.layers << layer;

	// <layer> has no subelements
	while(!reader.atEnd()) {
		const auto tokentype = reader.readNext();
		switch(tokentype) {
			case QXmlStreamReader::Invalid:
				canvas.error = reader.errorString();
				return false;
			case QXmlStreamReader::StartElement:
				canvas.extensionsWarning = true;
				qWarning() << "Encountered unexpected <layer> subelement:" << reader.name();
				skipElement(reader);
				break;
			case QXmlStreamReader::EndElement:
				return true;
			default: break;
		}
	}
	
	canvas.error = "File ended unexpectedly";
	return false;
}

//! Read a drawpile specific annotation element
static bool readStackAnnotation(QXmlStreamReader &reader, Canvas &canvas)
{
	static const char *knownAttributes[] = {"x", "y", "w", "h", "valign", "bg", nullptr};
	const QXmlStreamAttributes attrs = reader.attributes();
	if(hasUnknownAttributes("annotation", attrs, knownAttributes))
		canvas.extensionsWarning = true;

	Annotation ann {
		QRect(
			attrs.value("x").toInt(),
			attrs.value("y").toInt(),
			attrs.value("w").toInt(),
			attrs.value("h").toInt()
			),
		attrToColor(attrs.value("bg"), Qt::transparent),
		attrToString(attrs.value("valign"), QString()),
		QString()
	};

	// Process <annotation> subelements
	while(!reader.atEnd()) {
		const auto tokentype = reader.readNext();
		switch(tokentype) {
			case QXmlStreamReader::Invalid:
				canvas.error = reader.errorString();
				return false;
			case QXmlStreamReader::Characters:
				ann.content += reader.text();
				break;
			case QXmlStreamReader::StartElement:
				canvas.extensionsWarning = true;
				qWarning() << "Encountered unknown <a> subelement:" << reader.name();
				skipElement(reader);
				break;
			case QXmlStreamReader::EndElement:
				canvas.annotations << ann;
				return true;
			default: break;
		}
	}

	canvas.error = "File ended unexpectedly";
	return false;
}

//! Read the drawpile specific <annotations> container element
static bool readStackAnnotations(QXmlStreamReader &reader, Canvas &canvas)
{
	static const char *knownAttributes[] = {nullptr};
	if(hasUnknownAttributes("annotations", reader.attributes(), knownAttributes))
		canvas.extensionsWarning = true;

	// Process <annotations> subelements
	while(!reader.atEnd()) {
		const auto tokentype = reader.readNext();
		switch(tokentype) {
			case QXmlStreamReader::Invalid:
				canvas.error = reader.errorString();
				return false;
			case QXmlStreamReader::StartElement:
				if(reader.name() == "a" && reader.namespaceUri() == DP_NAMESPACE) {
					if(!readStackAnnotation(reader, canvas))
						return false;

				} else {
					canvas.extensionsWarning = true;
					qWarning() << "Encountered unknown <annotations> subelement:" << reader.name();
					skipElement(reader);
				}
				break;
			case QXmlStreamReader::EndElement:
				return true;
			default: break;
		}
	}

	canvas.error = "File ended unexpectedly";
	return false;
}


//! Read a <stack> element (contain sub-stacks and layers)
static bool readStackStack(QXmlStreamReader &reader, Canvas &canvas, const QPoint &parentOffset)
{
	// Grab <stack> element attributes first
	const QXmlStreamAttributes attrs = reader.attributes();
	const QPoint offset = parentOffset + QPoint(
			attrs.value("x").toInt(),
			attrs.value("y").toInt()
			);

	static const char *knownStackAttributes[] = {"x", "y", nullptr};

	if(hasUnknownAttributes("stack", attrs, knownStackAttributes))
		canvas.extensionsWarning = true;

	// Process <stack> subelements
	while(!reader.atEnd()) {
		const auto tokentype = reader.readNext();
		switch(tokentype) {
			case QXmlStreamReader::Invalid:
				canvas.error = reader.errorString();
				return false;
			case QXmlStreamReader::StartElement:
				if(reader.name() == "stack") {
					canvas.nestedWarning = true;
					if(!readStackStack(reader, canvas, offset))
						return false;

				} else if(reader.name() == "annotations" && reader.namespaceUri() == DP_NAMESPACE) {
					if(!readStackAnnotations(reader, canvas))
						return false;

				} else if(reader.name() == "layer") {
					if(!readStackLayer(reader, canvas, QPoint()))
						return false;

				} else {
					canvas.extensionsWarning = true;
					qWarning() << "Encountered unknown <stack> subelement:" << reader.name();
					skipElement(reader);
				}
				break;
			case QXmlStreamReader::EndElement:
				return true;
			default: break;
		}
	}

	canvas.error = "File ended unexpectedly";
	return false;
}

//! Read the stack.xml root <image> element
static bool readStackImage(QXmlStreamReader &reader, Canvas &canvas)
{
	// Grab the <image> element attributes first
	const QXmlStreamAttributes attrs = reader.attributes();
	canvas.size = QSize(
			attrs.value("w").toInt(),
			attrs.value("h").toInt()
			);

	static const char *knownImageAttributes[] = {"w", "h", "version", nullptr};

	if(hasUnknownAttributes("image", attrs, knownImageAttributes))
		canvas.extensionsWarning = true;

	// Process <image> subelements
	while(!reader.atEnd()) {
		const auto tokentype = reader.readNext();
		switch(tokentype) {
			case QXmlStreamReader::Invalid:
				canvas.error = reader.errorString();
				return false;
			case QXmlStreamReader::StartElement:
				if(reader.name() == "stack") {
					if(!readStackStack(reader, canvas, QPoint()))
						return false;
				} else {
					canvas.extensionsWarning = true;
					qWarning() << "Encountered unknown <stack> subelement:" << reader.name();
					skipElement(reader);
				}
				break;
			case QXmlStreamReader::EndElement:
				return true;
			default: break;

		}
	}

	canvas.error = "File ended unexpectedly";
	return false;
}

/**
 * Generate the initialization commands from the layer stack and layer content images.
 */
static OraResult makeInitCommands(KZip &zip, const Canvas &canvas)
{
	if(canvas.size.isEmpty())
		return QGuiApplication::tr("Image has zero size!");

	if(!utils::checkImageSize(canvas.size))
		return QGuiApplication::tr("Image is too big!");

	if(canvas.layers.isEmpty())
		return QGuiApplication::tr("No layers found!");

	OraResult result;
	if(canvas.extensionsWarning)
		result.warnings |= OraResult::ORA_EXTENDED;
	if(canvas.nestedWarning)
		result.warnings |= OraResult::ORA_NESTED;

	const int ctxId = 1;

	// Set canvas size
	result.commands << MessagePtr(new protocol::CanvasResize(ctxId, 0, canvas.size.width(), canvas.size.height(), 0));

	// Create layers
	// Note: layers are stored topmost first in ORA, but we create them bottom-most first
	int layerId = ctxId << 8;
	for(int i=canvas.layers.size()-1;i>=0;--i) {
		const Layer &layer = canvas.layers[i];
		QImage content;
		{
			QByteArray image = utils::getArchiveFile(zip, layer.src);
			if(image.isNull() || !content.loadFromData(image)) {
				return QGuiApplication::tr("Couldn't load layer %1").arg(layer.src);
			}
		}

		const QColor solidColor = utils::isSolidColorImage(content);

		result.commands.append(MessagePtr(new protocol::LayerCreate(
				ctxId,
				++layerId,
				0,
				solidColor.isValid() ? solidColor.rgba() : 0,
				0,
				layer.name
			)));

		if(!solidColor.isValid())
			result.commands << net::command::putQImage(
					ctxId,
					layerId,
					layer.offset.x(),
					layer.offset.y(),
					content,
					paintcore::BlendMode::MODE_REPLACE
					);

		bool exact_blendop;
		int blendmode = paintcore::findBlendModeByName(layer.compositeOp, &exact_blendop).id;
		if(!exact_blendop)
			result.warnings |= OraResult::ORA_EXTENDED;

		result.commands << MessagePtr(new protocol::LayerAttributes(
			ctxId,
			layerId,
			qRound(255 * layer.opacity),
			blendmode
			));

		if(layer.locked) {
			result.commands << MessagePtr(new protocol::LayerACL(ctxId, layerId, true, QList<uint8_t>()));
		}

		if(!layer.visibility) {
			result.commands << MessagePtr(new protocol::LayerVisibility(ctxId, layerId, false));
		}
	}

	// Create annotations
	int annotationId = ctxId << 8;
	for(const Annotation &ann : canvas.annotations) {
		result.commands.append(MessagePtr(new protocol::AnnotationCreate(
			ctxId,
			++annotationId,
			ann.rect.x(),
			ann.rect.y(),
			ann.rect.width(),
			ann.rect.height()
		)));
		result.commands.append(MessagePtr(new protocol::AnnotationEdit(
			ctxId,
			annotationId,
			ann.bg.rgba(),
			paintcore::Annotation::valignFromString(ann.valign),
			0,
			ann.content
		)));
	}

	return result;
}

OraResult loadOpenRaster(const QString &filename)
{
	Canvas canvas;
	QFile orafile(filename);
	KZip zip(&orafile);

	if(!zip.open(QIODevice::ReadOnly)) {
		return orafile.errorString();
	}

	// Make sure this is an OpenRaster file
	if(!checkIsOraFile(zip)) {
		return QGuiApplication::tr("File is not an OpenRaster file");
	}

	// Read the layer stack definition
	QXmlStreamReader reader(zip.directory()->file("stack.xml")->data());

	while(!reader.atEnd()) {
		const auto tokentype = reader.readNext();
		switch(tokentype) {
		case QXmlStreamReader::Invalid:
			return reader.errorString();
		case QXmlStreamReader::StartElement:
			if(reader.name() == "image") {
				if(!readStackImage(reader, canvas))
					return canvas.error;
			} else {
				return QStringLiteral("Unexpected root element: ") + reader.name().toString();
			}
		default: break;
		}
	}

	// Generate the commands to initialize session history
	return makeInitCommands(zip, canvas);
}

}

