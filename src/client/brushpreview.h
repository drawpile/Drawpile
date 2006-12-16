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

#include <QFrame>

#include "brush.h"

#ifndef DESIGNER_PLUGIN
namespace widgets {
#define PLUGIN_EXPORT
#else
#include <QtDesigner/QDesignerExportWidget>
#define PLUGIN_EXPORT QDESIGNER_WIDGET_EXPORT
#endif

//! Brush previewing widget
/**
 */
class PLUGIN_EXPORT BrushPreview : public QFrame {
	Q_OBJECT
	public:
		BrushPreview(QWidget *parent=0, Qt::WindowFlags f=0);

	public slots:
		//! Set the brush to preview
		/**
		 * @param brush brush to set
		 */
		void setBrush(const drawingboard::Brush& brush);

		//! Set preview brush size
		void setSize(int size);

		//! Set preview brush opacity
		void setOpacity(int opacity);

		//! Set preview brush hardness
		void setHardness(int hardness);

		//! Enable/disable default size pressure sensitivity
		void setSizePressure(bool enable);

		//! Set foreground color
		void setColor1(const QColor& color);

		//! Set background color
		void setColor2(const QColor& color);

		//! Enable/disable default opacity pressure sensitivity
		void setOpacityPressure(bool enable);

		//! Enable/disable default hardness pressure sensitivity
		void setHardnessPressure(bool enable);

		//! Enable/disable color pressure sensitivity
		void setColorPressure(bool enable);

	protected:
		void paintEvent(QPaintEvent *);
		void resizeEvent(QResizeEvent *);
		void changeEvent(QEvent *);

	private:
		void updatePreview();
		void updateBackground();

		drawingboard::Brush brush_;
		QImage preview_;
		QPixmap bg_;
		bool sizepressure_;
		bool opacitypressure_;
		bool hardnesspressure_;
		bool colorpressure_;
		QColor color1_, color2_;
};

#ifndef DESIGNER_PLUGIN
}
#endif

#endif

