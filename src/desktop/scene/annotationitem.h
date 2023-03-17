/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2008-2021 Calle Laakkonen

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

namespace drawingboard {

/**
 * @brief A text box that can be overlaid on the picture.
 */
class AnnotationItem final : public QGraphicsItem {
public:
	enum { Type = UserType + 10 };
	static const int HANDLE=10;

	explicit AnnotationItem(int id, QGraphicsItem *parent=nullptr);

	//! Get the ID number of this annotation
	int id() const { return m_id; }

	//! Get the user part of the ID
	int userId() const { return (m_id >> 8) & 0xff; }

	//! Set the text box position and size
	void setGeometry(const QRect &rect);

	QRectF rect() const { return m_rect; }

	//! Set the background color (transparent by default)
	void setColor(const QColor &color);

	QColor color() const { return m_color; }

	//! Set the text content (subset of HTML is supported)
	void setText(const QString &text);

	QString text() const { return m_doc.toHtml(); }

	//! Set vertical alignment
	void setValign(int valign);

	int valign() const { return m_valign; }

	//! Set the "protected" flag
	void setProtect(bool protect) { m_protect = protect; }
	bool protect() const { return m_protect; }

	//! Highlight this item
	void setHighlight(bool h);

	//! Enable border
	void setShowBorder(bool show);

	QRectF boundingRect() const override;

	int type() const override { return Type; }

	//! Render this annotation into an image
	QImage toImage() const;

protected:
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *options, QWidget *) override;

private:
	void paintHiddenBorder(QPainter *painter);

	int m_id;
	int m_valign;
	QRectF m_rect;
	QColor m_color;
	QTextDocument m_doc;

	bool m_highlight;
	bool m_showborder;
	bool m_protect;
};

}

#endif

