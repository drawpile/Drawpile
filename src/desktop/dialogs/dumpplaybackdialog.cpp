/*
 * Drawpile - a collaborative drawing program.
 *
 * Copyright (C) 2023 askmeaboutloom
 *
 * Drawpile is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Drawpile is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "dumpplaybackdialog.h"
#include "canvas/canvasmodel.h"
#include "canvas/paintengine.h"
#include "ui_dumpplayback.h"
#include <QDateTime>
#include <QTimer>

namespace dialogs {

struct DumpPlaybackDialog::Private {
	Ui_DumpPlaybackDialog ui;
	canvas::PaintEngine *paintEngine;
	QTimer *timer;
	long long lastStepMs;
	bool awaiting;
	bool playing;
};

DumpPlaybackDialog::DumpPlaybackDialog(
	canvas::CanvasModel *canvas, QWidget *parent)
	: QDialog(parent)
	, d{new Private{
		  {}, canvas->paintEngine(), new QTimer{this}, 0, false, false}}
{
	d->ui.setupUi(this);
	d->ui.positionSpinner->setMaximum(INT_MAX);
	d->ui.jumpSpinner->setMaximum(INT_MAX);

	d->timer->setTimerType(Qt::PreciseTimer);
	d->timer->setSingleShot(true);

	connect(
		d->paintEngine, &canvas::PaintEngine::playbackAt, this,
		&DumpPlaybackDialog::onPlaybackAt, Qt::QueuedConnection);
	connect(
		d->ui.playPauseButton, &QPushButton::pressed, this,
		&DumpPlaybackDialog::playPause);
	connect(
		d->ui.singleStepButton, &QPushButton::pressed, this,
		&DumpPlaybackDialog::singleStep);
	connect(
		d->ui.previousResetButton, &QPushButton::pressed, this,
		&DumpPlaybackDialog::jumpToPreviousReset);
	connect(
		d->ui.nextResetButton, &QPushButton::pressed, this,
		&DumpPlaybackDialog::jumpToNextReset);
	connect(
		d->ui.jumpButton, &QPushButton::pressed, this,
		&DumpPlaybackDialog::jump);
	connect(d->timer, &QTimer::timeout, this, &DumpPlaybackDialog::singleStep);

	updateUi();
}

DumpPlaybackDialog::~DumpPlaybackDialog()
{
	delete d;
}

void DumpPlaybackDialog::closeEvent(QCloseEvent *)
{
	d->paintEngine->closePlayback();
}

void DumpPlaybackDialog::onPlaybackAt(long long pos, int)
{
	d->awaiting = false;

	if(pos < 0) {
		d->playing = false;
	} else {
		d->ui.positionSpinner->setValue(pos);
	}

	if(d->playing) {
		long long now = QDateTime::currentMSecsSinceEpoch();
		long long delay = d->ui.delaySpinner->value();
		long long next = d->lastStepMs + delay;
		if(next > now) {
			d->timer->start(next - now);
			updateUi();
		} else {
			singleStep();
		}
	} else {
		updateUi();
	}
}

void DumpPlaybackDialog::playPause()
{
	if(d->playing) {
		d->playing = false;
		d->timer->stop();
	} else {
		d->playing = true;
		singleStep();
	}
	updateUi();
}

void DumpPlaybackDialog::singleStep()
{
	if(!d->awaiting) {
		d->awaiting = true;
		d->paintEngine->stepDumpPlayback();
		d->lastStepMs = QDateTime::currentMSecsSinceEpoch();
		updateUi();
	}
}

void DumpPlaybackDialog::jumpToPreviousReset()
{
	if(!d->awaiting) {
		d->awaiting = true;
		d->paintEngine->jumpDumpPlaybackToPreviousReset();
		updateUi();
	}
}

void DumpPlaybackDialog::jumpToNextReset()
{
	if(!d->awaiting) {
		d->awaiting = true;
		d->paintEngine->jumpDumpPlaybackToNextReset();
		updateUi();
	}
}

void DumpPlaybackDialog::jump()
{
	if(!d->awaiting && !d->playing) {
		d->awaiting = true;
		d->paintEngine->jumpDumpPlayback(d->ui.jumpSpinner->value());
		updateUi();
	}
}

void DumpPlaybackDialog::updateUi()
{
	d->ui.previousResetButton->setEnabled(!d->awaiting && !d->playing);
	if(d->playing) {
		d->ui.playPauseButton->setDown(true);
		d->ui.playPauseButton->setEnabled(true);
	} else {
		d->ui.playPauseButton->setDown(false);
		d->ui.playPauseButton->setEnabled(!d->awaiting);
	}
	d->ui.singleStepButton->setEnabled(!d->awaiting && !d->playing);
	d->ui.nextResetButton->setEnabled(!d->awaiting && !d->playing);
	d->ui.jumpButton->setEnabled(!d->awaiting && !d->playing);
}

}
