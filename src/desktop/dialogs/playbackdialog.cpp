/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2021 Calle Laakkonen

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

#include "dialogs/playbackdialog.h"
#include "dialogs/videoexportdialog.h"

#include "recording/playbackcontroller.h"
#include "canvas/canvasmodel.h"
#include "canvas/paintengine.h"

#include "mainwindow.h"
#include "../rustpile/rustpile.h"

#include "ui_playback.h"

#include <QDebug>
#include <QMessageBox>
#include <QInputDialog>
#include <QCloseEvent>
#include <QApplication>
#include <QTimer>
#include <QMenu>

using recording::PlaybackController;

namespace dialogs {

PlaybackDialog::PlaybackDialog(canvas::CanvasModel *canvas, QWidget *parent) :
	QDialog(parent), m_ui(new Ui_PlaybackDialog),
	m_paintengine(canvas->paintEngine()),
	m_speedFactor(1.0),
	m_autoplay(false), m_awaiting(false)
{
	setWindowTitle(tr("Playback"));
	setWindowFlags(Qt::Tool);
	setMinimumSize(200, 80);
	resize(420, 250);

	// Set up the UI
	m_ui->setupUi(this);

	m_ui->buildIndexProgress->hide();

	connect(m_ui->buildIndexButton, &QAbstractButton::clicked, this, &PlaybackDialog::onBuildIndexClicked);
	connect(m_ui->configureExportButton, &QAbstractButton::clicked, this, &PlaybackDialog::onVideoExportClicked);

	// Markers (bookmarks) are enabled when playing back a recording
	m_markers = new QMenu(this);
	m_ui->markers->setMenu(m_markers);
	m_ui->markers->setEnabled(false);
	connect(m_markers, &QMenu::triggered, this, &PlaybackDialog::onMarkerMenuTriggered);

	// Step timer is used to limit playback speed
	m_autoStepTimer = new QTimer(this);
	m_autoStepTimer->setSingleShot(true);
	connect(m_autoStepTimer, &QTimer::timeout, this, &PlaybackDialog::stepNext);

	connect(m_ui->speedcontrol, &QAbstractSlider::valueChanged, [this](int speed) {
		qreal s;
		if(speed<=100)
			s = speed / 100.0;
		else
			s = 1.0 + ((speed-100) / 100.0) * 8.0;

		m_speedFactor = 1.0 / s;
		m_ui->speedLabel->setText(QString("x %1").arg(s, 0, 'f', 1));
	});

	// The paint engine's playback callback lets us know when the step/sequence
	// has been rendered and we're free to take another one.
	connect(canvas->paintEngine(), &canvas::PaintEngine::playbackAt, this, &PlaybackDialog::onPlaybackAt);

	// |<< button skips backwards. This needs an index to work
	connect(m_ui->skipBackward, &QAbstractButton::clicked, this, [this]() {
		if(!m_awaiting) {
			m_awaiting = true;
			rustpile::paintengine_playback_step(m_paintengine->engine(), -1, true);
		}
	});

	// >>| button skips forward a whole stroke
	connect(m_ui->skipForward, &QAbstractButton::clicked, this, [this]() {
		if(!m_awaiting) {
			m_awaiting = true;
			rustpile::paintengine_playback_step(m_paintengine->engine(), 1, true);
		}
	});

	// >> button steps forward a single message
	connect(m_ui->stepForward, &QAbstractButton::clicked, this, &PlaybackDialog::stepNext);

	// play button toggles automatic step mode
	connect(m_ui->play, &QAbstractButton::toggled, this, &PlaybackDialog::setPlaying);

#if 0
	// Connect the UI to the controller
	connect(m_ui->filmStrip, &widgets::Filmstrip::doubleClicked, m_ctrl, &PlaybackController::jumpTo);

	connect(m_ctrl, &PlaybackController::indexLoaded, this, &PlaybackDialog::onIndexLoaded);
	connect(m_ctrl, &PlaybackController::indexLoadError, this, &PlaybackDialog::onIndexLoadError);
	connect(m_ctrl, &PlaybackController::indexBuildProgressed, [this](qreal p) { m_ui->buildIndexProgress->setValue(p * m_ui->buildIndexProgress->maximum()); });

	connect(m_ctrl, &PlaybackController::exportStarted, this, &PlaybackDialog::onVideoExportStarted);
	connect(m_ctrl, &PlaybackController::exportEnded, this, &PlaybackDialog::onVideoExportEnded);
	connect(m_ctrl, &PlaybackController::exportError, [this](const QString &msg) {
		QMessageBox::warning(this, tr("Video error"), msg);
	});

	connect(m_ui->saveFrame, &QAbstractButton::clicked, m_ctrl, &PlaybackController::exportFrame);
	connect(m_ui->stopExport, &QAbstractButton::clicked, m_ctrl, &PlaybackController::stopExporter);
	connect(m_ui->autoSaveFrame, &QCheckBox::toggled, m_ctrl, &PlaybackController::setAutosave);
	connect(m_ctrl, &PlaybackController::exportedFrame, [this]() {
		m_ui->frameLabel->setText(QString::number(m_ctrl->currentExportFrame()));
		m_ui->timeLabel->setText(m_ctrl->currentExportTime());
	});

	connect(m_ctrl, &PlaybackController::canSaveFrameChanged, m_ui->saveFrame, &QPushButton::setEnabled);

	rebuildMarkerMenu();

	// Automatically try to load the index
	QTimer::singleShot(0, m_ctrl, &PlaybackController::loadIndex);
#endif
}

PlaybackDialog::~PlaybackDialog()
{
	delete m_ui;
}

/**
 * @brief The paint engine has finished rendering the sequence
 *
 * We can now automatically step forward again
 */
void PlaybackDialog::onPlaybackAt(qint64 pos, qint32 interval)
{
	qInfo() << "playback at" << pos;
	m_awaiting = false;
	if(pos < 0) {
		// Negative number means we've reached the end of the recording
		setPlaying(false);
		m_ui->play->setEnabled(false);
		m_ui->skipForward->setEnabled(false);
		m_ui->stepForward->setEnabled(false);
	} else {
		m_ui->play->setEnabled(true);
		m_ui->skipForward->setEnabled(true);
		m_ui->stepForward->setEnabled(true);
		m_ui->playbackProgress->setValue(pos);
	}

	if(pos >=0 && m_autoplay) {

		if(interval > 0) {
			const auto elapsed = m_lastInterval.elapsed();
			m_lastInterval.restart();

			m_autoStepTimer->start(qMax(0.0f, interval * m_speedFactor - elapsed));
		} else {
			m_autoStepTimer->start(33.0 * m_speedFactor);
		}
	}
}

void PlaybackDialog::stepNext() {
	if(!m_awaiting) {
		m_awaiting = true;
		rustpile::paintengine_playback_step(m_paintengine->engine(), 1, false);
	}
}

void PlaybackDialog::onBuildIndexClicked()
{
	m_ui->noIndexReason->setText(tr("Building index..."));
	m_ui->buildIndexProgress->show();
	m_ui->buildIndexButton->setEnabled(false);
#if 0
	m_ctrl->buildIndex();
#endif
}

void PlaybackDialog::onIndexLoaded()
{
#if 0
	disconnect(m_ctrl, &PlaybackController::progressChanged, m_ui->filmStrip, &widgets::Filmstrip::setCursor);
	connect(m_ctrl, &PlaybackController::indexPositionChanged,m_ui->filmStrip, &widgets::Filmstrip::setCursor);

	m_ui->filmStrip->setLength(m_ctrl->maxIndexPosition());
	m_ui->filmStrip->setFrames(m_ctrl->indexThumbnailCount());
	m_ui->filmStrip->setCursor(m_ctrl->indexPosition());


	m_ui->skipBackward->setEnabled(true);

	m_ui->buildIndexButton->hide();
	m_ui->buildIndexProgress->hide();
	m_ui->noIndexReason->hide();

	m_ui->filmStrip->setLoadImageFn(std::bind(&PlaybackController::getIndexThumbnail, m_ctrl, std::placeholders::_1));

	rebuildMarkerMenu();
#endif
}

void PlaybackDialog::onIndexLoadError(const QString &msg, bool canRetry)
{
	m_ui->buildIndexProgress->hide();
	m_ui->noIndexReason->setText(msg);
	m_ui->buildIndexButton->setEnabled(canRetry);
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

bool PlaybackDialog::isPlaying() const
{
	return m_ui->play->isChecked();
}

void PlaybackDialog::setPlaying(bool playing)
{
	m_ui->play->setChecked(playing);
	if(playing)
		m_lastInterval.restart();
	else
		m_autoStepTimer->stop();

	m_autoplay = playing;
	if(playing && !m_awaiting) {
		m_awaiting = true;
		rustpile::paintengine_playback_step(m_paintengine->engine(), 1, false);
	}
}

#if 0 // FIXME REPLACE WITH RUST
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
#endif

void PlaybackDialog::closeEvent(QCloseEvent *event)
{
	// TODO call rustpile end playback
	QDialog::closeEvent(event);
}

void PlaybackDialog::keyPressEvent(QKeyEvent *event)
{
	// This is not a OK/Cancel type dialog, so disable
	// key events. Without this, it is easy to close
	// the window accidentally by hitting Esc.
	event->ignore();
}

void PlaybackDialog::rebuildMarkerMenu()
{
#if 0
	m_markers->clear();
	QAction *stopOnMarkers = m_markers->addAction(tr("Stop on markers"));
	stopOnMarkers->setCheckable(true);
	stopOnMarkers->setChecked(true);
	connect(stopOnMarkers, &QAction::triggered, m_ctrl, &PlaybackController::setStopOnMarkers);

	m_markers->addSeparator();

	QStringList markers = m_ctrl->getMarkers();
	if(markers.isEmpty()) {
		QAction *a = m_markers->addAction(tr("No indexed markers"));
		a->setEnabled(false);

	} else {
		for(int i=0;i<markers.size();++i) {
			QAction *a = m_markers->addAction(markers.at(i));
			a->setProperty("markeridx", i);
		}
	}
#endif
}

void PlaybackDialog::onMarkerMenuTriggered(QAction *a)
{
#if 0
	QVariant idx = a->property("markeridx");
	if(idx.isValid())
		m_ctrl->jumpToMarker(idx.toInt());
#endif
}

void PlaybackDialog::onVideoExportClicked()
{
#if 0
	QScopedPointer<VideoExportDialog> dialog(new VideoExportDialog(this));

	VideoExporter *ve=0;
	while(!ve) {
		if(dialog->exec() != QDialog::Accepted)
			return;

		ve = dialog->getExporter();
	}

	m_ctrl->startVideoExport(ve);
#endif
}

void PlaybackDialog::onVideoExportStarted()
{
	m_ui->exportStack->setCurrentIndex(0);
}

void PlaybackDialog::onVideoExportEnded()
{
	m_ui->exportStack->setCurrentIndex(1);
}

}
