/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2014 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include <QPushButton>
#include <QFileDialog>
#include <QImageReader>
#include <QSettings>

#include "loader.h"

#include "dialogs/hostdialog.h"

#include "widgets/imageselector.h"
#include "widgets/colorbutton.h"
#include "utils/mandatoryfields.h"

using widgets::ImageSelector;
using widgets::ColorButton;

#include <QSettings>

#include "ui_hostdialog.h"

namespace dialogs {

HostDialog::HostDialog(const QImage& original, QWidget *parent)
	: QDialog(parent)
{
	ui_ = new Ui_HostDialog;
	ui_->setupUi(this);
	ui_->buttons->button(QDialogButtonBox::Ok)->setText(tr("Host"));
	ui_->buttons->button(QDialogButtonBox::Ok)->setDefault(true);

	if(original.isNull()) {
		ui_->imageSelector->setWidth(800);
		ui_->imageSelector->setHeight(600);
		ui_->existingpicture->setEnabled(false);
		ui_->imageSelector->chooseColor();
		ui_->solidcolor->setChecked(true);
	} else {
		ui_->imageSelector->setOriginal(original);
	}

	connect(ui_->colorButton, SIGNAL(colorChanged(QColor)), ui_->imageSelector, SLOT(setColor(QColor)));
	connect(ui_->colorButton, SIGNAL(colorChanged(QColor)), ui_->solidcolor, SLOT(click()));

	connect(ui_->selectPicture, SIGNAL(clicked()), this, SLOT(selectPicture()));
	connect(ui_->imageSelector, SIGNAL(noImageSet()), this, SLOT(newSelected()));

	// Set defaults
	QSettings cfg;
	cfg.beginGroup("history");
	ui_->username->setText(cfg.value("username").toString());
	ui_->sessiontitle->setText(cfg.value("sessiontitle").toString());
	ui_->remotehost->insertItems(0, cfg.value("recentremotehosts").toStringList());

	new MandatoryFields(this, ui_->buttons->button(QDialogButtonBox::Ok));
}

HostDialog::~HostDialog()
{
	delete ui_;
}

void HostDialog::rememberSettings() const
{
	QSettings cfg;
	cfg.beginGroup("history");
	cfg.setValue("username", getUserName());
	cfg.setValue("sessiontitle", getTitle());
	QStringList hosts;
	// Move current address to the top of the list
	const QString current = ui_->remotehost->currentText();
	int curind = ui_->remotehost->findText(current);
	if(curind!=-1)
		ui_->remotehost->removeItem(curind);
	hosts << current;
	for(int i=0;i<ui_->remotehost->count();++i)
			hosts << ui_->remotehost->itemText(i);
	cfg.setValue("recentremotehosts", hosts);

}

bool HostDialog::selectPicture()
{
	// TODO support openraster

	// Get a list of supported formats
	QString formats;
	foreach(QByteArray format, QImageReader::supportedImageFormats()) {
			formats += "*." + format + " ";
	}
	const QString filter = tr("Images (%1);;All files (*)").arg(formats);

	// Get the file name to open
	QSettings cfg;

	const QString file = QFileDialog::getOpenFileName(this,
					tr("Open image"),
					cfg.value("window/lastpath").toString(),
					filter
	);

	bool selected = false;
	if(!file.isEmpty()) {
		// Open the file
		QImage img(file);
		if(!img.isNull()) {
			ui_->imageSelector->setImage(img);
			ui_->otherpicture->click();
			selected = true;
		}

		cfg.setValue("window/lastpath", file);
	}
	return selected;
}

QString HostDialog::getRemoteAddress() const
{
	return ui_->remotehost->currentText();
}

bool HostDialog::useRemoteAddress() const
{
	return ui_->useremote->isChecked();
}

QString HostDialog::getUserName() const
{
	return ui_->username->text();
}

QString HostDialog::getTitle() const
{
	return ui_->sessiontitle->text();
}

int HostDialog::getUserLimit() const
{
	return ui_->userlimit->value();
}

QString HostDialog::getPassword() const
{
	return ui_->sessionpassword->text();
}

SessionLoader *HostDialog::getSessionLoader() const
{
	if(ui_->imageSelector->isColor()) {
		return new BlankCanvasLoader(
			QSize(
				ui_->picturewidth->value(),
				ui_->pictureheight->value()
			),
			ui_->imageSelector->color()
		);
	} else {
		return new QImageCanvasLoader(ui_->imageSelector->image());
	}
}

bool HostDialog::useOriginalImage() const
{
	return ui_->imageSelector->isOriginal();
}

void HostDialog::newSelected()
{
	if(!selectPicture()) {
		// Select something else if no image was selected
		if(ui_->existingpicture->isEnabled())
			ui_->existingpicture->click();
		else
			ui_->solidcolor->click();
	}
}

bool HostDialog::getAllowDrawing() const
{
	return ui_->allowdrawing->isChecked();
}

bool HostDialog::getLayerControlLock() const
{
	return ui_->layerctrllock->isChecked();
}

}
