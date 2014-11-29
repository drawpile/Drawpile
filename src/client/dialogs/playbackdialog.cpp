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

#include <QDebug>
#include <QMessageBox>
#include <QInputDialog>
#include <QScopedPointer>
#include <QTimer>
#include <QCloseEvent>
#include <QFileInfo>
#include <QSettings>
#include <QThreadPool>

#include <QGraphicsScene>
#include <QGraphicsRectItem>

#include <cmath>

#include "dialogs/playbackdialog.h"
#include "dialogs/videoexportdialog.h"
#include "dialogs/recfilterdialog.h"
#include "dialogs/tinyplayer.h"

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
#include "ui_tinyplayer.h"

namespace dialogs {

PlaybackDialog::PlaybackDialog(drawingboard::CanvasScene *canvas, recording::Reader *reader, QWidget *parent) :
	QDialog(parent),
	_reader(reader), _index(0), _indexscene(0), _indexpositem(0), _markermenu(0),
	_canvas(canvas), _exporter(0), _speedfactor(1.0f), _play(false), _closing(false)
{
	setWindowFlags(Qt::Tool);
	_reader->setParent(this);
	_ui = new Ui_PlaybackDialog;
	_ui->setupUi(this);

	_tinyPlayer = new TinyPlayer(parent);

	_timer = new QTimer(this);
	_timer->setSingleShot(true);
	connect(_timer, SIGNAL(timeout()), this, SLOT(nextCommand()));

	if(reader->isCompressed()) {
		_ui->progressBar->setEnabled(false);
		_ui->progressBar->setFormat(tr("Compressed recording"));
		_ui->progressBar->setTextVisible(true);
	} else {
		_ui->progressBar->setMaximum(_reader->filesize());
		_tinyPlayer->setMaxProgress(_reader->filesize());
	}

	// Playback control
	connect(_ui->play, SIGNAL(toggled(bool)), this, SLOT(togglePlay(bool)));
	connect(_ui->seek, SIGNAL(clicked()), this, SLOT(nextCommand()));
	connect(_ui->skip, SIGNAL(clicked()), this, SLOT(nextSequence()));

	connect(_ui->snapshotBackwards, SIGNAL(clicked()), this, SLOT(prevSnapshot()));
	connect(_ui->snapshotForwards, SIGNAL(clicked()), this, SLOT(nextSnapshot()));
	connect(_ui->prevMarker, SIGNAL(clicked()), this, SLOT(prevMarker()));
	connect(_ui->nextMarker, SIGNAL(clicked()), this, SLOT(nextMarker()));

	connect(_ui->speedcontrol, &QDial::valueChanged, [this](int speed) {
		if(speed<=100)
			_speedfactor = speed / 100.0f;
		else
			_speedfactor = 1.0f + ((speed-100) / 100.f) * 8;

		_ui->speedBox->setValue(_speedfactor * 100);
		//_ui->speedbox->setText(tr("Speed: %1%").arg(_speedfactor * 100, 0, 'f', 0));

		_speedfactor = 1.0f / _speedfactor;
	});

	// Filtering and indexing
	connect(_ui->filterButton1, SIGNAL(clicked()), this, SLOT(filterRecording()));
	connect(_ui->filterButton2, SIGNAL(clicked()), this, SLOT(filterRecording()));
	connect(_ui->indexButton, SIGNAL(clicked()), this, SLOT(makeIndex()));
	connect(_ui->markButton, SIGNAL(clicked()), this, SLOT(addMarkerHere()));

	// Video export
	connect(_ui->configureExportButton, SIGNAL(clicked()), this, SLOT(exportConfig()));
	connect(_ui->saveFrame, SIGNAL(clicked()), this, SLOT(exportFrame()));
	connect(_ui->stopExport, SIGNAL(clicked()), this, SLOT(stopExportClicked()));

	// Tiny player controls
	connect(_ui->smallPlayer, &QCheckBox::clicked, [this]() {
		QSize size = _tinyPlayer->size();
		_tinyPlayer->move(QCursor::pos() - QPoint(size.width()/2, size.height()/2));
		_tinyPlayer->show();
		this->hide();
		_ui->smallPlayer->setChecked(false);
	});

	connect(_tinyPlayer, SIGNAL(hidden()), this, SLOT(show()));
	connect(_tinyPlayer, SIGNAL(prevMarker()), this, SLOT(prevMarker()));
	connect(_tinyPlayer, SIGNAL(nextMarker()), this, SLOT(nextMarker()));
	connect(_tinyPlayer, SIGNAL(prevSnapshot()), this, SLOT(prevSnapshot()));
	connect(_tinyPlayer, SIGNAL(nextSnapshot()), this, SLOT(nextSnapshot()));
	connect(_tinyPlayer, SIGNAL(playToggled(bool)), _ui->play, SLOT(setChecked(bool)));
	connect(_tinyPlayer, SIGNAL(step()), this, SLOT(nextCommand()));
	connect(_tinyPlayer, SIGNAL(skip()), this, SLOT(nextSequence()));

	loadIndex();

	// Restore settings
	QSettings cfg;
	cfg.beginGroup("playback");
	_ui->stopOnMarkers->setChecked(cfg.value("stoponmarkers", true).toBool());
	_ui->maxinterval->setValue(cfg.value("maxinterval", 1.0).toDouble());

	// Automatically take the first step
	_timer->start(0);
}

PlaybackDialog::~PlaybackDialog()
{
	// Remember some settings
	QSettings cfg;
	cfg.beginGroup("playback");

	cfg.setValue("stoponmarkers", _ui->stopOnMarkers->isChecked());
	cfg.setValue("maxinterval", _ui->maxinterval->value());

	delete _tinyPlayer;
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

bool PlaybackDialog::exitCleanup()
{
	if(_indexbuilder)
		_indexbuilder->abort();

	if(_exporter) {
		if(!_closing) {
			QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
			_closing = true;
			_exporter->finish();
		}
		return false;
	} else {
		if(_closing)
			QApplication::restoreOverrideCursor();
		return true;
	}
}

void PlaybackDialog::closeEvent(QCloseEvent *event)
{
	if(!exitCleanup())
		event->ignore();
	else
		QDialog::closeEvent(event);
}

void PlaybackDialog::done(int r)
{
	if(exitCleanup())
		QDialog::done(r);
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

	_tinyPlayer->setPlayback(play);
	emit playbackToggled(play);
}

void PlaybackDialog::nextCommand()
{
	int c = 1;
	if(_play)
		c = qMax(1, _ui->speedBox->value() / 100);
	nextCommand(c);
}

void PlaybackDialog::nextCommand(int stepcount)
{
	if(stepcount<=0)
		return;

	if(waitForExporter()) {
		_waitedForExporter = true;
		return;
	}

	recording::MessageRecord next = _reader->readNext();

	int writeFrames = 1;

	switch(next.status) {
	case recording::MessageRecord::OK: {
		protocol::MessagePtr msg(next.message);

		if(msg->type() == protocol::MSG_INTERVAL) {
			if(_play) {
				// Autoplay mode: pause for the given interval
				int interval = msg.cast<protocol::Interval>().milliseconds();
				float maxinterval = _ui->maxinterval->value() * 1000;
				_timer->start(qMin(maxinterval, interval * _speedfactor));

				// Export pause
				if(_exporter && _ui->autoSaveFrame->isChecked()) {
					int pauseframes = qRound(qMin(double(interval), _ui->maxinterval->value() * 1000) / 1000.0 *  _exporter->fps());
					if(pauseframes>0)
						writeFrames = pauseframes;
				}

			} else {
				// Manual mode: skip interval
				nextCommand(1);
				return;
			}
		} else {
			if(_play) {
				if(msg->type() == protocol::MSG_MARKER && _ui->stopOnMarkers->isChecked()) {
					_ui->play->setChecked(false);
				} else {
					if(stepcount==1)
						_timer->start(int(qMax(1.0f, 33.0f * _speedfactor) + 0.5));
				}
			}

			emit commandRead(msg);
		}
		break;
	}
	case recording::MessageRecord::INVALID:
		qWarning() << "Unrecognized command " << next.error.type << "of length" << next.error.len;
		if(_play)
			_timer->start(1);
		break;
	case recording::MessageRecord::END_OF_RECORDING:
		endOfFileReached();
		break;
	}

	if(stepcount>1) {
		nextCommand(stepcount-1);

	} else {
		if(_ui->autoSaveFrame->isChecked())
			exportFrame(writeFrames);

		updateIndexPosition();
	}
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
			qWarning() << "Unrecognized command " << next.error.type << "of length" << next.error.len;
			break;
		case recording::MessageRecord::END_OF_RECORDING:
			endOfFileReached();
			loop = false;
			break;
		}
	}

