/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2019 Calle Laakkonen

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

#include "../../libclient/brushes/brush.h"
#include "../../libclient/core/blendmodes.h"

class QMenu;

namespace paintcore {
	class LayerStack;
	class LayerStackPixmapCacheObserver;
}

#ifndef DESIGNER_PLUGIN
//! Custom widgets
namespace widgets {
#define PLUGIN_EXPORT
#else
#include <QtUiPlugin/QDesignerExportWidget>
#define PLUGIN_EXPORT QDESIGNER_WIDGET_EXPORT
#endif

/**
 * @brief Brush previewing widget
 */
class PLUGIN_EXPORT BrushPreview : public QFrame {
	Q_OBJECT
	Q_PROPERTY(PreviewShape previewShape READ previewShape WRITE setPreviewShape)
	Q_ENUMS(PreviewShape)
public:
	enum PreviewShape {Stroke, Line, Rectangle, Ellipse, FloodFill, FloodErase};

	explicit BrushPreview(QWidget *parent=nullptr, Qt::WindowFlags f=0);
	~BrushPreview();

	//! Set preview shape
	void setPreviewShape(PreviewShape shape);

	PreviewShape previewShape() const { return m_shape; }

	QColor brushColor() const { return m_brush.color(); }
	const brushes::ClassicBrush &brush() const { return m_brush; }

public slots:
	//! Set the brush to preview
	void setBrush(const brushes::ClassicBrush& brush);

	//! This is used for flood fill preview only
	void setFloodFillTolerance(int tolerance);

	//! This is used for flood fill preview only
	void setFloodFillExpansion(int expansion);

	//! This is used for flood fill preview only
	void setUnderFill(bool underfill);

signals:
	void requestColorChange();

protected:
	void paintEvent(QPaintEvent *event);
	void resizeEvent(QResizeEvent *);
	void changeEvent(QEvent *);
	void mouseDoubleClickEvent(QMouseEvent*);

private:
	void updatePreview();
	void updateBackground();

	brushes::ClassicBrush m_brush;

	paintcore::LayerStack *m_preview = nullptr;
	paintcore::LayerStackPixmapCacheObserver *m_previewCache = nullptr;

	QColor m_bg = Qt::white;
	PreviewShape m_shape = Stroke;
	int m_fillTolerance = 0;
	int m_fillExpansion = 0;
	bool m_underFill = false;
	bool m_needupdate = false;
};

#ifndef DESIGNER_PLUGIN
}
#endif

#endif

