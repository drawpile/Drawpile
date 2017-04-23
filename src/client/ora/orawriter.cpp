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

#include "ora/orawriter.h"
#include "core/layerstack.h"
#include "core/layer.h"
#include "core/blendmodes.h"

#include <QXmlStreamWriter>
#include <QBuffer>
#include <QDebug>
#include <KZip>

namespace openraster {

const QString DP_NAMESPACE = QStringLiteral("http://drawpile.net/");

static bool putPngInZip(KZip &zip, const QString &filename, const QImage &image, QString *errorMessage)
{
	QBuffer buf;
	image.save(&buf, "PNG");

	// PNG is already compressed, so no use attempting to recompress
	zip.setCompression(KZip::NoCompression);
	if(!zip.writeFile(filename, buf.data())) {
		if(errorMessage)
			*errorMessage = zip.errorString();
		return false;
	}
	return true;
}

static void writeStackStack(QXmlStreamWriter &writer, const paintcore::LayerStack *image)
{
	writer.writeStartElement("stack");

	// Add annotations
	// This will probably be replaced with proper text element support
	// once standardized.
	if(!image->annotations()->isEmpty()) {
		writer.writeStartElement(DP_NAMESPACE, "annotations");

		for(const paintcore::Annotation &a : image->annotations()->getAnnotations()) {
			writer.writeStartElement(DP_NAMESPACE, "a");

			QRect ag = a.rect;
			writer.writeAttribute("x", QString::number(ag.x()));
			writer.writeAttribute("y", QString::number(ag.y()));
			writer.writeAttribute("w", QString::number(ag.width()));
			writer.writeAttribute("h", QString::number(ag.height()));
			writer.writeAttribute("bg", QString("#%1").arg(uint(a.background.rgba()), 8, 16, QChar('0')));
			writer.writeAttribute("valign", a.valignToString());
			writer.writeCDATA(a.text);
			writer.writeEndElement();
		}

		writer.writeEndElement();
	}

	// Add layers (topmost layer goes first in ORA)
	for(int i=image->layerCount()-1;i>=0;--i) {
		const paintcore::Layer *l = image->getLayerByIndex(i);

		writer.writeStartElement("layer");
		writer.writeAttribute("src", QString("data/layer%1.png").arg(i));
		writer.writeAttribute("name", l->title());
		writer.writeAttribute("opacity", QString::number(l->opacity() / 255.0, 'f', 3));
		if(l->isHidden())
			writer.writeAttribute("visibility", "hidden");
		if(l->blendmode() != 1)
			writer.writeAttribute("composite-op", "svg:" + paintcore::findBlendMode(l->blendmode()).svgname);

		writer.writeEndElement();
	}

	writer.writeEndElement();
}

static bool writeStackXml(KZip &zip, const paintcore::LayerStack *image, QString *errorMessage)
{
	QBuffer buffer;
	buffer.open(QBuffer::ReadWrite);

	QXmlStreamWriter writer(&buffer);
	writer.setAutoFormatting(true);

	writer.writeStartDocument();

	// Write root element
	writer.writeStartElement("image");
	writer.writeNamespace("http://drawpile.net", "drawpile");

	writer.writeAttribute("w", QString::number(image->width()));
	writer.writeAttribute("h", QString::number(image->height()));
	writer.writeAttribute("version", "0.0.3");

	// Write the main layer stack
	writeStackStack(writer, image);

	// Done.
	writer.writeEndDocument();
	zip.setCompression(KZip::DeflateCompression);
	if(!zip.writeFile("stack.xml", buffer.data())) {
		if(errorMessage)
			*errorMessage = zip.errorString();
		return false;
	}
	return true;
}

static bool writeLayer(KZip &zf, const paintcore::LayerStack *layers, int index, QString *errorMessage)
{
	const paintcore::Layer *l = layers->getLayerByIndex(index);
	Q_ASSERT(l);

	return putPngInZip(zf, QString("data/layer%1.png").arg(index), l->toImage(), errorMessage);
}

static bool writePreviewImages(KZip &zf, const paintcore::LayerStack *layers, QString *errorMessage)
{
	QImage img = layers->toFlatImage(false);

	// Flattened full size version for image viewers
	if(!putPngInZip(zf, "mergedimage.png", img, errorMessage))
		return false;

	// Thumbnail for browsers and such
	if(img.width() > 256 || img.height() > 256)
		img = img.scaled(QSize(256, 256), Qt::KeepAspectRatio, Qt::SmoothTransformation);

	return putPngInZip(zf, "Thumbnails/thumbnail.png", img, errorMessage);
}

bool saveOpenRaster(const QString& filename, const paintcore::LayerStack *image, QString *errorMessage)
{
	KZip zf(filename);
	if(!zf.open(QIODevice::WriteOnly)) {
		if(errorMessage)
			*errorMessage = zf.errorString();
		return false;
	}

	// The first entry of an OpenRaster file must be a
	// (uncompressed) file named "mimetype".
	zf.setCompression(KZip::NoCompression);
	if(!zf.writeFile("mimetype", QByteArray("image/openraster"))) {
		if(errorMessage)
			*errorMessage = zf.errorString();
		return false;
	}

	// The stack XML contains the image structure
	// definition.
	if(!writeStackXml(zf, image, errorMessage))
		return false;

	// Each layer is written as an individual PNG image
	for(int i=image->layerCount()-1;i>=0;--i) {
		if(!writeLayer(zf, image, i, errorMessage))
			return false;
	}

	// Ready to use images for viewers
	writePreviewImages(zf, image, errorMessage);

	if(!zf.close()) {
		if(errorMessage)
			*errorMessage = zf.errorString();
		return false;
	}
	return true;
}

}
