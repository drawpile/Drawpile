/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2007-2008 Calle Laakkonen

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
#ifndef PREVIEW_H
#define PREVIEW_H

#include <QGraphicsLineItem>

#include "core/brush.h"
#include "core/point.h"

namespace drawingboard {

//! Drawing feedback
/**
 * The stroke feedback object provides immediate feedback for the user
 * while the drawing commands are making their roundtrip through the server.
 *
 * In addition to providing feedback when drawing over the network, the
 * previews are also used with line and rectangle drawing tools to preview
 * before actually committing it to the canvas.
 */
class Preview {
	public:
		virtual ~Preview() { };

		//! Do a preview
		virtual void preview(const dpcore::Point& from, const dpcore::Point& to, const dpcore::Brush& brush);

		//! Move the end point of the preview
		virtual void moveTo(const dpcore::Point& to);

		//! Hide the preview object
		virtual void hidePreview() = 0;

		//! Get the used brush
		const dpcore::Brush& brush() const { return brush_; }
		//! Get the beginning point of the preview
		const dpcore::Point& from() const { return from_; }
		//! Get the end point of the preview
		const dpcore::Point& to() const { return to_; }

	protected:
		//! Initialize appearance
		virtual void initAppearance(const QPen& pen) = 0;

	private:
		dpcore::Brush brush_;
		dpcore::Point from_, to_;
};

//! Stroke feedback
/**
 * This class provides simple feedback for strokes and lines.
 * Brush opacity and hardness are ignored.
 */
class StrokePreview : public Preview, public QGraphicsLineItem {
	public:
		StrokePreview(QGraphicsItem *parent);

		void preview(const dpcore::Point& from, const dpcore::Point& to, const dpcore::Brush& brush);
		void moveTo(const dpcore::Point& to);
		void hidePreview() { hide(); }

	protected:
		void initAppearance(const QPen& pen);
};

//! Rectangle feedback
/**
 * This class is used to preview the rectangle tool.
 */
class RectanglePreview : public Preview, public QGraphicsRectItem {
	public:
		RectanglePreview(QGraphicsItem *parent);

		void preview(const dpcore::Point& from, const dpcore::Point& to, const dpcore::Brush& brush);
		void moveTo(const dpcore::Point& to);
		void hidePreview() { hide(); }

	protected:
		void initAppearance(const QPen& pen);
};
}

#endif