	if(_ui->autoSaveFrame->isChecked())
		exportFrame();

	updateIndexPosition();
}

void PlaybackDialog::updateIndexPosition()
{
	if(!_reader->isCompressed()) {
		if(_indexpositem) {
			_indexpositem->setIndex(_reader->currentIndex());
			_indexpositem->setVisible(true);
			_ui->indexView->centerOn(_indexpositem);
		}
		_ui->progressBar->setValue(_reader->currentPosition());
		_tinyPlayer->setProgress(_reader->currentPosition());
	}
}

void PlaybackDialog::jumpTo(int pos)
{
	Q_ASSERT(_index);

	if(!_ui->play->isEnabled() || waitForExporter())
		return;

	// If the target position is behind current position or sufficiently far ahead, jump
	// to the closest snapshot point first
	if(pos < _reader->currentIndex() || pos - _reader->currentIndex() > 100) {
		int seIdx=0;
		for(int i=1;i<_index->index().snapshots().size();++i) {
			const recording::SnapshotEntry &next = _index->index().snapshots().at(i);
			if(int(next.pos) > pos)
				break;

			seIdx = i;
		}

		jumptToSnapshot(seIdx);
	}

	// Now the current position is somewhere before the target position: replay commands
	while(_reader->currentIndex() < pos && !_reader->isEof()) {
		recording::MessageRecord next = _reader->readNext();
		switch(next.status) {
		case recording::MessageRecord::OK:
			if(next.message->type() == protocol::MSG_INTERVAL) {
				// skip intervals
				delete next.message;
			} else {
				emit commandRead(protocol::MessagePtr(next.message));
			}
			break;
		case recording::MessageRecord::INVALID:
			qWarning() << "Unrecognized command " << next.error.type << "of length" << next.error.len;
			break;
		case recording::MessageRecord::END_OF_RECORDING:
			endOfFileReached();
			break;
		}
	}

	if(_ui->autoSaveFrame->isChecked())
		exportFrame();

	updateIndexPosition();
}

