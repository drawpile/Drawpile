/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2017 Calle Laakkonen

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

#ifndef WIDGET_PRESETPIE_H
#define WIDGET_PRESETPIE_H

#include "tools/toolproperties.h"

#include <QWidget>
#include <QPixmap>

namespace color_widgets { class ColorWheel; }

namespace widgets {

class PresetPie : public QWidget {
	Q_OBJECT
public:
	//! Number of slices in the pie
	static const int SLICES = 8;

	PresetPie(QWidget *parent=nullptr);

	void setToolPreset(int slot, const tools::ToolProperties &props);

public slots:
	void showAt(const QPoint &p);
	void showAtCursor();
	void setColor(const QColor &c);
	void assignSelectedPreset();

	void assignPreset(int i);
	bool selectPreset(int i);

signals:
	void colorChanged(const QColor &c);
	void toolSelected(const tools::ToolProperties &tool);

	//! User indicated that a preset should be assigned to the given slice
	void presetRequest(int slice);

protected:
	void paintEvent(QPaintEvent*) override;
	void mouseMoveEvent(QMouseEvent*) override;
	void mouseReleaseEvent(QMouseEvent*) override;
	void leaveEvent(QEvent *e) override;
	void resizeEvent(QResizeEvent *e) override;

private:
	void redrawPreview();
	int sliceAt(const QPoint &p);

	color_widgets::ColorWheel *m_color;
	tools::ToolProperties m_preset[SLICES];
	QPixmap m_preview;
	int m_slice;
};

}

#endif
