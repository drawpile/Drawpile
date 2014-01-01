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
#ifndef PLAYBACKDIALOG_H
#define PLAYBACKDIALOG_H

#include <QDialog>

#include "../shared/net/message.h"

namespace recording {
	class Reader;
}

class QTimer;

class Ui_PlaybackDialog;

namespace dialogs {

class PlaybackDialog : public QDialog
{
	Q_OBJECT
public:
	explicit PlaybackDialog(recording::Reader *reader, QWidget *parent = 0);

	static recording::Reader *openRecording(const QString &filename, QWidget *msgboxparent=0);

	bool isPlaying() const { return _play; }

signals:
	void commandRead(protocol::MessagePtr msg);
	void playbackToggled(bool play);

public slots:
	void togglePlay(bool play);
	void nextCommand();
	void nextSequence();

private:
	void endOfFileReached();

	Ui_PlaybackDialog *_ui;
	recording::Reader *_reader;
	QTimer *_timer;
	float _speedfactor;
	bool _play;
};

}

#endif // PLAYBACKDIALOG_H
