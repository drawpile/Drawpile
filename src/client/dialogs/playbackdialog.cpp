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
#include <QMenu>

#include <cmath>

#include "playbackdialog.h"
#include "videoexportdialog.h"
#include "export/videoexporter.h"
#include "canvasscene.h"

#include "../shared/record/reader.h"
#include "../shared/net/recording.h"

#include "ui_playback.h"

namespace dialogs {

PlaybackDialog::PlaybackDialog(drawingboard::CanvasScene *canvas, recording::Reader *reader, QWidget *parent) :
	QDialog(parent), _canvas(canvas), _reader(reader), _exporter(0), _speedfactor(1.0f), _play(false)
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

	QMenu *exportmenu = new QMenu(this);

	_exportFrameAction = exportmenu->addAction("Save frame", this, SLOT(exportFrame()));
	{
		QFont font = _exportFrameAction->font();
		font.setBold(true);
		_exportFrameAction->setFont(font);
		_exportFrameAction->setEnabled(false);
	}

	_autoExportAction = exportmenu->addAction("Autosave");
	_autoExportAction->setCheckable(true);
	_autoExportAction->setChecked(true);

	exportmenu->addSeparator();

	_exportConfigAction = exportmenu->addAction("Configure...", this, SLOT(exportConfig()));

	_ui->exportbutton->setMenu(exportmenu);
	connect(_ui->exportbutton, SIGNAL(clicked()), this, SLOT(exportButtonClicked()));
}

PlaybackDialog::~PlaybackDialog()
{
	delete _ui;
	if(_exporter) {
		_exporter->finish();
		delete _exporter;
	}
}

void PlaybackDialog::centerOnParent()
{
	if(parentWidget() != 0) {
		QRect parentG = parentWidget()->geometry();
		QRect myG = geometry();

		move(
			parentG.x() + parentG.width()/2 - myG.width()/2,
			parentG.y() + parentG.height()/2 - myG.height()/2
		);
	}
}

void PlaybackDialog::togglePlay(bool play)
{
	_play = play;
	if(_play)
		_timer->start(0);
	else
		_timer->stop();
}

void PlaybackDialog::nextCommand()
{
	recording::MessageRecord next = _reader->readNext();
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
			if(_play)
				_timer->start(int(qMax(1.0f, 33.0f * _speedfactor) + 0.5));

			emit commandRead(protocol::MessagePtr(next.message));
		}
		break;
	case recording::MessageRecord::INVALID:
		qWarning() << "Unrecognized command " << next.type << "of length" << next.len;
		if(_play)
			_timer->start(1);
		break;
	case recording::MessageRecord::END_OF_RECORDING:
		endOfFileReached();
		break;
	}

	if(_autoExportAction->isChecked())
		exportFrame();
	_ui->progressBar->setValue(_reader->position());
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

	if(_autoExportAction->isChecked())
		exportFrame();

	_ui->progressBar->setValue(_reader->position());
}

void PlaybackDialog::endOfFileReached()
{
	_ui->play->setChecked(false);
	_ui->play->setEnabled(false);
	_ui->seek->setEnabled(false);
	_ui->skip->setEnabled(false);
	_ui->progressBar->setEnabled(false);
	_ui->progressBar->setFormat(tr("Recording ended"));
	_ui->progressBar->setTextVisible(true);
}

void PlaybackDialog::exportButtonClicked()
{
	if(_exporter)
		exportFrame();
	else
		exportConfig();
}

void PlaybackDialog::exportFrame()
{
	if(_exporter) {
		QImage img = _canvas->image();
		if(!img.isNull()) {
			if(!_exporter->saveFrame(img)) {
				// Stop playback and exporting on error
				if(_play)
					_ui->play->setChecked(false);

				QMessageBox::warning(this, tr("Export error"), _exporter->errorString());

				delete _exporter;
				_exporter = 0;
				_exportConfigAction->setEnabled(true);
				_ui->frameLabel->setEnabled(false);
				_ui->timeLabel->setEnabled(false);
			} else {
				_ui->frameLabel->setText(QString::number(_exporter->frame()));

				float time = _exporter->time();
				int minutes = time / 60;
				if(minutes>0) {
					time = fmod(time, 60.0);
					_ui->timeLabel->setText(tr("%1 m. %2 s.").arg(minutes).arg(time, 0, 'f', 2));
				} else {
					_ui->timeLabel->setText(tr("%1 s.").arg(time, 0, 'f', 2));
				}

				if(_ui->play->isEnabled()==false) {
					// Stop exporting after saving the last frame
					_exporter->finish();
					delete _exporter;
					_exporter = 0;
					_ui->exportbutton->setEnabled(false);
					_ui->frameLabel->setEnabled(false);
					_ui->timeLabel->setEnabled(false);
				}
			}
		}
	}
}

void PlaybackDialog::exportConfig()
{
	VideoExportDialog *dialog = new VideoExportDialog(this);

	dialog->exec();

	VideoExporter *ve = dialog->getExporter();
	if(ve) {
		_exporter = ve;
		_exportFrameAction->setEnabled(true);
		_exportConfigAction->setEnabled(false);
		_ui->frameLabel->setEnabled(true);
		_ui->timeLabel->setEnabled(true);
	}

	delete dialog;
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
