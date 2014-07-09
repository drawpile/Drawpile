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
#include "utils/usernamevalidator.h"
#include "utils/sessionidvalidator.h"

using widgets::ImageSelector;
using widgets::ColorButton;

#include <QSettings>

#include "ui_hostdialog.h"

namespace dialogs {

HostDialog::HostDialog(const QImage& original, QWidget *parent)
	: QDialog(parent)
{
	_ui = new Ui_HostDialog;
	_ui->setupUi(this);
	_ui->buttons->button(QDialogButtonBox::Ok)->setText(tr("Host"));
	_ui->buttons->button(QDialogButtonBox::Ok)->setDefault(true);
	_ui->username->setValidator(new UsernameValidator(this));
	_ui->vanityId->setValidator(new SessionIdValidator(this));

	if(original.isNull()) {
		_ui->imageSelector->setWidth(800);
		_ui->imageSelector->setHeight(600);
		_ui->existingpicture->setEnabled(false);
		_ui->imageSelector->chooseColor();
		_ui->solidcolor->setChecked(true);
	} else {
		_ui->imageSelector->setOriginal(original);
	}

	connect(_ui->colorButton, SIGNAL(colorChanged(QColor)), _ui->imageSelector, SLOT(setColor(QColor)));
	connect(_ui->colorButton, SIGNAL(colorChanged(QColor)), _ui->solidcolor, SLOT(click()));

	connect(_ui->selectPicture, SIGNAL(clicked()), this, SLOT(selectPicture()));
	connect(_ui->imageSelector, SIGNAL(noImageSet()), this, SLOT(newSelected()));

	// Session tab defaults
	QSettings cfg;
	cfg.beginGroup("history");
	_ui->username->setText(cfg.value("username").toString());
	_ui->sessiontitle->setText(cfg.value("sessiontitle").toString());
	_ui->remotehost->insertItems(0, cfg.value("recentremotehosts").toStringList());

	QSize lastsize = cfg.value("newsize", QSize(800, 600)).toSize();
	_ui->picturewidth->setValue(lastsize.width());
	_ui->pictureheight->setValue(lastsize.height());

	QColor lastcolor = cfg.value("newcolor").value<QColor>();
	if(lastcolor.isValid())
		_ui->colorButton->setColor(lastcolor);

	// Server tab defaults
	_ui->persistentSession->setChecked(cfg.value("persistentsession", false).toBool());
	_ui->userlimit->setValue(cfg.value("userlimit", 20).toInt());
	_ui->allowdrawing->setChecked(cfg.value("allowdrawing", true).toBool());
	_ui->layerctrllock->setChecked(cfg.value("layerctrllock", true).toBool());
	if(cfg.value("hostremote", false).toBool())
		_ui->useremote->setChecked(true);
	_ui->vanityId->setText(cfg.value("vanityid").toString());

	new MandatoryFields(this, _ui->buttons->button(QDialogButtonBox::Ok));
}

HostDialog::~HostDialog()
{
	delete _ui;
}

void HostDialog::rememberSettings() const
{
	QSettings cfg;
	cfg.beginGroup("history");

	cfg.setValue("username", getUserName());
	cfg.setValue("sessiontitle", getTitle());

	// Move current address to the top of the list
	QStringList hosts;
	const QString current = _ui->remotehost->currentText();
	int curind = _ui->remotehost->findText(current);
	if(curind!=-1)
		_ui->remotehost->removeItem(curind);
	hosts << current;
	for(int i=0;i<_ui->remotehost->count();++i)
			hosts << _ui->remotehost->itemText(i);
	cfg.setValue("recentremotehosts", hosts);

	// Remember size and background color if we created a new picture
	if(_ui->solidcolor->isChecked()) {
		cfg.setValue("newsize", QSize(_ui->picturewidth->value(), _ui->pictureheight->value()));
		cfg.setValue("newcolor", _ui->colorButton->color());
	}

	// Remember server tab settings
	cfg.setValue("hostremote", _ui->useremote->isChecked());
	cfg.setValue("persistentsession", _ui->persistentSession->isChecked());
	cfg.setValue("userlimit", _ui->userlimit->value());
	cfg.setValue("allowdrawing", _ui->allowdrawing->isChecked());
	cfg.setValue("layerctrllock", _ui->layerctrllock->isChecked());
	cfg.setValue("vanityid", _ui->vanityId->text());

}

bool HostDialog::selectPicture()
{
	// Get a list of supported formats
	QString formats = "*.ora ";
	foreach(QByteArray format, QImageReader::supportedImageFormats()) {
			formats += "*." + format + " ";
	}
	const QString filter = tr("Images (%1)").arg(formats) + ";;" + QApplication::tr("All files (*)");

	// Get the file name to open
	QSettings cfg;

	const QString file = QFileDialog::getOpenFileName(this,
					tr("Open image"),
					cfg.value("window/lastpath").toString(),
					filter
	);

	bool selected = false;
	if(!file.isEmpty()) {
		_ui->imageSelector->setImage(file);
		if(_ui->imageSelector->imageFile() == file) {
			_ui->otherpicture->click();
			selected = true;
		}

		cfg.setValue("window/lastpath", file);
	}
	return selected;
}

QString HostDialog::getRemoteAddress() const
{
	return _ui->remotehost->currentText();
}

bool HostDialog::useRemoteAddress() const
{
	return _ui->useremote->isChecked();
}

QString HostDialog::getUserName() const
{
	return _ui->username->text();
}

QString HostDialog::getTitle() const
{
	return _ui->sessiontitle->text();
}

int HostDialog::getUserLimit() const
{
	return _ui->userlimit->value();
}

QString HostDialog::getPassword() const
{
	return _ui->sessionpassword->text();
}

SessionLoader *HostDialog::getSessionLoader() const
{
	if(_ui->imageSelector->isColor()) {
		return new BlankCanvasLoader(
			QSize(
				_ui->picturewidth->value(),
				_ui->pictureheight->value()
			),
			_ui->imageSelector->color()
		);

	} else if(_ui->imageSelector->isImageFile()) {
		return new ImageCanvasLoader(_ui->imageSelector->imageFile());

	} else {
		return new QImageCanvasLoader(_ui->imageSelector->image());
	}
}

bool HostDialog::useOriginalImage() const
{
	return _ui->imageSelector->isOriginal();
}

void HostDialog::newSelected()
{
	if(!selectPicture()) {
		// Select something else if no image was selected
		if(_ui->existingpicture->isEnabled())
			_ui->existingpicture->click();
		else
			_ui->solidcolor->click();
	}
}

bool HostDialog::getAllowDrawing() const
{
	return _ui->allowdrawing->isChecked();
}

bool HostDialog::getLayerControlLock() const
{
	return _ui->layerctrllock->isChecked();
}

bool HostDialog::getPersistentMode() const
{
	return _ui->persistentSession->isChecked();
}

QString HostDialog::getSessionId() const
{
	return _ui->vanityId->text();
}

}
