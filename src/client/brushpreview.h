/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006 Calle Laakkonen

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
#ifndef BRUSHPREVIEW_H
#define BRUSHPREVIEW_H

#include <QWidget>
#include <QtDesigner/QDesignerExportWidget>

#include "brush.h"

namespace widgets {

//! Brush previewing widget
/**
 */
class QDESIGNER_WIDGET_EXPORT BrushPreview : public QWidget {
	Q_OBJECT
	public:
		BrushPreview(QWidget *parent=0);

	public slots:

	protected:
		void paintEvent(QPaintEvent *event);

	private:
		drawingboard::Brush brush_;
};

}

#endif

