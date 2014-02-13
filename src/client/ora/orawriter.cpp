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
#include <QDomDocument>
#include <QBuffer>
#include <QDebug>

#include "scene/annotationitem.h"

#include "ora/zipwriter.h"
#include "ora/orawriter.h"
#include "core/layerstack.h"
#include "core/layer.h"
#include "core/rasterop.h" // for blending modes

namespace {

bool putTextInZip(ZipWriter &zip, const QString& filename, const QString& text)
{
	QBuffer *buf = new QBuffer();
	buf->setData(text.toUtf8());
	zip.setCompressionPolicy(ZipWriter::NeverCompress);
	zip.addFile(filename, buf);
	return true;
}

bool writeStackXml(ZipWriter &zf, const paintcore::LayerStack *layers, const QList<drawingboard::AnnotationItem*> annotations)
{
	QDomDocument doc;
	QDomElement root = doc.createElement("image");
	doc.appendChild(root);
	// Width and height are required attributes
	root.setAttribute("w", layers->width());
	root.setAttribute("h", layers->height());

	QDomElement stack = doc.createElement("stack");
	root.appendChild(stack);

	// Add annotations
	// This will probably be replaced with proper text element support
	// once standardized.
	if(annotations.isEmpty()==false) {
		QDomElement annotationEls = doc.createElementNS("http://drawpile.sourceforge.net/", "annotations");
		annotationEls.setPrefix("drawpile");
		foreach(const drawingboard::AnnotationItem *a, annotations) {
			QDomElement an = doc.createElementNS("http://drawpile.sourceforge.net/","a");
			an.setPrefix("drawpile");
			QRect ag = a->geometry();
			an.setAttribute("x", ag.x());
			an.setAttribute("y", a->y());
			an.setAttribute("w", ag.width());
			an.setAttribute("h", ag.height());
			an.setAttribute("bg", QString("#%1").arg(uint(a->backgroundColor().rgba()), 8, 16, QChar('0')));
			an.appendChild(doc.createCDATASection(a->text()));
			annotationEls.appendChild(an);
		}
		stack.appendChild(annotationEls);
	}

	// Add layers (topmost layer goes first in ORA)
	for(int i=layers->layers()-1;i>=0;--i) {
		const paintcore::Layer *l = layers->getLayerByIndex(i);

		QDomElement layer = doc.createElement("layer");
		layer.setAttribute("src", QString("data/layer%1.png").arg(i));
		layer.setAttribute("name", l->title());
		layer.setAttribute("opacity", QString::number(l->opacity() / 255.0, 'f', 3));
		if(l->hidden())
			layer.setAttribute("visibility", "hidden");
		if(l->blendmode() != 1)
			layer.setAttribute("composite-op", "svg:" + paintcore::svgBlendMode(l->blendmode()));

		stack.appendChild(layer);
	}

	QBuffer *buf = new QBuffer();
	buf->setData(doc.toByteArray());
	zf.setCompressionPolicy(ZipWriter::AlwaysCompress);
	zf.addFile("stack.xml", buf);
	return true;
}

bool writeLayer(ZipWriter &zf, const paintcore::LayerStack *layers, int index)
{
	const paintcore::Layer *l = layers->getLayerByIndex(index);
	QBuffer image;
	image.open(QIODevice::ReadWrite);
	// TODO autocrop layer to play nice with programs like mypaint?
	l->toImage().save(&image, "PNG");
	// Save the image without compression, as trying to squeeze a few more bytes out of a PNG is pointless.
	zf.setCompressionPolicy(ZipWriter::NeverCompress);
	zf.addFile(QString("data/layer%1.png").arg(index), image.data());
	return true;
}

bool writeThumbnail(ZipWriter &zf, const paintcore::LayerStack *layers)
{
	QImage img = layers->toFlatImage();
	if(img.width() > 256 || img.height() > 256)
		img = img.scaled(QSize(256, 256), Qt::KeepAspectRatio, Qt::SmoothTransformation);

	QBuffer thumb;
	thumb.open(QIODevice::ReadWrite);
	img.save(&thumb, "PNG");
	zf.setCompressionPolicy(ZipWriter::NeverCompress);
	zf.addFile("Thumbnails/thumbnail.png", thumb.data());
	return true;
}

}

namespace openraster {

bool saveOpenRaster(const QString& filename, const paintcore::LayerStack *layers, const QList<drawingboard::AnnotationItem*> &annotations)
{
	QFile orafile(filename);
	if(!orafile.open(QIODevice::WriteOnly))
		return false;

	ZipWriter zf(&orafile);

	// The first entry of an OpenRaster file is a
	// file named "mimetype" containing the text
	// "image/openraster" (must be uncompressed)
	putTextInZip(zf, "mimetype", "image/openraster");

	// The stack XML contains the image structure
	// definition.
	writeStackXml(zf, layers, annotations);

	// Each layer is written as an individual PNG image
	for(int i=layers->layers()-1;i>=0;--i)
		writeLayer(zf, layers, i);

	// A ready to use thumbnail for file managers etc.
	writeThumbnail(zf, layers);

	zf.close();
	return true;
}

}
