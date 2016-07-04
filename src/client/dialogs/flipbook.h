/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015 Calle Laakkonen

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

namespace paintcore {
	class LayerStack;
}

namespace dialogs {

class Flipbook : public QDialog
{
	Q_OBJECT
public:
	Flipbook(QWidget *parent=0);
	~Flipbook();

	void setLayers(paintcore::LayerStack *layers);

private slots:
	void loadFrame();
	void playPause();
	void rewind();
	void updateFps(int newFps);
	void updateRange();

private:
	void resetFrameCache();

	Ui_Flipbook *_ui;

	paintcore::LayerStack *_layers;
	QList<QPixmap> _frames;
	QTimer *_timer;
};

}

#endif // FLIPBOOK_H
