/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2008-2014 Calle Laakkonen

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
#ifndef ANNOTATIONITEM_H
#define ANNOTATIONITEM_H

#include <QGraphicsObject>

namespace paintcore {
	class Annotation;
	class LayerStack;
}

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

		AnnotationItem(int id, paintcore::LayerStack *image, QGraphicsItem *parent=0);

		//! Get the ID number of this annotation
		int id() const { return _id; }

		//! Get the annotation model instance
		const paintcore::Annotation *getAnnotation() const;

		//! Get the translation handle at the point
		Handle handleAt(const QPoint &point, float zoom) const;

		//! Adjust annotation position or size
		void adjustGeometry(Handle handle, const QPoint &delta);

		//! Get item position and size
		const QRectF &geometry() const { return _rect; }

		//! Refresh item from underlaying annotation model
		void refresh();

		//! Does the item geometry differ from the model geometry?
		bool isChanged() const;

		//! Highlight this item
		void setHighlight(bool h);

		//! Enable border
		void setShowBorder(bool show);

		//! reimplementation
		QRectF boundingRect() const;

		//! reimplementation
		int type() const { return Type; }

	protected:
		void paint(QPainter *painter, const QStyleOptionGraphicsItem *options, QWidget *);

	private:
		int _id;
		QRectF _rect;
		QRectF _oldrect;

		paintcore::LayerStack *_image;

		bool _highlight;
		bool _showborder;
};

}

#endif

