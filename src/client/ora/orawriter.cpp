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
#include <QDomDocument>
#include <QBuffer>
#include <QDebug>

#include "zipfile.h"
#include "orawriter.h"
#include "../core/layerstack.h"
#include "../core/layer.h"

namespace openraster {

Writer::Writer(const dpcore::LayerStack *layers)
	: layers_(layers)
{
}

bool putTextInZip(Zipfile &zip, const QString& filename, const QString& text)
{
	QBuffer *buf = new QBuffer();
   	buf->setData(text.toUtf8());
	return zip.addFile(filename, buf, Zipfile::STORE);
}

bool Writer::save(const QString& filename) const
{
	Zipfile zf(filename, Zipfile::OVERWRITE);

	if(zf.open()==false)
		return false;

	// The first entry of an OpenRaster file is a
	// file named "mimetype" containing the text
	// "image/openraster" (must be uncompressed)
	putTextInZip(zf, "mimetype", "image/openraster");

	writeStackXml(zf);

	for(int i=layers_->layers()-1;i>=0;--i)
		writeLayer(zf, i);

	writeThumbnail(zf);

	return zf.close();
}

void Writer::setAnnotations(const QStringList& annotations)
{
	annotations_ = annotations;
}

bool Writer::writeStackXml(Zipfile &zf) const
{
	QDomDocument doc;
	QDomElement root = doc.createElement("image");
	doc.appendChild(root);
	root.setAttribute("w", layers_->width());
	root.setAttribute("h", layers_->height());

	QDomElement stack = doc.createElement("stack");
	root.appendChild(stack);

	// Add annotations
	// This will be replaced with proper text element support
	// once standardized.
	if(annotations_.isEmpty()==false) {
		QDomElement annotations = doc.createElementNS("http://drawpile.sourceforge.net/", "annotations");
		annotations.setPrefix("drawpile");
		foreach(QString a, annotations_) {
			QDomElement an = doc.createElementNS("http://drawpile.sourceforge.net/","a");
			an.setPrefix("drawpile");
			an.appendChild(doc.createTextNode(a));
			annotations.appendChild(an);
		}
		stack.appendChild(annotations);
	}

	// Add layers
	for(int i=layers_->layers()-1;i>=0;--i) {
		const dpcore::Layer *l = layers_->getLayerByIndex(i);
		QDomElement layer = doc.createElement("layer");
		layer.setAttribute("src", QString("data/layer") + QString::number(i) + ".png");
		layer.setAttribute("name", l->name());
		layer.setAttribute("opacity", QString::number(l->opacity() / 255.0, 'f', 3));

		stack.appendChild(layer);
	}

	QBuffer *buf = new QBuffer();
	buf->setData(doc.toByteArray());
	qDebug() << "STACK: " << doc.toByteArray();
	return zf.addFile("stack.xml", buf);
}

bool Writer::writeLayer(Zipfile &zf, int index) const
{
	const dpcore::Layer *l = layers_->getLayerByIndex(index);
	QBuffer *image = new QBuffer();
	image->open(QIODevice::ReadWrite);
	// TODO autocrop layer to play nice with programs like mypaint?
	l->toImage().save(image, "PNG");
	// Save the image without compression, as trying to squeeze a few more bytes out of a PNG is pointless.
	return zf.addFile(QString("data/layer") + QString::number(index) + ".png", image, Zipfile::STORE);
}

bool Writer::writeThumbnail(Zipfile &zf) const
{
	QImage img = layers_->toFlatImage();
	if(img.width() > 256 || img.height() > 256)
		img = img.scaled(QSize(256, 256), Qt::KeepAspectRatio, Qt::SmoothTransformation);
	QBuffer *thumb = new QBuffer();
	thumb->open(QIODevice::ReadWrite);
	img.save(thumb, "PNG");
	return zf.addFile(QString("Thumbnails/thumbnail.png"), thumb, Zipfile::STORE);
}

}
