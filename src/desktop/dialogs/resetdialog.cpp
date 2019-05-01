/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2016-2019 Calle Laakkonen

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

#include "resetdialog.h"
#include "canvas/statetracker.h"
#include "canvas/loader.h"
#include "core/layerstack.h"
#include "utils/images.h"

#include "ui_resetsession.h"

#include <QToolButton>
#include <QList>
#include <QDateTime>
#include <QPainter>
#include <QFileDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QVector>

namespace dialogs {

static const QSize THUMBNAIL_SIZE { 256, 256 };

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

struct ResetPoint {
	// One of these contains the reset image data:
	canvas::StateSavepoint savepoint;
	protocol::MessageList image;

	QPixmap thumbnail;
	QString title;
};

struct ResetDialog::Private
{
	Ui_ResetDialog *ui;
	QVector<ResetPoint> resetPoints;
	int selection;

	Private(const QList<canvas::StateSavepoint> &savepoints)
		: ui(new Ui_ResetDialog), selection(0)
	{
		const auto zerotime = QDateTime::currentMSecsSinceEpoch();

		resetPoints.reserve(savepoints.size() + 1);
		for(const auto &sp : savepoints) {
			const auto age = zerotime - sp.timestamp();

			resetPoints << ResetPoint {
				sp,
				protocol::MessageList(),
				QPixmap(),
				QApplication::tr("%1 s. ago").arg(age / 1000)
			};
		}
	}

	~Private() { delete ui; }

	void updateSelectionTitle()
	{
		Q_ASSERT(!resetPoints.isEmpty());
		Q_ASSERT(selection >=0 && selection < resetPoints.size());

		ui->btnNext->setEnabled(selection < (resetPoints.size()-1));
		ui->btnPrev->setEnabled(selection > 0);

		ResetPoint &rp = resetPoints[selection];
		ui->current->setText(rp.title);

		// Load thumbnail on-demand
		if(rp.thumbnail.isNull()) {
			QImage thumb = rp.savepoint.thumbnail(THUMBNAIL_SIZE);

			if(thumb.isNull()) {
				thumb = QImage(32, 32, QImage::Format_ARGB32_Premultiplied);
				thumb.fill(0);
			}
			drawCheckerBackground(thumb);
			rp.thumbnail = QPixmap::fromImage(thumb);
		}

		ui->preview->setPixmap(rp.thumbnail);
	}
};

ResetDialog::ResetDialog(const canvas::StateTracker *state, QWidget *parent)
	: QDialog(parent), d(new Private(state->getSavepoints()))
{
	d->ui->setupUi(this);

	QPushButton *openButton = d->ui->buttonBox->addButton(tr("Open..."), QDialogButtonBox::ActionRole);

	connect(openButton, &QPushButton::clicked, this, &ResetDialog::onOpenClick);
	connect(d->ui->btnPrev, &QToolButton::clicked, this, &ResetDialog::onPrevClick);
	connect(d->ui->btnNext, &QToolButton::clicked, this, &ResetDialog::onNextClick);

	QImage currentImage = state->image()->toFlatImage(true, true);
	if(currentImage.width() > THUMBNAIL_SIZE.width() || currentImage.height() > THUMBNAIL_SIZE.height())
		currentImage = currentImage.scaled(THUMBNAIL_SIZE, Qt::KeepAspectRatio);
	drawCheckerBackground(currentImage);

	d->resetPoints.append(ResetPoint {
		canvas::StateSavepoint(), // when both of these are blank, and empty message list
		protocol::MessageList(),  // will be returned and the current canvas snapshotted
		QPixmap::fromImage(currentImage),
		tr("Current")
	});
	d->selection = d->resetPoints.size() - 1;

	d->updateSelectionTitle();
}

ResetDialog::~ResetDialog()
{
	delete d;
}

void ResetDialog::onOpenClick()
{
	QSettings cfg;
	cfg.beginGroup("window");

	const QString file = QFileDialog::getOpenFileName(
		this,
		tr("Reset to Image"),
		cfg.value("lastpath").toString(),
		utils::fileFormatFilter(utils::FileFormatOption::OpenImages)
	);

	if(file.isEmpty())
		return;

	const QFileInfo info(file);
	cfg.setValue("lastpath", info.absolutePath());

	canvas::ImageCanvasLoader loader(file);

	const auto initCommands = loader.loadInitCommands();
	if(initCommands.isEmpty()) {
		QMessageBox::warning(this, tr("Reset to Image"), loader.errorMessage());
		return;
	}

	d->resetPoints << ResetPoint {
		canvas::StateSavepoint(),
		initCommands,
		loader.loadThumbnail(THUMBNAIL_SIZE),
		info.fileName()
	};
	d->selection = d->resetPoints.size() - 1;
	d->updateSelectionTitle();
}

void ResetDialog::onPrevClick()
{
	if(d->selection>0) {
		d->selection--;
		d->updateSelectionTitle();
	}
}

void ResetDialog::onNextClick()
{
	if(d->selection < (d->resetPoints.size()-1)) {
		d->selection++;
		d->updateSelectionTitle();
	}
}

protocol::MessageList ResetDialog::resetImage(int myId, const canvas::CanvasModel *canvas)
{
	const ResetPoint &rp = d->resetPoints.at(d->selection);

	if(rp.savepoint)
		return rp.savepoint.initCommands(myId, canvas);
	else
		return rp.image;
}

}
