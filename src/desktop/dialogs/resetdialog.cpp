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
#include "rustpile/rustpile.h"
#include "libclient/utils/icon.h"
#include "libclient/net/envelopebuilder.h"
#include "libclient/utils/images.h"

#include "ui_resetsession.h"

#include <QPushButton>
#include <QPainter>
#include <QVector>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QApplication>

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

struct ResetDialog::Private
{
	Ui_ResetDialog *ui;
	rustpile::PaintEngine *paintEngine;
	QPushButton *resetButton;
	rustpile::SnapshotQueue *snapshots;
	QVector<QPixmap> resetPoints;
	int selection;

	Private(const canvas::PaintEngine *pe)
		: ui(new Ui_ResetDialog), paintEngine(pe->engine()), selection(0)
	{
		snapshots = rustpile::paintengine_get_snapshots(pe->engine());
		resetPoints.resize(rustpile::snapshots_count(snapshots));
		selection = resetPoints.size() - 1;
	}

	~Private() {
		rustpile::paintengine_release_snapshots(snapshots);
		delete ui;
	}

	void updateSelection()
	{
		if(resetPoints.isEmpty())
			return;

		Q_ASSERT(!resetPoints.isEmpty());
		Q_ASSERT(selection >=0 && selection < resetPoints.size());

		if(resetPoints[selection].isNull()) {
			const rustpile::Size s = rustpile::snapshots_size(snapshots, selection);
			if(s.width > 0) {
				QImage img(s.width, s.height, QImage::Format_ARGB32_Premultiplied);
				img.fill(0);
				rustpile::snapshots_get_content(snapshots, selection, img.bits());
				img = img.scaled(THUMBNAIL_SIZE, Qt::KeepAspectRatio);
				drawCheckerBackground(img);
				resetPoints[selection] = QPixmap::fromImage(img);
			}
		}

		ui->preview->setPixmap(resetPoints[selection]);
	}
};

ResetDialog::ResetDialog(const canvas::PaintEngine *pe, QWidget *parent)
	: QDialog(parent), d(new Private(pe))
{
	d->ui->setupUi(this);

	d->resetButton = d->ui->buttonBox->addButton(tr("Reset Session"), QDialogButtonBox::DestructiveRole);
	auto *newButton = d->ui->buttonBox->addButton(tr("New"), QDialogButtonBox::ActionRole);
	auto *openButton = d->ui->buttonBox->addButton(tr("Open..."), QDialogButtonBox::ActionRole);

	d->resetButton->setIcon(icon::fromTheme("edit-undo"));
	connect(d->resetButton, &QPushButton::clicked, this, &ResetDialog::resetSelected);

	newButton->setIcon(icon::fromTheme("document-new"));
	connect(newButton, &QPushButton::clicked, this, &ResetDialog::newSelected);

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
	d->selection = d->resetPoints.size() - pos;
	qInfo("sliderMoved(%d) selection=%d", pos, d->selection);
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

	if(rustpile::snapshots_import_file(d->snapshots, reinterpret_cast<const uint16_t*>(file.constData()), file.length())) {
		d->resetPoints.append(QPixmap());
		d->ui->snapshotSlider->setMaximum(d->resetPoints.size());
		d->ui->snapshotSlider->setValue(0);
		d->selection = d->resetPoints.size() - 1;
		d->updateSelection();
		QApplication::restoreOverrideCursor();

	} else {
		QApplication::restoreOverrideCursor();
		QMessageBox::warning(this, tr("Reset"), tr("Couldn't open file"));
	}
}

net::Envelope ResetDialog::getResetImage() const
{
	if(d->resetPoints.isEmpty())
		return net::Envelope();

	net::EnvelopeBuilder eb;
	rustpile::paintengine_get_historical_reset_snapshot(d->paintEngine, d->snapshots, d->selection, eb);

	return eb.toEnvelope();
}

}