void PlaybackDialog::jumptToSnapshot(int idx)
{
	Q_ASSERT(_index);
	recording::SnapshotEntry se = _index->index().snapshots().at(idx);
	drawingboard::StateSavepoint savepoint = _index->loadSavepoint(idx, _canvas->statetracker());

	if(!savepoint) {
		qWarning() << "error loading savepoint";
		return;
	}

	_reader->seekTo(se.pos, se.stream_offset);
	_canvas->statetracker()->resetToSavepoint(savepoint);
	updateIndexPosition();
}

void PlaybackDialog::prevSnapshot()
{
	Q_ASSERT(_index);
	const unsigned int current = qMax(0, _reader->currentIndex());

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
	Q_ASSERT(_index);
	const unsigned int current = qMax(0, _reader->currentIndex());

	int seIdx=_index->index().snapshots().size() - 1;
	int pos = _index->index().snapshots().last().pos;
	for(int i=seIdx-1;i>=0;--i) {
		const recording::SnapshotEntry &prev = _index->index().snapshots().at(i);
		if(prev.pos <= current)
			break;

		seIdx = i;
		pos = prev.pos;
	}

	if(pos > _reader->currentIndex())
		jumptToSnapshot(seIdx);
}

void PlaybackDialog::prevMarker()
{
	Q_ASSERT(_index);
	recording::MarkerEntry e = _index->index().prevMarker(qMax(0, _reader->currentIndex()));
	if(e.pos>0) {
		jumpTo(e.pos);
	}
}

void PlaybackDialog::nextMarker()
{
	Q_ASSERT(_index);
	recording::MarkerEntry e = _index->index().nextMarker(qMax(0, _reader->currentIndex()));
	if(e.pos>0) {
		jumpTo(e.pos);
	}
}

void PlaybackDialog::addMarkerHere()
{
	Q_ASSERT(_index);

	bool ok;
	QString title = QInputDialog::getText(this, tr("Mark Position"), tr("Marker text"), QLineEdit::Normal, QString(), &ok);
	if(ok) {
		recording::IndexEntry e = _index->index().addMarker(_reader->currentPosition(), _reader->currentIndex(), title);
		IndexGraphicsItem::addToScene(e, _indexscene);

		updateMarkerMenu();
	}
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

		_ui->smallPlayer->setEnabled(false);
	}
}

