/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2014 Calle Laakkonen

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
#ifndef BRUSHPREVIEW_H
#define BRUSHPREVIEW_H

#include <QFrame>

#include "core/brush.h"

class QMenu;

namespace paintcore {
	class LayerStack;
}

#ifndef DESIGNER_PLUGIN
//! Custom widgets
namespace widgets {
#define PLUGIN_EXPORT
#else
#include <QtDesigner/QDesignerExportWidget>
#define PLUGIN_EXPORT QDESIGNER_WIDGET_EXPORT
#endif

/**
 * @brief Brush previewing widget
 */
class PLUGIN_EXPORT BrushPreview : public QFrame {
	Q_OBJECT
	Q_PROPERTY(PreviewShape previewShape READ previewShape WRITE setPreviewShape)
	Q_PROPERTY(bool transparentBackground READ isTransparentBackground WRITE setTransparentBackground)
	Q_ENUMS(PreviewShape)
	public:
		enum PreviewShape {Stroke, Line, Rectangle, Ellipse, FloodFill};

		BrushPreview(QWidget *parent=0, Qt::WindowFlags f=0);
		~BrushPreview();

		//! Set preview shape
		void setPreviewShape(PreviewShape shape);

		//! Get preview shape
		PreviewShape previewShape() const { return shape_; }

		//! Get the displayed brush
		const paintcore::Brush& brush(bool swapcolors) const;

		bool isTransparentBackground() const { return _tranparentbg; }

	public slots:
		//! Set the brush to preview
		/**
		 * @param brush brush to set
		 */
		void setBrush(const paintcore::Brush& brush);

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

		//! Enable/disable subpixel precision
		void setSubpixel(bool enable);

		//! Select a blending mode
		void setBlendingMode(int mode);

		//! Set/unset hard edge mode (100% hardness + no subpixels)
		void setHardEdge(bool hard);

		//! Set/unset incremental drawing mode
		void setIncremental(bool incremental);

		//! Set/unset transparent layer background (affects preview only)
		void setTransparentBackground(bool tranparent);

		//! This is used for flood fill preview only
		void setFloodFillTolerance(int tolerance);

		//! This is used for flood fill preview only
		void setFloodFillExpansion(int expansion);

	signals:
		void requestFgColorChange();
		void requestBgColorChange();

	protected:
		void paintEvent(QPaintEvent *event);
		void resizeEvent(QResizeEvent *);
		void changeEvent(QEvent *);
		void mouseDoubleClickEvent(QMouseEvent*);
		void contextMenuEvent(QContextMenuEvent *);

	private:
		void updatePreview();
		void updateBackground();

		paintcore::Brush brush_;
		mutable paintcore::Brush swapbrush_;

		paintcore::LayerStack *preview_;
		bool sizepressure_;
		bool opacitypressure_;
		bool hardnesspressure_;
		bool colorpressure_;
		QColor color1_, color2_;
		PreviewShape shape_;
		qreal oldhardness1_, oldhardness2_;
		int _fillTolerance;
		int _fillExpansion;
		bool _needupdate;
		bool _tranparentbg;

		QMenu *_ctxmenu;
};

#ifndef DESIGNER_PLUGIN
}
#endif

#endif

