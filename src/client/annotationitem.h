/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2008 Calle Laakkonen

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
#ifndef ANNOTATIONITEM_H
#define ANNOTATIONITEM_H

#include <QGraphicsItem>

namespace protocol {
	class Annotation;
}

namespace dpcore {
	class Layer;
}

namespace drawingboard {

//! A text box that can be overlaid on the picture.
class AnnotationItem : public QGraphicsItem {
	public:
		static const int HANDLE = 10;
		enum { Type = UserType + 10 };
		enum Handle {TRANSLATE, RS_TOPLEFT, RS_BOTTOMRIGHT};

		AnnotationItem(int id, QGraphicsItem *parent=0);

		//! Get the ID number of this annotation
		int id() const { return id_; }

		//! Get the translation handle at the point
		Handle handleAt(QPointF point);

		//! Grow the box from the top-left corner
		void growTopLeft(qreal x, qreal y);

		//! Grow the box from the bottom-right corner
		void growBottomRight(qreal x, qreal y);

		//! Highlight this item
		void setHighlight(bool h);

		//! Set options from an annotation message
		void setOptions(const protocol::Annotation& a);

		//! Get options for an annotation message
		void getOptions(protocol::Annotation& a);

		//! Get the size of the box
		const QSizeF& size() const { return size_; }

		//! Get the annotation text
		const QString& text() const { return text_; }

		//! Get the color of the text
		const QColor& textColor() const { return textcol_; }

		//! Get the color of the background
		const QColor& backgroundColor() const { return bgcol_; }

		//! Get the text justification
		int justify() const;

		//! Is the text bold
		bool bold() const { return font_.bold(); }

		//! Is the text italic
		bool italic() const { return font_.italic(); }

		//! Get the font family name
		QString font() const { return font_.family(); }

		//! Get the font size in pixels
		int fontSize() const { return font_.pixelSize(); }

		//! reimplementation
		QRectF boundingRect() const;

		//! reimplementation
		int type() const { return Type; }

		//! Render this annotation onto a layer
		dpcore::Layer *toLayer(int *x, int *y);

	protected:
		void paint(QPainter *painter, const QStyleOptionGraphicsItem *options, QWidget *);

	private:
		void render(QPainter *painter, const QRectF& rect) const;

		int id_;
		QSizeF size_;
		QString text_;
		QColor textcol_, bgcol_;
		QFont font_;
		int flags_;
		bool highlight_;
};

}

#endif

