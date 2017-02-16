/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2016 Calle Laakkonen

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
#include "core/layerstack.h"

#include "ui_resetsession.h"

#include <QToolButton>
#include <QList>
#include <QDateTime>

namespace dialogs {

static const QSize THUMBNAIL_SIZE { 256, 256 };

struct ResetDialog::Private
{
	Ui_ResetDialog *ui;
	QList<canvas::StateSavepoint> savepoints;
	QList<QPixmap> thumbnails;
	int selection;
	qint64 zerotime;

	Private(const QList<canvas::StateSavepoint> &sp)
		: ui(new Ui_ResetDialog), savepoints(sp), selection(0)
	{
		zerotime = QDateTime::currentMSecsSinceEpoch();
	}
	~Private() { delete ui; }

	void updateSelectionTitle()
	{
		QString title;
		if(selection == 0) {
			title = QApplication::tr("Current");
		} else {
			Q_ASSERT(selection<0);
			const qint64 age = zerotime - savepoints.at(savepoints.size() + selection).timestamp();
			title = QApplication::tr("%1 s. ago").arg(age / 1000);
		}
		ui->btnNext->setEnabled(selection < 0);
		ui->btnPrev->setEnabled(selection > -savepoints.size());

		ui->current->setText(title);

		while(thumbnails.size() <= -selection)
			thumbnails.append(QPixmap());
		if(thumbnails.at(-selection).isNull())
			thumbnails[-selection] = QPixmap::fromImage(savepoints[savepoints.size() + selection].thumbnail(THUMBNAIL_SIZE));

		ui->preview->setPixmap(thumbnails.at(-selection));
	}
};

ResetDialog::ResetDialog(const canvas::StateTracker *state, QWidget *parent)
	: QDialog(parent), d(new Private(state->getSavepoints()))
{
	d->ui->setupUi(this);
	connect(d->ui->btnPrev, &QToolButton::clicked, this, &ResetDialog::onPrevClick);
	connect(d->ui->btnNext, &QToolButton::clicked, this, &ResetDialog::onNextClick);

	QImage currentImage = state->image()->toFlatImage(true);
	if(currentImage.width() > THUMBNAIL_SIZE.width() || currentImage.height() > THUMBNAIL_SIZE.height())
		currentImage = currentImage.scaled(THUMBNAIL_SIZE, Qt::KeepAspectRatio);
	d->thumbnails.append(QPixmap::fromImage(currentImage));

	d->updateSelectionTitle();
}

ResetDialog::~ResetDialog()
{
	delete d;
}

void ResetDialog::onPrevClick()
{
	if(d->selection > -d->savepoints.size())
		d->selection--;
	d->updateSelectionTitle();
}

void ResetDialog::onNextClick()
{
	if(d->selection < 0)
		d->selection++;
	d->updateSelectionTitle();
}

canvas::StateSavepoint ResetDialog::selectedSavepoint() const
{
	if(d->selection == 0)
		return canvas::StateSavepoint();
	else
		return d->savepoints.at(d->savepoints.size() + d->selection);
}

}
