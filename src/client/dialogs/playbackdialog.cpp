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
#include <QCloseEvent>
#include <QBuffer>
#include <QFileInfo>

#include <QGraphicsScene>
#include <QGraphicsRectItem>

#include <cmath>

#include "dialogs/playbackdialog.h"
#include "dialogs/videoexportdialog.h"
#include "dialogs/recfilterdialog.h"

#include "export/videoexporter.h"
#include "scene/canvasscene.h"
#include "recording/indexgraphicsitem.h"
#include "recording/indexpointergraphicsitem.h"
#include "recording/indexbuilder.h"
#include "recording/indexloader.h"
#include "statetracker.h"
#include "mainwindow.h"

#include "../shared/record/reader.h"
#include "../shared/net/recording.h"

#include "ui_playback.h"

namespace dialogs {

PlaybackDialog::PlaybackDialog(drawingboard::CanvasScene *canvas, recording::Reader *reader, QWidget *parent) :
	QDialog(parent),
	_reader(reader), _index(0), _indexscene(0), _indexpositem(0),
	_canvas(canvas), _exporter(0), _speedfactor(1.0f), _play(false), _closing(false)
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

	connect(_ui->snapshotBackwards, SIGNAL(clicked()), this, SLOT(prevSnapshot()));
	connect(_ui->snapshotForwards, SIGNAL(clicked()), this, SLOT(nextSnapshot()));

	connect(_ui->filterButton1, SIGNAL(clicked()), this, SLOT(filterRecording()));
	connect(_ui->filterButton2, SIGNAL(clicked()), this, SLOT(filterRecording()));

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

	connect(_ui->indexButton, SIGNAL(clicked()), this, SLOT(makeIndex()));

	loadIndex();
}

PlaybackDialog::~PlaybackDialog()
{
	delete _ui;
}

void PlaybackDialog::createIndexView()
{
	_indexscene = new QGraphicsScene(this);

	// Populate scene
	IndexGraphicsItem::buildScene(_indexscene, _index->index());

	// Create current position indicator item
	_indexpositem = new IndexPointerGraphicsItem(_indexscene->height());
	_indexpositem->setVisible(false);
	_indexscene->addItem(_indexpositem);

	_ui->indexView->setScene(_indexscene);
	_ui->indexStack->setCurrentIndex(1);

	_ui->indexView->centerOn(QPointF(0, 0));
}

void PlaybackDialog::closeEvent(QCloseEvent *event)
{
	if(_exporter) {
		event->ignore();
		if(!_closing) {
			QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
			_closing = true;
			_exporter->finish();
		}
	} else {
		if(_closing)
			QApplication::restoreOverrideCursor();
		QDialog::closeEvent(event);
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
	if(waitForExporter()) {
		_waitedForExporter = true;
		return;
	}

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

	updateIndexPosition();

	_ui->progressBar->setValue(_reader->position());
}

void PlaybackDialog::nextSequence()
{
	if(waitForExporter())
		return;

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

void PlaybackDialog::updateIndexPosition()
{
	if(_indexpositem) {
		_indexpositem->setIndex(_reader->current());
		_indexpositem->setVisible(true);
		_ui->indexView->centerOn(_indexpositem);
	}
}

void PlaybackDialog::jumpTo(int pos)
{
	if(!_ui->play->isEnabled())
		return;

	// Skip forward a shot distance: don't bother resetting to a snapshot
	if(pos > _reader->current() && pos - _reader->current() < 100) {
		while(_reader->current() < pos && _ui->play->isEnabled()) {
			nextCommand();
		}
		return;
	}

	// Find nearest snapshot and jump there
	int seIdx=0;
	for(int i=1;i<_index->index().snapshots().size();++i) {
		const recording::SnapshotEntry &next = _index->index().snapshots().at(i);
		if(int(next.pos) > pos)
			break;

		seIdx = i;
	}

	jumptToSnapshot(seIdx);

	while(_reader->current() < pos && _ui->play->isEnabled())
		nextCommand();
}

void PlaybackDialog::jumptToSnapshot(int idx)
{
	recording::SnapshotEntry se = _index->index().snapshots().at(idx);
	drawingboard::StateSavepoint savepoint = _index->loadSavepoint(idx, _canvas->statetracker());

	if(!savepoint) {
		qWarning() << "error loading savepoint";
		return;
	}

	_reader->seekTo(se.pos, se.stream_offset);
	_canvas->statetracker()->resetToSavepoint(savepoint);
	_ui->progressBar->setValue(_reader->position());
	updateIndexPosition();
}

void PlaybackDialog::prevSnapshot()
{
	const unsigned int current = qMin(0, _reader->current());

	int seIdx=0;
	for(int i=1;i<_index->index().snapshots().size();++i) {
		const recording::SnapshotEntry &next = _index->index().snapshots().at(i);
		if(next.pos >= current)
			break;

		seIdx = i;
	}

	jumptToSnapshot(seIdx);
	updateIndexPosition();
}

void PlaybackDialog::nextSnapshot()
{
	const unsigned int current = qMin(0, _reader->current());

	int seIdx=_index->index().snapshots().size() - 1;
	int pos = _index->index().snapshots().last().pos;
	for(int i=seIdx-1;i>=0;--i) {
		const recording::SnapshotEntry &prev = _index->index().snapshots().at(i);
		if(prev.pos <= current)
			break;

		seIdx = i;
		pos = prev.pos;
	}

	if(pos > _reader->position())
		jumptToSnapshot(seIdx);
}

void PlaybackDialog::endOfFileReached()
{
	_ui->play->setChecked(false);

	if(_index) {
		// EOF won't stop us in indexed mode
	} else {
		_ui->play->setEnabled(false);
		_ui->seek->setEnabled(false);
		_ui->skip->setEnabled(false);
		_ui->progressBar->setEnabled(false);
		_ui->progressBar->setFormat(tr("Recording ended"));
		_ui->progressBar->setTextVisible(true);
		if(_indexpositem)
			_indexpositem->setVisible(false);
	}
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
			Q_ASSERT(_exporterReady);
			_exporterReady = false;
			_ui->exportbutton->setEnabled(false);
			_exporter->saveFrame(img);
		}
	}
}

