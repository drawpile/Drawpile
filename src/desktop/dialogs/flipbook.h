/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015-2021 Calle Laakkonen

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

#ifndef FLIPBOOK_H
#define FLIPBOOK_H

#include <QDialog>
#include <QList>
#include <QPixmap>

class Ui_Flipbook;

class QTimer;

namespace canvas {
	class PaintEngine;
}

namespace dialogs {

class Flipbook final : public QDialog
{
	Q_OBJECT
public:
	explicit Flipbook(QWidget *parent=nullptr);
	~Flipbook() override;

	void setPaintEngine(canvas::PaintEngine *pe);

private slots:
	void loadFrame();
	void playPause();
	void rewind();
	void updateFps(int newFps);
	void updateRange();
	void setCrop(const QRectF &rect);
	void resetCrop();

private:
	void resetFrameCache();

	Ui_Flipbook *m_ui;

	canvas::PaintEngine *m_paintengine;
	QList<QPixmap> m_frames;
	QTimer *m_timer;
	QRect m_crop;
	int m_realFps;
};

}

#endif // FLIPBOOK_H
