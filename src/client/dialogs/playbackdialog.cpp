/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2016 Calle Laakkonen

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
#include "dialogs/recfilterdialog.h"

#include "widgets/filmstrip.h"
using widgets::Filmstrip;

#include "recording/playbackcontroller.h"

#include "mainwindow.h"

#include "../shared/record/reader.h"
#include "../shared/net/recording.h"

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

PlaybackDialog::PlaybackDialog(canvas::CanvasModel *canvas, recording::Reader *reader, QWidget *parent) :
	QDialog(parent), m_ui(new Ui_PlaybackDialog), m_closing(false)
{
	setWindowTitle(tr("Playback"));
	setWindowFlags(Qt::Tool);
	setMinimumSize(200, 80);
	resize(420, 250);

	// Set up the UI
	m_ui->setupUi(this);

	m_ui->buildIndexProgress->hide();
	m_ui->noIndexReason->hide();

	connect(m_ui->buildIndexButton, &QAbstractButton::clicked, this, &PlaybackDialog::onBuildIndexClicked);
	connect(m_ui->filterButton, &QAbstractButton::clicked, this, &PlaybackDialog::onFilterRecordingClicked);
	connect(m_ui->configureExportButton, &QAbstractButton::clicked, this, &PlaybackDialog::onVideoExportClicked);

	m_markers = new QMenu(this);
	m_ui->markers->setMenu(m_markers);
	connect(m_markers, &QMenu::triggered, this, &PlaybackDialog::onMarkerMenuTriggered);

	// Connect the UI to the controller
	m_ctrl = new PlaybackController(canvas, reader, this);

	connect(m_ctrl, &PlaybackController::playbackToggled, this, &PlaybackDialog::playbackToggled);
	connect(m_ctrl, &PlaybackController::commandRead, this, &PlaybackDialog::commandRead);

	connect(m_ui->play, &QAbstractButton::clicked, m_ctrl, &PlaybackController::setPlaying);
	connect(m_ctrl, &PlaybackController::playbackToggled, m_ui->play, &QAbstractButton::setChecked);

	connect(m_ui->skipBackward, &QAbstractButton::clicked, m_ctrl, &PlaybackController::prevSequence);
	connect(m_ui->skipForward, &QAbstractButton::clicked, m_ctrl, &PlaybackController::nextSequence);
	connect(m_ui->stepForward, &QAbstractButton::clicked, m_ctrl, &PlaybackController::nextCommand);
	connect(m_ui->speedcontrol, &QAbstractSlider::valueChanged, [this](int speed) {
		qreal s;
		if(speed<=100)
			s = speed / 100.0;
		else
			s = 1.0 + ((speed-100) / 100.0) * 8.0;

		m_ctrl->setSpeedFactor(s);
		m_ui->speedLabel->setText(QString("x %1").arg(s, 0, 'f', 1));
	});

	connect(m_ui->filmStrip, &Filmstrip::doubleClicked, m_ctrl, &PlaybackController::jumpTo);

	connect(m_ctrl, &PlaybackController::indexLoaded, this, &PlaybackDialog::onIndexLoaded);
	connect(m_ctrl, &PlaybackController::indexLoadError, this, &PlaybackDialog::onIndexLoadError);
	connect(m_ctrl, &PlaybackController::indexBuildProgressed, [this](qreal p) { m_ui->buildIndexProgress->setValue(p * m_ui->buildIndexProgress->maximum()); });

	connect(m_ctrl, &PlaybackController::exportStarted, this, &PlaybackDialog::onVideoExportStarted);
	connect(m_ctrl, &PlaybackController::exportEnded, this, &PlaybackDialog::onVideoExportEnded);

	connect(m_ui->saveFrame, &QAbstractButton::clicked, m_ctrl, &PlaybackController::exportFrame);
	connect(m_ui->stopExport, &QAbstractButton::clicked, m_ctrl, &PlaybackController::stopExporter);
	connect(m_ui->autoSaveFrame, &QCheckBox::toggled, m_ctrl, &PlaybackController::setAutosave);
	connect(m_ctrl, &PlaybackController::exportedFrame, [this]() {
		m_ui->frameLabel->setText(QString::number(m_ctrl->currentExportFrame()));
		m_ui->timeLabel->setText(m_ctrl->currentExportTime());
	});

	connect(m_ctrl, &PlaybackController::canSaveFrameChanged, [this](bool e) {
		m_ui->play->setEnabled(e);
		m_ui->skipBackward->setEnabled(e && m_ctrl->hasIndex());
		m_ui->skipForward->setEnabled(e);
		m_ui->stepForward->setEnabled(e);
		m_ui->markers->setEnabled(e);
		m_ui->saveFrame->setEnabled(e);
	});

	// Connections for non-indexed recordings. These will be changed when/if the index is loaded
	m_ui->filmStrip->setLength(reader->filesize());
	m_ui->filmStrip->setFrames(qMax(1, int(reader->filesize() / 100000)));
	connect(m_ctrl, &PlaybackController::progressChanged, m_ui->filmStrip, &Filmstrip::setCursor);

	rebuildMarkerMenu();

	// Automatically try to load the index
	QTimer::singleShot(0, m_ctrl, &PlaybackController::loadIndex);
}

