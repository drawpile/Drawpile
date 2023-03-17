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

#include "desktop/dialogs/playbackdialog.h"
#include "desktop/dialogs/videoexportdialog.h"

#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/paintengine.h"
#include "libclient/canvas/indexbuilderrunnable.h"

#include "libclient/export/videoexporter.h"

#include "desktop/mainwindow.h"

#include "ui_playback.h"

#include <QDebug>
#include <QInputDialog>
#include <QMessageBox>
#include <QCloseEvent>
#include <QTimer>
#include <QThreadPool>

namespace dialogs {

PlaybackDialog::PlaybackDialog(canvas::CanvasModel *canvas, QWidget *parent) :
	QDialog(parent), m_ui(new Ui_PlaybackDialog),
	m_paintengine(canvas->paintEngine()),
	m_index(nullptr),
	m_speedFactor(1.0),
	m_intervalAfterExport(0),
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

	// Step timer is used to limit playback speed
	m_autoStepTimer = new QTimer(this);
	m_autoStepTimer->setSingleShot(true);
	connect(m_autoStepTimer, &QTimer::timeout, this, &PlaybackDialog::stepNext);

	connect(m_ui->speedcontrol, &QAbstractSlider::valueChanged, this, [this](int speed) {
		qreal s;
		if(speed<=100)
			s = qMax(1, speed) / 100.0;
		else
			s = 1.0 + ((speed-100) / 100.0) * 8.0;

		m_speedFactor = 1.0 / s;
		m_ui->speedLabel->setText(QString("x %1").arg(s, 0, 'f', 1));
	});

	// The paint engine's playback callback lets us know when the step/sequence
	// has been rendered and we're free to take another one.
	connect(canvas->paintEngine(), &canvas::PaintEngine::playbackAt, this, &PlaybackDialog::onPlaybackAt, Qt::QueuedConnection);

	// |<< button skips backwards. This needs an index to work
	connect(m_ui->skipBackward, &QAbstractButton::clicked, this, [this]() {
		if(!m_awaiting) {
			m_awaiting = true;
			if(m_paintengine->skipPlaybackBy(-1) != DP_PLAYER_SUCCESS) {
				qWarning("Error skipping backward: %s", DP_error());
			}
		}
	});

	// >>| button skips forward a whole stroke
	connect(m_ui->skipForward, &QAbstractButton::clicked, this, [this]() {
		if(!m_awaiting) {
			m_awaiting = true;
			if(m_paintengine->skipPlaybackBy(1) != DP_PLAYER_SUCCESS) {
				qWarning("Error skipping forward: %s", DP_error());
			}
		}
	});

	// >> button steps forward a single message
	connect(m_ui->stepForward, &QAbstractButton::clicked, this, &PlaybackDialog::stepNext);

	// play button toggles automatic step mode
	connect(m_ui->play, &QAbstractButton::toggled, this, &PlaybackDialog::setPlaying);

	// jump to a position in the recording from the film strip
	connect(m_ui->filmStrip, &widgets::Filmstrip::doubleClicked, this, &PlaybackDialog::jumpTo);

