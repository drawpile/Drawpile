// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/dialogs/dumpplaybackdialog.h"
#include "desktop/utils/qtguicompat.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/paintengine.h"
#include "libclient/drawdance/canvashistory.h"
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
	drawdance::CanvasHistorySnapshot chs;

	static const QString &undoToString(DP_Undo undo)
	{
		static QString undoDone{QStringLiteral("done")};
		static QString undoUndone{QStringLiteral("undone")};
		static QString undoGone{QStringLiteral("gone")};
		static QString undoUnknown{QStringLiteral("unknown")};
		switch(undo) {
		case DP_UNDO_DONE:
			return undoDone;
		case DP_UNDO_UNDONE:
			return undoUndone;
		case DP_UNDO_GONE:
			return undoGone;
		default:
			return undoUnknown;
		}
	}

	static QString affectedAreaToString(const DP_AffectedArea *aa)
	{
		switch(aa->domain) {
		case DP_AFFECTED_DOMAIN_USER_ATTRS:
			return QStringLiteral("local user");
		case DP_AFFECTED_DOMAIN_LAYER_ATTRS:
			return QStringLiteral("properties of layer %1")
				.arg(aa->affected_id);
		case DP_AFFECTED_DOMAIN_ANNOTATIONS:
			return QStringLiteral("annotation %1").arg(aa->affected_id);
		case DP_AFFECTED_DOMAIN_PIXELS: {
			DP_Rect bounds = aa->bounds;
			return QStringLiteral(
					   "pixels on layer %1, from (%2, %3) to (%4, %5)")
				.arg(aa->affected_id)
				.arg(DP_rect_left(bounds))
				.arg(DP_rect_top(bounds))
				.arg(DP_rect_right(bounds))
				.arg(DP_rect_bottom(bounds));
		}
		case DP_AFFECTED_DOMAIN_CANVAS_BACKGROUND:
			return QStringLiteral("canvas background");
		case DP_AFFECTED_DOMAIN_DOCUMENT_METADATA:
			return QStringLiteral("document metadata type %1")
				.arg(aa->affected_id);
		case DP_AFFECTED_DOMAIN_TIMELINE:
			return QStringLiteral("timeline frame %1").arg(aa->affected_id);
		case DP_AFFECTED_DOMAIN_SELECTIONS:
			return QStringLiteral("selection %1 of user %2")
				.arg(aa->affected_id & 0xff)
				.arg((aa->affected_id >> 8) & 0xff);
		case DP_AFFECTED_DOMAIN_EVERYTHING:
			return QStringLiteral("everything");
		default:
			return QStringLiteral("unknown domain %1")
				.arg(static_cast<int>(aa->domain));
		}
	}
};

DumpPlaybackDialog::DumpPlaybackDialog(
	canvas::CanvasModel *canvas, QWidget *parent)
	: QDialog(parent)
	, d{new Private{
		  {},
		  canvas->paintEngine(),
		  new QTimer{this},
		  0,
		  false,
		  false,
		  drawdance::CanvasHistorySnapshot::null()}}
{
	d->ui.setupUi(this);
	d->ui.positionSpinner->setMaximum(INT_MAX);
	d->ui.jumpSpinner->setMaximum(INT_MAX);

	d->ui.historyTable->setColumnCount(4);
	d->ui.historyTable->setHorizontalHeaderLabels(
		{QStringLiteral("Type"), QStringLiteral("User"), QStringLiteral("Undo"),
		 QStringLiteral("State")});
	d->ui.historyTable->horizontalHeader()->setStretchLastSection(true);
	d->ui.historyTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

	d->ui.forkTable->setColumnCount(3);
	d->ui.forkTable->setHorizontalHeaderLabels(
		{QStringLiteral("Type"), QStringLiteral("User"),
		 QStringLiteral("Affected Area")});
	d->ui.forkTable->horizontalHeader()->setStretchLastSection(true);
	d->ui.forkTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

	d->timer->setTimerType(Qt::PreciseTimer);
	d->timer->setSingleShot(true);

	qRegisterMetaType<drawdance::CanvasHistorySnapshot>();
	connect(
		d->paintEngine, &canvas::PaintEngine::dumpPlaybackAt, this,
		&DumpPlaybackDialog::onDumpPlaybackAt, Qt::QueuedConnection);
	connect(
		d->ui.playPauseButton, &QPushButton::clicked, this,
		&DumpPlaybackDialog::playPause);
	connect(
		d->ui.singleStepButton, &QPushButton::clicked, this,
		&DumpPlaybackDialog::singleStep);
	connect(
		d->ui.previousResetButton, &QPushButton::clicked, this,
		&DumpPlaybackDialog::jumpToPreviousReset);
	connect(
		d->ui.nextResetButton, &QPushButton::clicked, this,
		&DumpPlaybackDialog::jumpToNextReset);
	connect(
		d->ui.jumpButton, &QPushButton::clicked, this,
		&DumpPlaybackDialog::jump);
	connect(
		d->ui.hideWithoutState, COMPAT_CHECKBOX_STATE_CHANGED_SIGNAL(QCheckBox),
		[this](compat::CheckBoxState) {
			updateHistoryTable();
		});
	connect(
		d->ui.hideGoneEntries, COMPAT_CHECKBOX_STATE_CHANGED_SIGNAL(QCheckBox),
		[this](compat::CheckBoxState) {
			updateHistoryTable();
		});
	connect(d->timer, &QTimer::timeout, this, &DumpPlaybackDialog::singleStep);

	updateUi();
	updateTables();
}

