/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2008 Calle Laakkonen

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

#include "core/brush.h"

namespace dpcore {
	class Layer;
}

#ifndef DESIGNER_PLUGIN
//! Custom widgets
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
	Q_PROPERTY(PreviewShape previewShape READ previewShape WRITE setPreviewShape)
	Q_ENUMS(PreviewShape)
	public:
		enum PreviewShape {Stroke, Line, Rectangle};

		BrushPreview(QWidget *parent=0, Qt::WindowFlags f=0);

		//! Set preview shape
		void setPreviewShape(PreviewShape shape);

		//! Get preview shape
		PreviewShape previewShape() const { return shape_; }

		//! Get the displayed brush
		const dpcore::Brush& brush() const { return brush_; }

	public slots:
		//! Set the brush to preview
		/**
		 * @param brush brush to set
		 */
		void setBrush(const dpcore::Brush& brush);

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

		//! Set dab spacing
		void setSpacing(int spacing);

		//! Enable/disable default opacity pressure sensitivity
		void setOpacityPressure(bool enable);

		//! Enable/disable default hardness pressure sensitivity
		void setHardnessPressure(bool enable);

		//! Enable/disable color pressure sensitivity
		void setColorPressure(bool enable);

	protected:
		void paintEvent(QPaintEvent *event);
		void resizeEvent(QResizeEvent *);
		void changeEvent(QEvent *);

	private:
		void updatePreview();
		void updateBackground();

		dpcore::Brush brush_;
		//QImage preview_;
		dpcore::Layer *preview_;
		QPixmap bg_;
		bool sizepressure_;
		bool opacitypressure_;
		bool hardnesspressure_;
		bool colorpressure_;
		QColor color1_, color2_;
		PreviewShape shape_;
};

#ifndef DESIGNER_PLUGIN
}
#endif

#endif