	loadIndex();
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
void PlaybackDialog::onPlaybackAt(long long pos, int interval)
{
	m_awaiting = false;
	if(pos < 0) {
		// Negative number means we've reached the end of the recording
		setPlaying(false);
		m_ui->play->setEnabled(false);
		m_ui->skipForward->setEnabled(false);
		m_ui->stepForward->setEnabled(false);

		m_ui->playbackProgress->setValue(m_ui->playbackProgress->maximum());
		m_ui->filmStrip->setCursor(m_ui->filmStrip->length());

	} else {
		m_ui->play->setEnabled(true);
		m_ui->skipForward->setEnabled(true);
		m_ui->stepForward->setEnabled(true);

		m_ui->playbackProgress->setValue(pos);
		m_ui->filmStrip->setCursor(pos);
	}

	if(pos>=0 && m_exporter && m_ui->autoSaveFrame->isChecked()) {
		const int writeFrames = qBound(1, int(interval / 1000.0 * m_exporter->fps()), m_exporter->fps());
		m_intervalAfterExport = interval;
		exportFrame(writeFrames);

		// Note: autoStepNext will be called  when the exporter becomes ready again.

	} else if(pos>=0 && m_autoplay) {
		autoStepNext(interval);
	}
}

void PlaybackDialog::autoStepNext(int interval)
{
	if(interval > 0) {
		const auto elapsed = m_lastInterval.elapsed();
		m_lastInterval.restart();
		m_autoStepTimer->start(qMax(0.0f, qMin(interval, 5000) * m_speedFactor - elapsed));
	} else {
		m_autoStepTimer->start(33.0 * m_speedFactor);
	}
}

void PlaybackDialog::stepNext()
{
	if(!m_awaiting) {
		m_awaiting = true;
		if(m_paintengine->stepPlayback(1) != DP_PLAYER_SUCCESS) {
			qWarning("Error stepping next: %s", DP_error());
		}
	}
}

void PlaybackDialog::jumpTo(int pos)
{
	if(!m_awaiting) {
		m_awaiting = true;
		if(m_paintengine->jumpPlaybackTo(pos) != DP_PLAYER_SUCCESS) {
			qWarning("Error jumping to %d: %s", pos, DP_error());
		}
	}
}

void PlaybackDialog::onBuildIndexClicked()
{
	m_ui->noIndexReason->setText(tr("Building index..."));
	m_ui->buildIndexProgress->show();
	m_ui->buildIndexButton->setEnabled(false);

	canvas::IndexBuilderRunnable *indexer = new canvas::IndexBuilderRunnable(m_paintengine);
	connect(indexer, &canvas::IndexBuilderRunnable::progress, m_ui->buildIndexProgress, &QProgressBar::setValue);
	connect(indexer, &canvas::IndexBuilderRunnable::indexingComplete, this, [this](bool success, QString error) {
		m_ui->buildIndexProgress->hide();
		if(success) {
			loadIndex();
		} else {
			qWarning("Error building index: %s", qUtf8Printable(error));
			m_ui->noIndexReason->setText(tr("Index building failed."));
		}
	});

	QThreadPool::globalInstance()->start(indexer);
}

void PlaybackDialog::loadIndex()
{
	if(!m_paintengine->loadPlaybackIndex()) {
		qWarning("Error loading index: %s", DP_error());
		return;
	}

	m_ui->filmStrip->setLength(m_paintengine->playbackIndexMessageCount());
	m_ui->filmStrip->setFrames(m_paintengine->playbackIndexEntryCount());

	m_ui->skipBackward->setEnabled(true);

	m_ui->buildIndexButton->hide();
	m_ui->buildIndexProgress->hide();
	m_ui->noIndexReason->hide();

	m_ui->filmStrip->setLoadImageFn([this](int frame) {
		return m_paintengine->playbackIndexThumbnailAt(frame);
	});

	m_ui->indexStack->setCurrentIndex(1);
}

void PlaybackDialog::centerOnParent()
{
	if(parentWidget()) {
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
		if(m_paintengine->stepPlayback(1)) {
			qWarning("Error stepping next: %s", DP_error());
		}
	}
}

void PlaybackDialog::closeEvent(QCloseEvent *event)
{
	m_paintengine->closePlayback();

	if(m_exporter) {
		// Exporter still working? Disown it and let it finish.
		// It will delete itself once done.
		m_exporter->setParent(nullptr);
		m_exporter->finish();
	}

	QDialog::closeEvent(event);
}

void PlaybackDialog::keyPressEvent(QKeyEvent *event)
{
	// This is not a OK/Cancel type dialog, so disable
	// key events. Without this, it is easy to close
	// the window accidentally by hitting Esc.
	event->ignore();
}

void PlaybackDialog::onVideoExportClicked()
{

	QScopedPointer<VideoExportDialog> dialog(new VideoExportDialog(this));
	VideoExporter *ve=nullptr;

	// Loop until the user has selected a valid exporter
	// configuration or cancelled.
	while(!ve) {
		if(dialog->exec() != QDialog::Accepted)
			return;

		ve = dialog->getExporter();
	}

	m_exporter = ve;
	m_exporter->setParent(this);
	m_exporter->start();

	m_ui->exportStack->setCurrentIndex(0);
	m_ui->saveFrame->setEnabled(true);
	connect(m_exporter, &VideoExporter::exporterFinished, this, [this]() {
		m_ui->exportStack->setCurrentIndex(1);
	});
	connect(m_exporter, &VideoExporter::exporterFinished, m_exporter, &VideoExporter::deleteLater);

	connect(m_exporter, &VideoExporter::exporterError, this, [this](const QString &msg) {
		QMessageBox::warning(this, tr("Video error"), msg);
	});

	connect(m_ui->saveFrame, &QAbstractButton::clicked, this, &PlaybackDialog::exportFrame);
	connect(m_ui->stopExport, &QAbstractButton::clicked, m_exporter, &VideoExporter::finish);

	connect(m_exporter, &VideoExporter::exporterReady, this, [this]() {
		m_ui->saveFrame->setEnabled(true);
		m_ui->frameLabel->setText(QString::number(m_exporter->frame()));

		// When exporter is active, autoplay step isn't taken immediately when
		// the sequence point is received, because exporting is/can be asynchronous.
		// We must wait for the exporter to become ready again until taking the next step.
		if(m_autoplay)
			autoStepNext(m_intervalAfterExport);
	});
}

void PlaybackDialog::exportFrame(int count)
{
	Q_ASSERT(m_exporter);
	count = qMax(1, count);

	const QImage image = m_paintengine->getPixmap().toImage();
	if(image.isNull()) {
		qWarning("exportFrame: image is null!");
		return;
	}

	m_ui->saveFrame->setEnabled(false);
	m_exporter->saveFrame(image, count);
}

}
