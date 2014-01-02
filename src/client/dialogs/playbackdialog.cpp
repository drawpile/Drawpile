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
#include <QMessageBox>
#include <QScopedPointer>
#include <QTimer>

#include "playbackdialog.h"
#include "ui_playback.h"
#include "../shared/record/reader.h"
#include "../shared/net/recording.h"

namespace dialogs {

PlaybackDialog::PlaybackDialog(recording::Reader *reader, QWidget *parent) :
	QDialog(parent), _reader(reader), _speedfactor(1.0f), _play(false)
{
	setWindowFlags(Qt::Tool);
	_reader->setParent(this);
	_ui = new Ui_PlaybackDialog;
	_ui->setupUi(this);

	_timer = new QTimer(this);
	_timer->setSingleShot(true);
	connect(_timer, SIGNAL(timeout()), this, SLOT(nextCommand()));

	_ui->progressBar->setMaximum(_reader->filesize());

	connect(_ui->play, SIGNAL(toggled(bool)), this, SLOT(togglePlay(bool)));
	connect(_ui->play, SIGNAL(toggled(bool)), this, SIGNAL(playbackToggled(bool)));
	connect(_ui->seek, SIGNAL(clicked()), this, SLOT(nextCommand()));
	connect(_ui->skip, SIGNAL(clicked()), this, SLOT(nextSequence()));

	connect(_ui->speedcontrol, &QDial::valueChanged, [this](int speed) {
		if(speed<=100)
			_speedfactor = speed / 100.0f;
		else
			_speedfactor = 1.0f + ((speed-100) / 100.f) * 5;

		_ui->speedlabel->setText(tr("Speed: %1%").arg(_speedfactor * 100, 0, 'f', 0));

		_speedfactor = 1.0f / _speedfactor;
	});
}

void PlaybackDialog::togglePlay(bool play)
{
	_play = play;
	if(_play)
		_timer->start(0);
}

void PlaybackDialog::nextCommand()
{
	recording::MessageRecord next = _reader->readNext();
	_ui->progressBar->setValue(_reader->position());
	switch(next.status) {
	case recording::MessageRecord::OK:
		if(next.message->type() == protocol::MSG_INTERVAL) {
			if(_play) {
				// autoplay mode: set timer
				int interval = static_cast<protocol::Interval*>(next.message)->milliseconds();
				float maxinterval = _ui->maxinterval->value() * 1000;
				_timer->start(qMin(maxinterval, interval * _speedfactor));
			} else {
				// manual mode: skip interval
				nextCommand();
			}
			delete next.message;
		} else {
			emit commandRead(protocol::MessagePtr(next.message));
			if(_play)
				_timer->start(1);
		}
		break;
	case recording::MessageRecord::INVALID:
		qWarning() << "Unrecognized command " << next.type << "of length" << next.len;
		break;
	case recording::MessageRecord::END_OF_RECORDING:
		endOfFileReached();
		break;
	}
}

void PlaybackDialog::nextSequence()
{
	// Play back until either an undopoint or end-of-file is reached
	bool loop=true;
	while(loop) {
		recording::MessageRecord next = _reader->readNext();
		switch(next.status) {
		case recording::MessageRecord::OK:
			if(next.message->type() == protocol::MSG_INTERVAL) {
				// skip intervals
				delete next.message;
			} else {
				emit commandRead(protocol::MessagePtr(next.message));
				if(next.message->type() == protocol::MSG_UNDOPOINT)
					loop = false;
			}
			break;
		case recording::MessageRecord::INVALID:
			qWarning() << "Unrecognized command " << next.type << "of length" << next.len;
			break;
		case recording::MessageRecord::END_OF_RECORDING:
			endOfFileReached();
			loop = false;
			break;
		}
	}

	_ui->progressBar->setValue(_reader->position());
}

void PlaybackDialog::endOfFileReached()
{
	_ui->play->setChecked(false);
	_ui->play->setEnabled(false);
	_ui->seek->setEnabled(false);
	_ui->skip->setEnabled(false);
	_ui->progressBar->setEnabled(false);
}

recording::Reader *PlaybackDialog::openRecording(const QString &filename, QWidget *msgboxparent)
{
	QScopedPointer<recording::Reader> reader(new recording::Reader(filename));

	recording::Compatibility result = reader->open();

	QString warning;
	bool fatal=false;

	switch(result) {
	using namespace recording;
	case COMPATIBLE: break;
	case MINOR_INCOMPATIBILITY:
		warning = tr("This recording was made with a different Drawpile version (%1) and may appear differently").arg(reader->writerVersion());
		break;
	case UNKNOWN_COMPATIBILITY:
		warning = tr("This recording was made with a newer Drawpile version (%1) which might not be compatible").arg(reader->writerVersion());
		break;
	case INCOMPATIBLE:
		warning = tr("Recording is incompatible. This recording was made with Drawpile version %1.").arg(reader->writerVersion());
		fatal = true;
		break;
	case NOT_DPREC:
		warning = tr("Selected file is not a Drawpile recording");
		fatal = true;
		break;
	case CANNOT_READ:
		warning = tr("Cannot read file: %1").arg(reader->errorString());
		fatal = true;
		break;
	}

	if(!warning.isNull()) {
		if(fatal) {
			QMessageBox::warning(msgboxparent, tr("Open recording"), warning);
			return 0;
		} else {
			int res = QMessageBox::warning(msgboxparent, tr("Open recording"), warning, QMessageBox::Ok, QMessageBox::Cancel);
			if(res != QMessageBox::Ok)
				return 0;
		}
	}

	return reader.take();
}

}
