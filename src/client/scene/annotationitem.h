/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2008-2015 Calle Laakkonen

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

#include <QGraphicsItem>
#include <QTextDocument>

namespace paintcore { struct Annotation; }

namespace drawingboard {

/**
 * @brief A text box that can be overlaid on the picture.
 *
 * This class inherits from QGraphicsObject so we can point to
 * its instances with QPointer
 */
class AnnotationItem : public QGraphicsItem {
public:
	enum { Type = UserType + 10 };
	static const int HANDLE=10;

	AnnotationItem(int id, QGraphicsItem *parent=0);

	void setGeometry(const QRect &rect);
	void setColor(const QColor &color);
	void setText(const QString &text);

	//! Get the ID number of this annotation
	int id() const { return m_id; }

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
	int m_id;
	QRectF m_rect;
	QColor m_color;
	QTextDocument m_doc;

	bool m_highlight;
	bool m_showborder;
};

}

#endif