DumpPlaybackDialog::~DumpPlaybackDialog()
{
	delete d;
}

void DumpPlaybackDialog::closeEvent(QCloseEvent *)
{
	d->paintEngine->closePlayback();
}

void DumpPlaybackDialog::onDumpPlaybackAt(
	long long pos, const drawdance::CanvasHistorySnapshot &chs)
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

	d->chs = chs;
	updateTables();
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

void DumpPlaybackDialog::updateTables()
{
	updateHistoryTable();
	updateForkTable();
	if(d->chs.isNull()) {
		updateStatus(0, 0, 0, 0, 0);
	} else {
		updateStatus(
			d->chs.historyCount(), d->chs.historyOffset(), d->chs.forkCount(),
			d->chs.forkStart(), d->chs.forkFallbehind());
	}
}

void DumpPlaybackDialog::updateHistoryTable()
{
	int historyCount = d->chs.isNull() ? 0 : d->chs.historyCount();
	QTableWidget *ht = d->ui.historyTable;
	if(historyCount == 0) {
		ht->setRowCount(1);
		ht->clearContents();
	} else {
		ht->setRowCount(0);
		int row = 0;
		int offset = d->chs.historyOffset();
		bool hideWithoutState = d->ui.hideWithoutState->isChecked();
		bool hideGoneEntries = d->ui.hideGoneEntries->isChecked();
		for(int i = 0; i < historyCount; ++i) {
			const DP_CanvasHistoryEntry *entry = d->chs.historyEntryAt(i);
			bool hideEntry = (hideWithoutState && !entry->state) ||
							 (hideGoneEntries && entry->undo == DP_UNDO_GONE);
			if(!hideEntry) {
				ht->insertRow(row);
				ht->setVerticalHeaderItem(
					row, new QTableWidgetItem{QString::number(offset + i)});
				ht->setItem(
					row, 0,
					new QTableWidgetItem{QString::fromUtf8(
						DP_message_type_name(DP_message_type(entry->msg)))});
				ht->setItem(
					row, 1,
					new QTableWidgetItem{
						QString::number(DP_message_context_id(entry->msg))});
				ht->setItem(
					row, 2,
					new QTableWidgetItem{Private::undoToString(entry->undo)});
				ht->setItem(
					row, 3,
					new QTableWidgetItem{
						entry->state
							? QStringLiteral("0x%1").arg(
								  reinterpret_cast<quintptr>(entry->state),
								  QT_POINTER_SIZE * 2, 16, QChar('0'))
							: QString{}});
				++row;
			}
		}
	}
}

void DumpPlaybackDialog::updateForkTable()
{
	int forkCount = d->chs.isNull() ? 0 : d->chs.forkCount();
	QTableWidget *ft = d->ui.forkTable;
	if(forkCount == 0) {
		ft->setRowCount(1);
		ft->clearContents();
	} else {
		ft->setRowCount(forkCount);
		int start = d->chs.forkStart();
		for(int i = 0; i < forkCount; ++i) {
			const DP_ForkEntry *fe = d->chs.forkEntryAt(i);
			ft->setVerticalHeaderItem(
				i, new QTableWidgetItem{QString::number(start + i)});
			ft->setItem(
				i, 0,
				new QTableWidgetItem{QString::fromUtf8(
					DP_message_type_name(DP_message_type(fe->msg)))});
			ft->setItem(
				i, 1,
				new QTableWidgetItem{
					QString::number(DP_message_context_id(fe->msg))});
			ft->setItem(
				i, 2,
				new QTableWidgetItem{Private::affectedAreaToString(&fe->aa)});
		}
	}
}

void DumpPlaybackDialog::updateStatus(
	int historyCount, int historyOffset, int forkCount, int forkStart,
	int forkFallbehind)
{
	d->ui.historyCount->setText(QString::number(historyCount));
	d->ui.historyOffset->setText(QString::number(historyOffset));
	bool forkPresent = forkCount != 0;
	d->ui.forkPresent->setText(
		forkPresent ? QStringLiteral("Yes") : QStringLiteral("No"));
	d->ui.forkCount->setText(QString::number(forkCount));
	d->ui.forkStart->setText(QString::number(forkStart));
	d->ui.forkFallbehind->setText(QString::number(forkFallbehind));
}

}
