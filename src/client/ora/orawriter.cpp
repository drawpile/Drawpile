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

#include "ora/orawriter.h"
#include "core/annotation.h"
#include "core/layerstack.h"
#include "core/layer.h"
#include "core/blendmodes.h"

#include <QDomDocument>
#include <QBuffer>
#include <QDebug>
#include <KZip>

namespace {

bool putPngInZip(KZip &zip, const QString &filename, const QImage &image)
{
	QBuffer buf;
	image.save(&buf, "PNG");

	// PNG is already compressed, so no use attempting to recompress
	zip.setCompression(KZip::NoCompression);
	return zip.writeFile(filename, buf.data());
}

bool writeStackXml(KZip &zip, const paintcore::LayerStack *image)
{
	QDomDocument doc;
	QDomElement root = doc.createElement("image");

	// Note: Qt's createElementNS and friends are buggy and repeat
	// the namespace declaration at every element. Instead,
	// we declare the namespace and use prefixes manually.
	root.setAttribute("xmlns:drawpile", "http://drawpile.net/");

	doc.appendChild(root);

	// Width and height are required attributes
	root.setAttribute("w", image->width());
	root.setAttribute("h", image->height());
	root.setAttribute("version", "0.0.3");

	QDomElement stack = doc.createElement("stack");
	root.appendChild(stack);

	// Add annotations
	// This will probably be replaced with proper text element support
	// once standardized.
	if(image->hasAnnotations()) {
		QDomElement annotationEls = doc.createElement("drawpile:annotations");
		annotationEls.setPrefix("drawpile");
		foreach(const paintcore::Annotation *a, image->annotations()) {
			QDomElement an = doc.createElement("drawpile:a");
			an.setPrefix("drawpile");

			QRect ag = a->rect();
			an.setAttribute("x", ag.x());
			an.setAttribute("y", ag.y());
			an.setAttribute("w", ag.width());
			an.setAttribute("h", ag.height());
			an.setAttribute("bg", QString("#%1").arg(uint(a->backgroundColor().rgba()), 8, 16, QChar('0')));
			an.appendChild(doc.createCDATASection(a->text()));
			annotationEls.appendChild(an);
		}
		stack.appendChild(annotationEls);
	}

	// Add layers (topmost layer goes first in ORA)
	for(int i=image->layers()-1;i>=0;--i) {
		const paintcore::Layer *l = image->getLayerByIndex(i);

		QDomElement layer = doc.createElement("layer");
		layer.setAttribute("src", QString("data/layer%1.png").arg(i));
		layer.setAttribute("name", l->title());
		layer.setAttribute("opacity", QString::number(l->opacity() / 255.0, 'f', 3));
		if(l->hidden())
			layer.setAttribute("visibility", "hidden");
		if(l->blendmode() != 1)
			layer.setAttribute("composite-op", "svg:" + paintcore::findBlendMode(l->blendmode()).svgname);

		// TODO lock and selection
		stack.appendChild(layer);
	}

	zip.setCompression(KZip::DeflateCompression);
	return zip.writeFile("stack.xml", doc.toByteArray());
}

bool writeLayer(KZip &zf, const paintcore::LayerStack *layers, int index)
{
	const paintcore::Layer *l = layers->getLayerByIndex(index);
	Q_ASSERT(l);

	return putPngInZip(zf, QString("data/layer%1.png").arg(index), l->toImage());
}

bool writePreviewImages(KZip &zf, const paintcore::LayerStack *layers)
{
	QImage img = layers->toFlatImage(false);

	// Flattened full size version for image viewers
	if(!putPngInZip(zf, "mergedimage.png", img))
		return false;

	// Thumbnail for browsers and such
	if(img.width() > 256 || img.height() > 256)
		img = img.scaled(QSize(256, 256), Qt::KeepAspectRatio, Qt::SmoothTransformation);

	return putPngInZip(zf, "Thumbnails/thumbnail.png", img);
}

}

namespace openraster {

bool saveOpenRaster(const QString& filename, const paintcore::LayerStack *image)
{
	KZip zf(filename);
	if(!zf.open(QIODevice::WriteOnly))
		return false;

	// The first entry of an OpenRaster file must be a
	// (uncompressed) file named "mimetype".
	zf.setCompression(KZip::NoCompression);
	if(!zf.writeFile("mimetype", QByteArray("image/openraster")))
		return false;

	// The stack XML contains the image structure
	// definition.
	writeStackXml(zf, image);

	// Each layer is written as an individual PNG image
	for(int i=image->layers()-1;i>=0;--i)
		writeLayer(zf, image, i);

	// Ready to use images for viewers
	writePreviewImages(zf, image);

	return zf.close();
}

}
