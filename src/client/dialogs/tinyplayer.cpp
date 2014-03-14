/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

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

#include <QDebug>
#include <QMouseEvent>
#include <QContextMenuEvent>

#include "dialogs/tinyplayer.h"

#include "ui_tinyplayer.h"

namespace dialogs {

TinyPlayer::TinyPlayer(QWidget *parent)
	: QWidget(parent, Qt::Tool | Qt::CustomizeWindowHint | Qt::WindowStaysOnTopHint), _dragpoint(-1, -1)
{
	hide();
	_ui = new Ui_TinyPlayer;
	_ui->setupUi(this);

	QPolygon mask;
	const int R = 2;
	mask << QPoint(R, 0) << QPoint(width() - R, 0) << QPoint(width(), R)
		<< QPoint(width(), height()-R) << QPoint(width()-R, height()) << QPoint(R, height())
		<< QPoint(0, height() - R) << QPoint(0, R);

	setMask(QRegion(mask));

	connect(_ui->prevMarker, SIGNAL(clicked()), this, SIGNAL(prevMarker()));
	connect(_ui->nextMarker, SIGNAL(clicked()), this, SIGNAL(nextMarker()));
	connect(_ui->play, SIGNAL(clicked(bool)), this, SIGNAL(playToggled(bool)));
	connect(_ui->step, SIGNAL(clicked()), this, SIGNAL(step()));
	connect(_ui->skip, SIGNAL(clicked()), this, SIGNAL(skip()));

	_ui->prevMarker->hide();
	_ui->nextMarker->hide();

	// Context menu
	_idxactions = new QActionGroup(this);
	_ctxmenu = new QMenu(this);
	_ctxmenu->addAction("Normal player", this, SLOT(restoreBigPlayer()));
	_ctxmenu->addSeparator();
	_idxactions->addAction(_ctxmenu->addAction(tr("Previous snapshot"), this, SIGNAL(prevSnapshot())));
	_idxactions->addAction(_ctxmenu->addAction(tr("Next snapshot"), this, SIGNAL(nextSnapshot())));

	_idxactions->setEnabled(false);
}

TinyPlayer::~TinyPlayer()
{
	delete _ui;
}

void TinyPlayer::setMaxProgress(int max)
{
	_ui->progress->setMaximum(max);
}

void TinyPlayer::setProgress(int pos)
{
	_ui->progress->setValue(pos);
}

void TinyPlayer::setPlayback(bool play)
{
	_ui->play->setChecked(play);
}

void TinyPlayer::enableIndex()
{
	_ui->prevMarker->show();
	_ui->nextMarker->show();
	_idxactions->setEnabled(true);
}

void TinyPlayer::mouseMoveEvent(QMouseEvent *event)
{
	if(_dragpoint.x()<0)
		_dragpoint = event->pos();
	move(event->globalPos() - _dragpoint);
}

void TinyPlayer::mouseReleaseEvent(QMouseEvent *)
{
	_dragpoint = QPoint(-1, -1);
}

void TinyPlayer::keyReleaseEvent(QKeyEvent *event)
{
	QWidget::keyReleaseEvent(event);
	if(event->key() == Qt::Key_Escape)
		restoreBigPlayer();
}

void TinyPlayer::contextMenuEvent(QContextMenuEvent *event)
{
	_ctxmenu->popup(event->globalPos());
}

void TinyPlayer::restoreBigPlayer()
{
	hide();
	emit hidden();
}

}