PlaybackDialog::~PlaybackDialog()
{
	delete m_ui;
}

void PlaybackDialog::onBuildIndexClicked()
{
	m_ui->noIndexReason->hide();
	m_ui->buildIndexProgress->show();
	m_ui->buildIndexButton->setEnabled(false);
	m_ctrl->buildIndex();
}

void PlaybackDialog::onIndexLoaded()
{
	disconnect(m_ctrl, &PlaybackController::progressChanged, m_ui->filmStrip, &Filmstrip::setCursor);
	connect(m_ctrl, &PlaybackController::indexPositionChanged,m_ui->filmStrip, &Filmstrip::setCursor);

	m_ui->filmStrip->setLength(m_ctrl->maxIndexPosition());
	m_ui->filmStrip->setFrames(m_ctrl->indexThumbnailCount());
	m_ui->filmStrip->setCursor(m_ctrl->indexPosition());


	m_ui->skipBackward->setEnabled(true);

	m_ui->buildIndexButton->hide();
	m_ui->buildIndexProgress->hide();
	m_ui->noIndexReason->hide();

	m_ui->filmStrip->setLoadImageFn(std::bind(&PlaybackController::getIndexThumbnail, m_ctrl, std::placeholders::_1));

	rebuildMarkerMenu();
}

void PlaybackDialog::onIndexLoadError(const QString &msg, bool canRetry)
{
	m_ui->buildIndexProgress->hide();
	m_ui->noIndexReason->setText(msg);
	m_ui->noIndexReason->show();
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
	return m_ctrl->isPlaying();
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

void PlaybackDialog::closeEvent(QCloseEvent *event)
{
	if(!exitCleanup())
		event->ignore();
	else
		QDialog::closeEvent(event);
}

void PlaybackDialog::keyPressEvent(QKeyEvent *event)
{
	// This is not a OK/Cancel type dialog, so disable
	// key events. Without this, it is easy to close
	// the window accidentally by hitting Esc.
	event->ignore();
}

void PlaybackDialog::done(int r)
{
	if(exitCleanup())
		QDialog::done(r);
}

void PlaybackDialog::rebuildMarkerMenu()
{
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
}

void PlaybackDialog::onMarkerMenuTriggered(QAction *a)
{
	QVariant idx = a->property("markeridx");
	if(idx.isValid())
		m_ctrl->jumpToMarker(idx.toInt());
}

void PlaybackDialog::onFilterRecordingClicked()
{
	dialogs::FilterRecordingDialog dlg(this);

	if(dlg.exec() == QDialog::Accepted) {
		QString filename = dlg.filterRecording(m_ctrl->recordingFilename());

		if(!filename.isEmpty()) {
			MainWindow *win = new MainWindow(false);
			win->open(QUrl::fromLocalFile(filename));
		}
	}
}

void PlaybackDialog::onVideoExportClicked()
{
	QScopedPointer<VideoExportDialog> dialog(new VideoExportDialog(this));

	VideoExporter *ve=0;
	while(!ve) {
		if(dialog->exec() != QDialog::Accepted)
			return;

		ve = dialog->getExporter();
	}

	m_ctrl->startVideoExport(ve);
}

void PlaybackDialog::onVideoExportStarted()
{
	m_ui->exportStack->setCurrentIndex(0);
}

void PlaybackDialog::onVideoExportEnded()
{
	m_ui->exportStack->setCurrentIndex(1);
}


bool PlaybackDialog::exitCleanup()
{
	if(m_ctrl->isExporting()) {
		if(!m_closing) {
			QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
			m_closing = true;
			connect(m_ctrl, &recording::PlaybackController::exportEnded, this, &QDialog::close);
			m_ctrl->stopExporter();
		}
		return false;
	} else {
		if(m_closing)
			QApplication::restoreOverrideCursor();
		return true;
	}
}

}
