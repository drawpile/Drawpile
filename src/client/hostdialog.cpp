/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2007 Calle Laakkonen

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
#include <QColorDialog>
#include <QFileDialog>
#include <QImageReader>

#include "hostdialog.h"
#include "imageselector.h"
#include "mandatoryfields.h"
using widgets::ImageSelector;

#include <QSettings>

#include "ui_hostdialog.h"

#include "../shared/protocol.defaults.h"

namespace dialogs {

HostDialog::HostDialog(const QImage& original, QWidget *parent)
	: QDialog(parent)
{
	ui_ = new Ui_HostDialog;
	ui_->setupUi(this);
	ui_->buttons->button(QDialogButtonBox::Ok)->setText(tr("Host"));
	ui_->buttons->button(QDialogButtonBox::Ok)->setDefault(true);
	ui_->username->setProperty("mandatoryfield", true);

	if(original.isNull()) {
		ui_->imageSelector->setWidth(800);
		ui_->imageSelector->setHeight(600);
		ui_->imageSelector->chooseColor();
		ui_->existingpicture->setEnabled(false);
	} else {
		ui_->imageSelector->setOriginal(original);
	}
	connect(ui_->selectColor, SIGNAL(clicked()), this, SLOT(selectColor()));
	connect(ui_->selectPicture, SIGNAL(clicked()), this, SLOT(selectPicture()));
	connect(ui_->imageSelector, SIGNAL(noImageSet()), this, SLOT(newSelected()));

	// Set defaults
	ui_->port->setValue(protocol::default_port);
	QSettings cfg;
	cfg.beginGroup("history");
	ui_->username->setText(cfg.value("username").toString());
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
	QStringList hosts;
	// Move current address to the top of the list
	QString current = ui_->remotehost->currentText();
	ui_->remotehost->removeItem(ui_->remotehost->findText(current));
	hosts << current;
	for(int i=0;i<ui_->remotehost->count();++i)
			hosts << ui_->remotehost->itemText(i);
	cfg.setValue("recentremotehosts", hosts);

}

void HostDialog::selectColor()
{
	QColor oldcolor = ui_->imageSelector->color();
	QColor col = QColorDialog::getColor(oldcolor, this);
	if(col.isValid() && col != oldcolor) {
		ui_->imageSelector->setColor(col);
		ui_->solidcolor->click();
	}
}

void HostDialog::selectPicture()
{
	 // Get a list of supported formats
	QString formats;
	foreach(QByteArray format, QImageReader::supportedImageFormats()) {
			formats += "*." + format + " ";
	}
	QString filter = tr("Images (%1);;All files (*)").arg(formats);

	// Get the file name to open
	QString file = QFileDialog::getOpenFileName(this,
					tr("Open image"), prevpath_, filter);

	if(file.isEmpty()==false) {
		// Open the file
		QImage img(file);
		if(img.isNull()==false) {
			ui_->imageSelector->setImage(img);
			ui_->otherpicture->click();
		}
		prevpath_ = file;
	}
}

QString HostDialog::getRemoteAddress() const
{
	return ui_->remotehost->currentText();
}

bool HostDialog::useRemoteAddress() const
{
	return ui_->useremote->isChecked();
}

int HostDialog::getPort() const
{
	return ui_->port->value();
}

bool HostDialog::isDefaultPort() const
{
	return ui_->port->value() == protocol::default_port;
}

QString HostDialog::getUserName() const
{
	return ui_->username->text();
}

QString HostDialog::getTitle() const
{
	return ui_->sessiontitle->text();
}

QString HostDialog::getAdminPassword() const
{
	return ui_->adminpassword->text();
}

int HostDialog::getUserLimit() const
{
	return ui_->userlimit->value();
}

QString HostDialog::getPassword() const
{
	return ui_->sessionpassword->text();
}

QImage HostDialog::getImage() const
{
	return ui_->imageSelector->image();
}

bool HostDialog::useOriginalImage() const
{
	return ui_->imageSelector->isOriginal();
}

void HostDialog::newSelected()
{
	selectPicture();
	if(ui_->imageSelector->image().isNull()) {
		ui_->existingpicture->setChecked(true);
		ui_->imageSelector->chooseOriginal();
	}
}

bool HostDialog::getAllowDrawing() const
{
	return ui_->allowdrawing->isChecked();
}

bool HostDialog::getAllowChat() const
{
	return ui_->allowchat->isChecked();
}

}

