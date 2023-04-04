/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2016-2021 Calle Laakkonen

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

#include "desktop/dialogs/resetdialog.h"
#include "libclient/canvas/paintengine.h"
#include "libclient/utils/icon.h"
#include "libclient/utils/images.h"
#include "libshared/util/qtcompat.h"

#include "ui_resetsession.h"

#include <QScopedPointer>
#include <QPushButton>
#include <QPainter>
#include <QVector>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QApplication>

namespace {

struct ResetPoint {
	drawdance::CanvasState canvasState;
	QPixmap thumbnail;
};

QVector<ResetPoint> makeResetPoints(const canvas::PaintEngine *pe)
{
	QVector<ResetPoint> resetPoints;
	pe->snapshotQueue().getSnapshotsWith([&](size_t count, drawdance::SnapshotQueue::SnapshotAtFn at) {
		resetPoints.reserve(compat::castSize(count + 1));
		for (size_t i = 0; i < count; ++i) {
			DP_Snapshot *s = at(i);
			resetPoints.append(ResetPoint{
				drawdance::CanvasState::inc(DP_snapshot_canvas_state_noinc(s)),
				QPixmap{},
			});
		}
	});

	drawdance::CanvasState currentCanvasState = pe->historyCanvasState();
	int lastIndex = resetPoints.count() - 1;
	// Don't repeat last reset point if the canvas state hasn't changed since.
	if(lastIndex < 0 || currentCanvasState.get() != resetPoints[lastIndex].canvasState.get()) {
		resetPoints.append(ResetPoint{
			currentCanvasState,
			QPixmap{},
		});
	}

	return resetPoints;
}

}

namespace dialogs {

struct ResetDialog::Private {
	QScopedPointer<Ui_ResetDialog> ui;
	const canvas::PaintEngine *paintEngine;
	QPushButton *resetButton;
	QVector<ResetPoint> resetPoints;
	compat::sizetype selection;

	Private(const canvas::PaintEngine *pe)
		: ui{new Ui_ResetDialog}
		, paintEngine{pe}
		, resetPoints{makeResetPoints(pe)}
		, selection{resetPoints.size() - 1}
	{
	}

	~Private() = default;

	static void drawCheckerBackground(QImage &image)
	{
		const int TS = 16;
		const QBrush checker[] = {
			QColor(128,128,128),
			QColor(Qt::white)
		};
		QPainter painter(&image);
		painter.setCompositionMode(QPainter::CompositionMode_DestinationOver);
		for(int y=0;y<image.height();y+=TS) {
			for(int x=0;x<image.width();x+=TS*2) {
				const int z = (y/TS+x) % 2 == 0;
				painter.fillRect(x, y, TS, TS, checker[z]);
				painter.fillRect(x+TS, y, TS, TS, checker[1-z]);
			}
		}
	}

	void updateSelection()
	{
		Q_ASSERT(!resetPoints.isEmpty());
		Q_ASSERT(selection >= 0 && selection < resetPoints.size());

		ResetPoint &rp = resetPoints[selection];
		if(rp.thumbnail.isNull()) {
			const drawdance::CanvasState &canvasState = rp.canvasState;
			QImage img = canvasState.toFlatImage();
			if(!img.isNull()) {
				img = img.scaled(256, 256, Qt::KeepAspectRatio);
				drawCheckerBackground(img);
				rp.thumbnail = QPixmap::fromImage(img);
			}
		}

		ui->preview->setPixmap(rp.thumbnail);
	}
};

ResetDialog::ResetDialog(const canvas::PaintEngine *pe, QWidget *parent)
	: QDialog(parent), d(new Private(pe))
{
	d->ui->setupUi(this);

	d->resetButton = d->ui->buttonBox->addButton(tr("Reset Session"), QDialogButtonBox::DestructiveRole);
	d->resetButton->setIcon(icon::fromTheme("edit-undo"));
	connect(d->resetButton, &QPushButton::clicked, this, &ResetDialog::resetSelected);

#ifndef SINGLE_MAIN_WINDOW
	// If we can't open a new window, this would obliterate the current session.
	// That's confusing and not terribly useful, so we don't offer this option.
	QPushButton *newButton = d->ui->buttonBox->addButton(tr("New"), QDialogButtonBox::ActionRole);
	newButton->setIcon(icon::fromTheme("document-new"));
	connect(newButton, &QPushButton::clicked, this, &ResetDialog::newSelected);
#endif

	QPushButton *openButton = d->ui->buttonBox->addButton(tr("Open..."), QDialogButtonBox::ActionRole);
	openButton->setIcon(icon::fromTheme("document-open"));
	connect(openButton, &QPushButton::clicked, this, &ResetDialog::onOpenClicked);

	d->ui->snapshotSlider->setMaximum(d->resetPoints.size());
	connect(d->ui->snapshotSlider, &QSlider::valueChanged, this, &ResetDialog::onSelectionChanged);

	d->updateSelection();
}

ResetDialog::~ResetDialog()
{
	delete d;
}

void ResetDialog::setCanReset(bool canReset)
{
	d->resetButton->setEnabled(canReset);
}

void ResetDialog::onSelectionChanged(int pos)
{
	int count = d->resetPoints.count();
	d->selection = qBound(0, count - pos, count - 1);
	d->updateSelection();
}

void ResetDialog::onOpenClicked()
{
	const QString file = QFileDialog::getOpenFileName(
		this,
		tr("Open Image"),
		QSettings().value("window/lastpath").toString(),
		utils::fileFormatFilter(utils::FileFormatOption::OpenImages)
	);

	if(file.isEmpty())
		return;

	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

	drawdance::CanvasState canvasState = drawdance::CanvasState::load(file);
	QApplication::restoreOverrideCursor();

	if(canvasState.isNull()) {
		QMessageBox::warning(this, tr("Reset"), tr("Couldn't open file"));
	} else {
		d->resetPoints.append({canvasState, QPixmap{}});
		d->ui->snapshotSlider->setMaximum(d->resetPoints.size());
		d->ui->snapshotSlider->setValue(0);
		d->selection = d->resetPoints.size() - 1;
		d->updateSelection();
	}
}

drawdance::MessageList ResetDialog::getResetImage() const
{
	drawdance::MessageList resetImage;
	d->resetPoints[d->selection].canvasState.toResetImage(resetImage, 0);
	return resetImage;
}

}
