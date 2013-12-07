/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2008-2013 Calle Laakkonen

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

#include <QGraphicsObject>
#include <QTextDocument>
#include <QImage>

namespace drawingboard {

/**
 * @brief A text box that can be overlaid on the picture.
 *
 * This class inherits from QGraphicsObject so we can point to
 * its instances with QPointer
 */
class AnnotationItem : public QGraphicsObject {
	Q_OBJECT
	public:
		static const int HANDLE = 10;
		enum { Type = UserType + 10 };
		enum Handle {OUTSIDE, TRANSLATE, RS_TOPLEFT, RS_TOPRIGHT, RS_BOTTOMRIGHT, RS_BOTTOMLEFT, RS_TOP, RS_RIGHT, RS_BOTTOM, RS_LEFT};

		AnnotationItem(int id, QGraphicsItem *parent=0);

		//! Get the ID number of this annotation
		int id() const { return id_; }

		//! Get the translation handle at the point
		Handle handleAt(const QPoint &point) const;

		//! Adjust annotation position or size
		void adjustGeometry(Handle handle, const QPoint &delta);

		//! Highlight this item
		void setHighlight(bool h);

		//! Enable border
		void setShowBorder(bool show);

		//! Set the position and size of the annotation
		void setGeometry(const QRect &rect);

		//! Get the position and size
		QRect geometry() const;

		//! Set the annotation text
		void setText(const QString &text);

		//! Get the annotation text
		QString text() const { return _text.toHtml(); }

		//! Set the background color
		void setBackgroundColor(const QColor &color);

		//! Get the color of the background
		const QColor& backgroundColor() const { return bgcol_; }

		//! reimplementation
		QRectF boundingRect() const;

		//! reimplementation
		int type() const { return Type; }

		//! Render this annotation onto a QImage
		QImage toImage();

	protected:
		void paint(QPainter *painter, const QStyleOptionGraphicsItem *options, QWidget *);

	private:
		void render(QPainter *painter, const QRectF& rect);

		int id_;
		QRect _rect;

		QTextDocument _text;
		QColor bgcol_;

		bool _highlight, _showborder;
};

}

#endif

