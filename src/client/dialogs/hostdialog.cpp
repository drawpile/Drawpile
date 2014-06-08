/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2014 Calle Laakkonen

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

	QSize lastsize = cfg.value("newsize", QSize(800, 600)).toSize();
	ui_->picturewidth->setValue(lastsize.width());
	ui_->pictureheight->setValue(lastsize.height());

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

	// Move current address to the top of the list
	QStringList hosts;
	const QString current = ui_->remotehost->currentText();
	int curind = ui_->remotehost->findText(current);
	if(curind!=-1)
		ui_->remotehost->removeItem(curind);
	hosts << current;
	for(int i=0;i<ui_->remotehost->count();++i)
			hosts << ui_->remotehost->itemText(i);
	cfg.setValue("recentremotehosts", hosts);

	// Remember "newsize" if we created a new picture
	if(ui_->solidcolor->isChecked()) {
		cfg.setValue("newsize", QSize(ui_->picturewidth->value(), ui_->pictureheight->value()));
	}
}

bool HostDialog::selectPicture()
{
	// Get a list of supported formats
	QString formats = "*.ora ";
	foreach(QByteArray format, QImageReader::supportedImageFormats()) {
			formats += "*." + format + " ";
	}
	const QString filter = tr("Images (%1)") + ";;" + QApplication::tr("All files (*)").arg(formats);

	// Get the file name to open
	QSettings cfg;

	const QString file = QFileDialog::getOpenFileName(this,
					tr("Open image"),
					cfg.value("window/lastpath").toString(),
					filter
	);

	bool selected = false;
	if(!file.isEmpty()) {
		ui_->imageSelector->setImage(file);
		if(ui_->imageSelector->imageFile() == file) {
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

	} else if(ui_->imageSelector->isImageFile()) {
		return new ImageCanvasLoader(ui_->imageSelector->imageFile());

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

bool HostDialog::getPersistentMode() const
{
	return ui_->persistentSession->isChecked();
}

}