void PlaybackDialog::exportFrame(int count)
{
	Q_ASSERT(count>0);
	if(_exporter) {
		QImage img = _canvas->image();
		if(!img.isNull()) {
			Q_ASSERT(_exporterReady);
			_exporterReady = false;
			_ui->saveFrame->setEnabled(false);
			_exporter->saveFrame(img, count);
		}
	}
}

void PlaybackDialog::stopExportClicked()
{
	Q_ASSERT(_exporter);

	_ui->play->setChecked(false);
	_ui->stopExport->setEnabled(false);
	_exporter->finish();
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

	_ui->exportStack->setCurrentIndex(0);
}

bool PlaybackDialog::waitForExporter()
{
	if(!_exporter)
		return false;
	return !_exporterReady;
}

void PlaybackDialog::exporterReady()
{
	_ui->saveFrame->setEnabled(true);
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
	_ui->saveFrame->setEnabled(false);
	_ui->frameLabel->setEnabled(false);
	_ui->timeLabel->setEnabled(false);
	_ui->stopExport->setEnabled(true);

	_ui->exportStack->setCurrentIndex(1);

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
			QMessageBox::warning(msgboxparent, tr("Open Recording"), warning);
			return 0;
		} else {
			int res = QMessageBox::warning(msgboxparent, tr("Open Recording"), warning, QMessageBox::Ok, QMessageBox::Cancel);
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
	if(_reader->isCompressed()) {
		_ui->noindexReason->setText(tr("Cannot index compressed recordings."));
		_ui->indexButton->setEnabled(false);
		return;
	}

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

	_tinyPlayer->enableIndex();

	// Construct jump-to-marker list
	_markermenu = new QMenu(tr("Jump To"), this);
	updateMarkerMenu();
	_tinyPlayer->setMarkerMenu(_markermenu);

	_ui->jumpToMarker->setMenu(_markermenu);

	connect(_markermenu, &QMenu::triggered, [this](QAction *a) {
		jumpTo(a->property("streamidx").toInt());
	});

	// Re-enable controls in case the playback has already finished
	_ui->play->setEnabled(true);
	_ui->seek->setEnabled(true);
	_ui->skip->setEnabled(true);
	_ui->progressBar->setEnabled(true);
	_ui->progressBar->setTextVisible(false);
	_ui->smallPlayer->setEnabled(true);

	updateIndexPosition();
}

void PlaybackDialog::updateMarkerMenu()
{
	_markermenu->clear();
	int untitled=0;
	foreach(const recording::MarkerEntry &me, _index->index().markers()) {
		QString title = me.title;
		if(title.isEmpty())
			title = QString("#%1").arg(++untitled);

		_markermenu->addAction(title)->setProperty("streamidx", me.pos);
	}
	_ui->jumpToMarker->setDisabled(_markermenu->isEmpty());
}

void PlaybackDialog::makeIndex()
{
	Q_ASSERT(_indexbuilder.isNull());

	_ui->indexButton->setEnabled(false);
	_indexbuilder = new recording::IndexBuilder(_reader->filename(), indexFileName());

	_ui->buildProgress->setMaximum(_reader->filesize());
	connect(_indexbuilder, SIGNAL(progress(int)), _ui->buildProgress, SLOT(setValue(int)));
	connect(_indexbuilder, SIGNAL(done(bool, QString)), this, SLOT(indexMade(bool, QString)));

	QThreadPool::globalInstance()->start(_indexbuilder);
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

	// Get entries to silence
	if(_indexscene) {
		recording::IndexVector silence, markers;

		foreach(const QGraphicsItem *item, _indexscene->items()) {
			const IndexGraphicsItem *gi = qgraphicsitem_cast<const IndexGraphicsItem*>(item);
			// silenced entry (excluding added ones)
			if(gi && gi->isSilenced() && !(gi->entry().flags & recording::IndexEntry::FLAG_ADDED))
				silence.append(gi->entry());

			// added non-silenced markers
			else if(gi && !gi->isSilenced() && gi->entry().type == recording::IDX_MARKER && (gi->entry().flags & recording::IndexEntry::FLAG_ADDED))
				markers.append(gi->entry());
		}

		dlg.setSilence(silence);
		dlg.setNewMarkers(markers);
	}

	if(dlg.exec() == QDialog::Accepted) {
		QString filename = dlg.filterRecording(_reader->filename());

		if(!filename.isEmpty()) {
			MainWindow *win = new MainWindow(false);
			win->open(QUrl::fromLocalFile(filename));
		}
	}
}

}
