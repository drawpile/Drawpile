/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2020 Calle Laakkonen

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

#include "libclient/brushes/brush.h"
#include "rustpile/rustpile.h"

#include <QFrame>

#ifdef DESIGNER_PLUGIN
#include <QtUiPlugin/QDesignerExportWidget>
#else
#define QDESIGNER_WIDGET_EXPORT
#endif

class QMenu;

namespace widgets {

/**
 * @brief Brush previewing widget
 */
class QDESIGNER_WIDGET_EXPORT BrushPreview : public QFrame {
	Q_OBJECT
public:
	enum PreviewShape {Stroke, Line, Rectangle, Ellipse, FloodFill, FloodErase};

	explicit BrushPreview(QWidget *parent=nullptr, Qt::WindowFlags f=Qt::WindowFlags());
	~BrushPreview();

	//! Set preview shape
	void setPreviewShape(rustpile::BrushPreviewShape shape);

	rustpile::BrushPreviewShape previewShape() const { return m_shape; }

	QColor brushColor() const { return m_brush.qColor(); }
	const brushes::ActiveBrush &brush() const { return m_brush; }

public slots:
	//! Set the brush to preview
	void setBrush(const brushes::ActiveBrush& brush);

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

	QPixmap m_background;
	brushes::ActiveBrush m_brush;

	rustpile::BrushPreview *m_previewcanvas = nullptr;
	QPixmap m_preview;

	rustpile::BrushPreviewShape m_shape = rustpile::BrushPreviewShape::Stroke;
	int m_fillTolerance = 0;
	int m_fillExpansion = 0;
	bool m_underFill = false;
	bool m_needUpdate = false;
};

}

#endif

