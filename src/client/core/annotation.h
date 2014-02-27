/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

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
#ifndef PAINTCORE_ANNOTATION_H
#define PAINTCORE_ANNOTATION_H

#include <QRectF>
#include <QColor>

class QPainter;
class QTextDocument;

namespace paintcore {

class Annotation
{
public:
	Annotation(int id);
	Annotation(const Annotation &a);
	~Annotation();

	/**
	 * @brief Get the ID of the annotation
	 * @return
	 */
	int id() const { return _id; }

	/**
	 * @brief Get the annotation text content
	 * @return
	 */
	const QString &text() const { return _text; }

	void setText(const QString &text);

	/**
	 * @brief Check if this annotation has no content
	 * @return
	 */
	bool isEmpty() const;

	/**
	 * @brief Get the text box coordinates
	 * @return
	 */
	const QRect &rect() const { return _rect; }

	void setRect(const QRect &rect) { _rect = rect; }

	/**
	 * @brief Get the background color of the text box
	 * @return
	 */
	const QColor &backgroundColor() const { return _bgcolor; }

	void setBackgroundColor(const QColor &color) { _bgcolor = color; }

	/**
	 * Paint this annotation using the given painter.
	 * @param painter the painter to use
	 */
	void paint(QPainter *painter) const;

	/**
	 * @brief Paint this annotation using the given painter onto the given rectangle
	 * @param painter
	 * @param rect
	 */
	void paint(QPainter *painter, const QRectF &rect) const;

	/**
	 * @brief Render this annotation onto an image
	 * @return
	 */
	QImage toImage() const;

private:
	int _id;
	QRect _rect;
	QString _text;
	QColor _bgcolor;
	mutable QTextDocument *_doc;
};

}

#endif