void PlaybackDialog::exportConfig()
{
	QScopedPointer<VideoExportDialog> dialog(new VideoExportDialog(this));

	VideoExporter *ve=0;
	while(!ve) {
		if(dialog->exec() != QDialog::Accepted)
			return;

		ve = dialog->getExporter();
	}
	_exporterReady = false;
	_waitedForExporter = false;
	_exporter = ve;
	_exporter->setParent(this);

	connect(_exporter, SIGNAL(exporterReady()), this, SLOT(exporterReady()), Qt::QueuedConnection);
	connect(_exporter, SIGNAL(exporterError(QString)), this, SLOT(exporterError(QString)), Qt::QueuedConnection);
	connect(_exporter, SIGNAL(exporterFinished()), this, SLOT(exporterFinished()), Qt::QueuedConnection);

	_exporter->start();

	_exportConfigAction->setEnabled(false);
	_exportFrameAction->setEnabled(true);
	_ui->frameLabel->setEnabled(true);
	_ui->timeLabel->setEnabled(true);
}

bool PlaybackDialog::waitForExporter()
{
	if(!_exporter)
		return false;
	return !_exporterReady;
}

void PlaybackDialog::exporterReady()
{
	_ui->exportbutton->setEnabled(true);
	_exporterReady = true;

	_ui->frameLabel->setText(QString::number(_exporter->frame()));

	float time = _exporter->time();
	int minutes = time / 60;
	if(minutes>0) {
		time = fmod(time, 60.0);
		_ui->timeLabel->setText(tr("%1 m. %2 s.").arg(minutes).arg(time, 0, 'f', 2));
	} else {
		_ui->timeLabel->setText(tr("%1 s.").arg(time, 0, 'f', 2));
	}

	// Stop exporting after saving the last frame
	if(_ui->play->isEnabled()==false) {
		_exporter->finish();
		_exporter = 0;
		_ui->exportbutton->setEnabled(false);
		_ui->frameLabel->setEnabled(false);
		_ui->timeLabel->setEnabled(false);
	}
	// Tried to proceed to next frame while exporter was busy
	else if(_waitedForExporter) {
		_waitedForExporter = false;
		nextCommand();
	}
}

void PlaybackDialog::exporterError(const QString &message)
{
	// Stop playback on error
	if(_play)
		_ui->play->setChecked(false);

	// Show warning message
	QMessageBox::warning(this, tr("Video error"), message);

	exporterFinished();
}

void PlaybackDialog::exporterFinished()
{
	_exporter = 0;
	_ui->exportbutton->setEnabled(true);
	_exportConfigAction->setEnabled(true);
	_exportFrameAction->setEnabled(false);
	_ui->frameLabel->setEnabled(false);
	_ui->timeLabel->setEnabled(false);

	if(_closing)
		close();
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

QString PlaybackDialog::indexFileName() const
{
	QString name = _reader->filename();
	int suffix = name.lastIndexOf('.');
	return name.left(suffix) + ".dpidx";

}

void PlaybackDialog::loadIndex()
{
	QFileInfo indexfile(indexFileName());
	if(!indexfile.exists()) {
		_ui->noindexReason->setText(tr("Index not yet generated"));
		return;
	}

	_index = new recording::IndexLoader(_reader->filename(), indexFileName());
	if(!_index->open()) {
		delete _index;
		_index = 0;
		_ui->noindexReason->setText(tr("Error loading index!"));
		return;
	}

	createIndexView();
	connect(_ui->indexView, SIGNAL(jumpRequest(int)), this, SLOT(jumpTo(int)));
}

void PlaybackDialog::makeIndex()
{
	_ui->indexButton->setEnabled(false);
	auto *builder = new recording::IndexBuilder(_reader->filename(), indexFileName(), this);

	_ui->buildProgress->setMaximum(_reader->filesize());
	connect(builder, SIGNAL(progress(int)), _ui->buildProgress, SLOT(setValue(int)), Qt::QueuedConnection);
	connect(builder, SIGNAL(done(bool, QString)), this, SLOT(indexMade(bool, QString)), Qt::QueuedConnection);

	builder->start();
}

void PlaybackDialog::indexMade(bool ok, const QString &msg)
{
	_ui->indexButton->setEnabled(true);
	_ui->buildProgress->setValue(0);

	if(ok)
		loadIndex();
	else
		QMessageBox::warning(this, tr("Error"), msg);
}

void PlaybackDialog::filterRecording()
{
	dialogs::FilterRecordingDialog dlg(this);
	if(dlg.exec() == QDialog::Accepted) {
		QString filename = dlg.filterRecording(_reader->filename());

		if(!filename.isEmpty()) {
			MainWindow *win = new MainWindow(false);
			win->open(filename);
		}
	}
}

}
