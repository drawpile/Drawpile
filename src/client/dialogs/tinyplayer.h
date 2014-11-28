/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

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
#ifndef TINYPLAYER_H
#define TINYPLAYER_H

#include <QWidget>
#include <QMenu>

class Ui_TinyPlayer;

namespace dialogs {

class TinyPlayer : public QWidget
{
	Q_OBJECT
public:
	explicit TinyPlayer(QWidget *parent = 0);
	~TinyPlayer();

	void setMaxProgress(int max);
	void setProgress(int pos);
	void setPlayback(bool play);
	void enableIndex();
	void setMarkerMenu(QMenu *menu);

signals:
	void prevMarker();
	void nextMarker();
	void prevSnapshot();
	void nextSnapshot();
	void playToggled(bool play);
	void step();
	void skip();
	void hidden();

protected:
	void mouseMoveEvent(QMouseEvent *);
	void mouseReleaseEvent(QMouseEvent *);
	void keyReleaseEvent(QKeyEvent *);
	void contextMenuEvent(QContextMenuEvent *);
	void closeEvent(QCloseEvent *);

private:
	Ui_TinyPlayer *_ui;
	QPoint _dragpoint;
	QMenu *_ctxmenu;
	QActionGroup *_idxactions;
};

}

#endif

